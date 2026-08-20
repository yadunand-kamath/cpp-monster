# P-4.6 — Solution

## Reference Architecture

The platform-independent state machine, deliberately ignorant of OS process APIs:

```cpp
enum class SupervisorState { kNotRunning, kStarting, kRunning, kStopping, kBackoff };

class Supervisor {
public:
    explicit Supervisor(SupervisorConfig config) : config_(config) {}

    std::expected<void, LaunchError> start(std::vector<std::string> command) {
        command_ = std::move(command);
        return launch_locked();
    }

    void stop_gracefully() {
        deliberate_stop_requested_.store(true);
        if (child_) child_->request_graceful_termination();
        backoff_cv_.notify_all(); // interrupts an in-progress backoff wait — Edge Cases
    }

private:
    std::expected<void, LaunchError> launch_locked() {
        state_ = SupervisorState::kStarting;
        auto child = ChildProcess::launch(command_); // platform-specific type, Hint 1
        if (!child) return std::unexpected(LaunchError::kSpawnFailed);
        child_ = std::move(*child);
        state_ = SupervisorState::kRunning;
        monitor_thread_ = std::jthread([this](std::stop_token st) { monitor_loop(st); });
        return {};
    }

    void monitor_loop(std::stop_token st) {
        ExitResult exit = child_->wait_for_exit(); // blocks; no busy-poll, per Constraints
        if (deliberate_stop_requested_.load()) { state_ = SupervisorState::kNotRunning; return; }
        history_.record_exit(exit);
        if (exit.code == 0 && config_.treat_exit_zero_as_deliberate) { state_ = SupervisorState::kNotRunning; return; }
        schedule_restart_with_backoff(st);
    }

    void schedule_restart_with_backoff(std::stop_token st) {
        state_ = SupervisorState::kBackoff;
        auto delay = std::min(config_.base_backoff * (1u << history_.consecutive_failures), config_.max_backoff);
        history_.observed_restart_delays.push_back(delay);
        std::unique_lock lock(backoff_mutex_);
        backoff_cv_.wait_for(lock, delay, [&] { return deliberate_stop_requested_.load() || st.stop_requested(); });
        if (deliberate_stop_requested_.load() || st.stop_requested()) return;
        launch_locked();
    }

    std::unique_ptr<ChildProcess> child_;
    std::atomic<bool> deliberate_stop_requested_{false};
    std::condition_variable backoff_cv_;
    std::mutex backoff_mutex_;
    SupervisorConfig config_;
    SupervisorHistory history_;
    std::vector<std::string> command_;
    SupervisorState state_ = SupervisorState::kNotRunning;
    std::jthread monitor_thread_;
};
```

The grace-then-escalate sequence, showing Hint 3's technique inside the platform-specific type's stop path:

```cpp
void ChildProcess::terminate_gracefully_then_forcefully(std::chrono::milliseconds grace_period) {
    request_graceful_termination(); // SIGTERM, or GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT)
    if (wait_for_exit_with_timeout(grace_period)) {
        last_stop_was_forceful_ = false;
        return;
    }
    request_forceful_termination(); // SIGKILL, or TerminateProcess
    wait_for_exit_with_timeout(std::chrono::milliseconds::max()); // unbounded — a forceful kill won't hang
    last_stop_was_forceful_ = true;
}
```

## Design Rationale

**Why is the restart/backoff logic in a platform-independent `Supervisor` that merely calls into a platform-specific `ChildProcess`, rather than one combined class per platform?** The backoff policy (how long to wait, when to give up, how the deliberate-stop flag suppresses restart) is exactly the same logic on both platforms — duplicating it into two platform-specific classes would mean any policy bug fix or test has to be written and verified twice. Isolating platform difference to the narrowest possible seam (`ChildProcess`'s launch/wait/terminate primitives) is what makes the policy layer's tests (backoff growth, deliberate-stop suppression, cap enforcement) runnable without needing two full platform builds to verify the same logic.

**Why use a dedicated reader thread per output stream instead of, say, non-blocking reads polled from the monitor thread?** A non-blocking-poll approach would reintroduce exactly the busy-poll the Constraints forbid, or would require integrating output-draining into the same event loop as exit-detection, adding complexity for no benefit at this project's scope. A dedicated blocking reader thread per stream is the simplest design that guarantees the pipe is always being drained, which is the actual property needed to avoid the pipe-full deadlock — added thread count is an acceptable cost for one supervised child.

**Why does the forceful-kill wait have no timeout, when the graceful wait does?** A graceful termination request may legitimately be ignored by an uncooperative child, so a bounded wait is required to detect that and escalate. A forceful kill (`SIGKILL`, `TerminateProcess`) is a guarantee from the OS that the process will be terminated — waiting on it completing is not subject to the same "child chooses to ignore it" possibility, so an unbounded wait here is correct, not a missed timeout that should have been added.

## Reference Implementation

The above shows the state machine's core transitions and the grace-then-escalate primitive. Remaining work for the learner: `ChildProcess`'s platform-specific launch implementations (`fork`+`exec` with pipe setup and `close`-on-exec flags for the parent's ends; `CreateProcess` with `STARTUPINFO` pipe redirection), the dedicated stdout/stderr reader threads and their buffer/log destination, `SupervisorHistory`'s bookkeeping fields, the max-restart-attempts cap check, and the platform-specific graceful-signal helper test executables referenced in TESTS.md's Hidden Tests.

## Testing Strategy

Build the `test_child` helper first, before the supervisor itself — its controllable behaviors (immediate exit with code, signal-ignoring loop, output burst, sleep) are the actual test fixtures every other test in this project depends on, so getting its argument handling right early avoids having to retrofit test infrastructure mid-project.

## Performance Analysis

Not a performance-sensitive project in the usual sense (this supervises typically one to a handful of long-lived processes, not thousands of short operations); the more relevant measurement is *responsiveness* — how promptly exit is detected (bounded by the blocking-wait mechanism's own latency, effectively immediate) and how closely observed backoff delays track the configured exponential formula.

## Failure Modes

- A grace-period wait that's actually a busy-poll loop calling `waitpid(..., WNOHANG)` in a tight spin rather than blocking or sleeping between checks — technically works, but violates the no-busy-poll constraint and wastes CPU disproportionate to the task.
- Treating the deliberate-stop flag as a one-shot signal checked only at the moment `stop_gracefully()` is called, rather than a persistent flag checked by the monitor thread when it later observes the exit — since the exit notification and the stop request can arrive in either order, the flag must be checked at exit-observation time, not just at request time.
- Backoff computed with unsigned overflow-prone shifting (`1u << consecutive_failures` for a large failure count) — must be clamped before the shift grows unreasonably large, not merely capped after the fact once it may have already overflowed.

## Extensions

- Multiple simultaneously supervised child processes under one supervisor instance, with independent backoff state per child.
- A structured log/metrics feed of the supervisor's own state transitions, suitable for feeding into an external monitoring system.
