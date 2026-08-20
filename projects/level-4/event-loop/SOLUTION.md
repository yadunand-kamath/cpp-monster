# P-4.1 — Solution

## Reference Architecture

```cpp
using TimerHandle = std::uint64_t;

class EventLoop {
public:
    void watch_readable(int fd, std::function<void()> on_ready);
    void watch_writable(int fd, std::function<void()> on_ready);
    TimerHandle schedule_timer(std::chrono::milliseconds delay, std::function<void()> cb);
    void cancel(TimerHandle h);
    void run();
    void stop();
private:
    std::unique_ptr<Backend> backend_; // EpollBackend or IocpBackend
    std::multimap<std::chrono::steady_clock::time_point, TimerEntry> timers_;
    std::unordered_set<TimerHandle> cancelled_;
    bool stop_requested_ = false;
};
```

The core loop, showing the shared timer/wait-timeout unification from Hint 2:

```cpp
void EventLoop::run() {
    while (!stop_requested_) {
        std::optional<std::chrono::milliseconds> timeout;
        if (!timers_.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto next = timers_.begin()->first;
            timeout = next > now
                ? std::chrono::duration_cast<std::chrono::milliseconds>(next - now)
                : std::chrono::milliseconds{0};
        }
        backend_->wait_and_dispatch(timeout); // invokes ready I/O callbacks directly

        auto now = std::chrono::steady_clock::now();
        while (!timers_.empty() && timers_.begin()->first <= now) {
            auto it = timers_.begin();
            TimerEntry entry = std::move(it->second);
            timers_.erase(it);
            if (!cancelled_.erase(entry.handle)) entry.callback(); // skip if cancelled
        }
    }
    stop_requested_ = false; // allow run() to be called again after a clean stop
}
```

`EpollBackend::wait_and_dispatch` calls `epoll_wait` with the given timeout (or -1 for "no timers, wait indefinitely"), then for each ready fd invokes its registered callback directly — the callback itself performs the actual `read`/`write`/`accept` (epoll only reports readiness, not results).

`IocpBackend::wait_and_dispatch` calls `GetQueuedCompletionStatus` with the given timeout, and on a successful dequeue looks up the completion's associated operation via its `OVERLAPPED`-embedding structure, checks a cancellation flag on that structure, and invokes the callback with the already-available result only if not cancelled — the key structural difference from epoll being that the actual I/O (`ReadFile`/`WriteFile` with an `OVERLAPPED*`) was already submitted *before* this wait, not performed by the callback in response to it.

## Design Rationale

**Why does `watch_readable`'s callback perform the actual read on Linux, while IOCP's callback receives an already-completed result?** This isn't a stylistic choice — it's the fundamental difference between the two models that the public API is deliberately designed to accommodate rather than paper over. Epoll's `EPOLLIN` tells you "a read would not block right now," and you still have to issue it; IOCP's completion tells you "the read you already issued is done, here is what it returned." Trying to force one shape onto the other (e.g. making epoll's callback also just "receive a result" would require the loop to perform a synthetic read on the callback's behalf, adding an unnecessary layer) is why Hint 1 recommends designing the *contract*, not the mechanism, as the shared piece.

**Why a cancellation flag checked at dequeue time rather than attempting to cancel the OS-level operation directly (e.g. via `CancelIoEx`)?** `CancelIoEx` can be used and is a reasonable addition, but it does not eliminate the fundamental race: an operation can complete and have its completion already sitting in the IOCP queue before the cancellation request is even processed by the OS. The only way to guarantee the *observable* contract ("a cancelled registration's callback never fires") is to make that guarantee at the layer you fully control — your own bookkeeping — rather than relying on winning a race with the OS.

**Why re-check all due timers in a `while` loop after the wait returns, rather than firing only the single earliest one?** Multiple timers can become due during the same wait (especially if the loop was busy processing I/O for a while, or after resuming from a debugger pause) — firing only one per iteration would let a backlog of overdue timers silently accumulate delay iteration by iteration instead of being caught up promptly.

## Reference Implementation

The above covers the shared timer/wait unification and the cancellation-flag mechanism for the hardest platform-specific race. Remaining work for the learner: the full `EpollBackend` (fd registration/modification/removal via `epoll_ctl`, dispatch-on-ready-fd loop), the full `IocpBackend` (associating fds/handles with the completion port via `CreateIoCompletionPort`, submitting overlapped reads/writes, the per-operation cancellation-flag structure embedding an `OVERLAPPED`), and `stop()`'s exact interruption mechanism for a currently-blocked wait (an `eventfd`/self-pipe on Linux registered alongside watched fds; posting a completion with a distinguished key on Windows).

## Testing Strategy

Use real loopback TCP sockets for the I/O tests rather than mocking the backend — the entire point under test is genuine OS-level readiness/completion behavior, which is exactly what a mock would need to assume correctly in advance.

## Performance Analysis

Both backends dispatch in O(ready events) per wait call, not O(registered fds) — this is the entire reason `epoll`/IOCP exist over older O(n) mechanisms like `select`. Timer management is O(log n) per schedule/cancel via the ordered `multimap`.

## Failure Modes

- A wait-timeout computation that can evaluate to a zero or negative duration due to rounding, silently degrading the loop into a busy-poll despite using a nominally-blocking wait call.
- Forgetting that epoll's callback must still perform the actual I/O operation (readiness, not completion), while IOCP's callback receives an already-finished result — conflating the two leads to either double-reads on Linux or attempting to read data on Windows that's already been delivered.
- Not re-checking for further due timers after firing one, allowing a backlog to silently grow under sustained I/O load.

## Extensions

- A thread-safe cross-thread "wake and run this callback on the loop's thread" primitive, bridging toward multi-loop designs.
- Integrating with [P-5.2](../../level-5/coroutine-task-generator/STATEMENT.md)'s coroutine `task<T>` so `await`-ing an I/O operation suspends into this loop rather than requiring explicit callbacks.
