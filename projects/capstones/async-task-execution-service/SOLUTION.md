# C-2 — Solution

No single canonical solution — the scheduling, cancellation-propagation, and backpressure mechanisms are open design choices. This sketches one credible reference path at design-and-key-snippets depth.

## Reference Architecture

### Chosen design: `task<T>` (from P-5.2) + cancellation-token-carrying awaiters + work-stealing pool as the resumption substrate

```cpp
class TaskScope {
public:
    explicit TaskScope(TaskExecutor& exec) : exec_(exec) {}

    template <typename Awaitable>
    void spawn(Awaitable&& child) {
        std::lock_guard lock(mutex_);
        auto handle = std::make_shared<ChildHandle>();
        children_.push_back(handle);
        exec_.schedule(run_tracked(std::move(child), token_, handle)); // Hint 1: every spawn registers into this scope
    }

    void cancel() { source_.request_stop(); }        // cooperative — Hint 2
    CancellationToken token() const { return source_.get_token(); }

    void join() {                                      // blocks until every registered child has finished unwinding
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return active_count_ == 0; });
    }

    ~TaskScope() { cancel(); join(); }                  // Hint 1: no child outlives the scope

private:
    task<void> run_tracked(auto child, CancellationToken tok, std::shared_ptr<ChildHandle> handle) {
        struct Decrement { TaskScope* s; ~Decrement() { s->on_child_finished(); } } dec{this};
        co_await std::move(child);
    }
    void on_child_finished() {
        std::lock_guard lock(mutex_);
        if (--active_count_ == 0) cv_.notify_all();
    }

    TaskExecutor& exec_;
    std::stop_source source_;
    std::mutex mutex_;
    std::condition_variable cv_;
    int active_count_ = 0;
    std::vector<std::shared_ptr<ChildHandle>> children_;
};
```

Cancellation-aware awaiter, checked at the one place all suspension funnels through:

```cpp
struct CancellableAwaiter {
    CancellationToken token;
    bool await_ready() const { return token.stop_requested(); } // already cancelled: resume immediately into the throw path
    void await_suspend(std::coroutine_handle<> h) { register_wake_on_cancel_or_signal(token, h); }
    void await_resume() const {
        if (token.stop_requested()) throw TaskCancelledException{}; // Hint 2: unwind via ordinary exception machinery
    }
};
```

Scheduling a resumed coroutine handle as ordinary work-stealing-pool work (Hint 3):

```cpp
struct WorkerQueueAwaiter {
    WorkStealingPool& pool;
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) const { pool.push_local(h); } // resumption = just another queued job
    void await_resume() const {}
};
```

## Design Rationale

**Why cooperative cancellation via a checked token rather than any form of forced termination?** Forcibly terminating a thread mid-instruction cannot guarantee RAII destructors run, and cannot guarantee any invariant of shared state the task was mutating is left consistent — it is not a sound primitive to build correctness on. Cooperative cancellation, checked at await points and translated into an ordinary C++ exception, reuses the language's own unwind-and-destruct guarantees; the correctness argument for "cleanup always runs" reduces to the correctness argument C++ already gives you for `throw`.

**Why does every `spawn` register into the enclosing `TaskScope` rather than tasks being freely spawnable?** The entire value of structured concurrency is the invariant "no task's lifetime is untracked by some enclosing scope." If a task could be spawned without registering anywhere, `TaskScope::join()` would have no way to know it needs to wait for that task, and `cancel()` would have no way to reach it — exactly the orphaned-background-work failure this design is meant to make structurally impossible, per Hint 4.

**Why route resumed coroutine handles through the same work-stealing queue as fresh task submissions, rather than a separate "resumption queue"?** A resumed continuation and a freshly spawned task are, from the scheduler's point of view, the same kind of thing: a coroutine handle that is ready to run. Giving them one unified representation means the work-stealing pool's load-balancing logic (Phase 3's fairness requirement) applies uniformly to both, rather than needing two separate fairness policies that could fight each other.

## Reference Implementation

Left to the learner: `TaskExecutor`'s integration of the event loop (for I/O-bound awaiters like `io_sleep`) with the work-stealing pool (for CPU-bound work), including the specific mechanism by which an I/O-ready event wakes a suspended coroutine and hands its resumption back to the pool; the backpressure policy's concrete implementation (bounded queue + `try_spawn` returning failure, versus blocking `spawn`, versus some hybrid — the STATEMENT.md requires this be an explicit, justified, tested choice, not left implicit); the `DeterministicTestScheduler`'s scripted-or-seeded-interleaving driver; and the observability surface (in-flight count, queue depth, per-task-type latency histograms) for Phase 5.

## Testing Strategy

Build and correctness-test the cancellation-aware awaiter (`CancellableAwaiter` above) as an independent unit first, against a fake/manual scope, before wiring it into the full scheduler — this mirrors the workbook's repeated pattern (P-5.3's `FrameParser`, P-5.4's crash-injection steps) of isolating a state machine from its integration context so its correctness can be verified directly. The deterministic test scheduler (Phase 3) should then be built specifically to reproduce Phase 2's cancellation-race edge cases (cancel-during-suspend-to-resume transition) on demand, rather than relying on getting lucky with real thread scheduling to hit that window.

## Performance Analysis

Per-task scheduling overhead in a coroutine-based scheduler is dominated by: coroutine frame allocation cost (mitigate via P-5.2's arena-backed promise-type allocator), the cost of the resumption hand-off itself (a lock-free work-stealing queue push/pop is far cheaper than a mutex-guarded queue under contention), and — for cancellation-aware designs — the cost of the stop-token check at every await point (usually negligible, but worth measuring if await points are extremely hot). Phase 4's report should isolate and measure at least one of these three specifically, with a before/after.

## Failure Modes

- A `TaskScope` destructor that cancels but doesn't join before returning — the scope object is gone, but its children are still unwinding, and any resource the scope itself owned (e.g. a token source) is now a dangling reference from the children's perspective. The destructor must cancel *and block until every child has actually finished*, as shown above.
- Registering a spawned child into the parent scope's bookkeeping *after* the child coroutine has already started running (rather than before), creating a window where a fast-completing child finishes and reports "done" before it was ever counted as "started" — an off-by-one in the active-task accounting that manifests as `join()` returning too early, missing that task's cleanup.
- A backpressure policy that silently drops rejected work rather than surfacing rejection to the caller — this violates the "no work is silently dropped" spirit even if it's a deliberate backpressure decision; rejection must be an observable, distinct outcome from "accepted and completed."

## Extensions

- Task priorities, with the work-stealing pool's local-queue-ordering aware of priority, layered onto the fairness-focused unified-queue design above.
- A distributed variant, where task submission and cancellation propagate across a network boundary rather than staying in-process — a substantial redesign, since cooperative cancellation's "checked at await points" assumption gets much harder to reason about once "the task" isn't a single process's coroutine.
