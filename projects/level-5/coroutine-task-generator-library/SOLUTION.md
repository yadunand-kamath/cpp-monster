# P-5.2 — Solution

## Reference Architecture

`task<T>`'s promise type, showing the continuation-handle mechanism (Hint 3) that makes symmetric transfer possible:

```cpp
template <typename T>
class task {
public:
    struct promise_type {
        std::variant<std::monostate, T, std::exception_ptr> result_;
        std::coroutine_handle<> continuation_; // who to resume when this task completes

        task get_return_object() { return task{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() { return {}; } // lazy start
        void return_value(T v) { result_ = std::move(v); }
        void unhandled_exception() { result_ = std::current_exception(); }

        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                auto& promise = h.promise();
                return promise.continuation_ ? promise.continuation_ : std::noop_coroutine(); // Hint 2's tail call
            }
            void await_resume() noexcept {}
        };
        FinalAwaiter final_suspend() noexcept { return {}; }
    };

    struct Awaiter {
        handle_type handle_;
        bool await_ready() { return handle_.done(); } // synchronous-completion fast path — Hint 3
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) {
            handle_.promise().continuation_ = awaiting; // register continuation before resuming ourselves
            return handle_; // symmetric transfer into this task's coroutine
        }
        T await_resume() {
            auto& result = handle_.promise().result_;
            if (std::holds_alternative<std::exception_ptr>(result))
                std::rethrow_exception(std::get<std::exception_ptr>(result));
            return std::move(std::get<T>(result));
        }
    };
    Awaiter operator co_await() { return Awaiter{handle_}; }

private:
    using handle_type = std::coroutine_handle<promise_type>;
    explicit task(handle_type h) : handle_(h) {}
    handle_type handle_;
};
```

The promise-type-scoped `operator new` for the custom frame allocator (Hint 4):

```cpp
struct AllocatorAwarePromise {
    static void* operator new(std::size_t size, ArenaAllocator& arena, auto&&...) {
        return arena.allocate(size, alignof(std::max_align_t)); // frame routed through the arena, not global new
    }
    static void operator delete(void*, std::size_t) {} // arena reclaims via reset(), not per-frame delete
    // ... rest of promise_type as above
};
```

## Design Rationale

**Why must `final_suspend`'s awaiter return the continuation handle from `await_suspend` rather than calling `continuation_.resume()` directly?** Calling `.resume()` directly pushes a new stack frame for that resumption on top of the current one (which itself may be nested many levels deep in a chain). Returning the handle instead lets the coroutine machinery perform a guaranteed tail call — the current frame's stack space is released before the next coroutine resumes, which is exactly what makes a 100,000-deep chain use O(1) stack space instead of O(n).

**Why does the `task<T>` awaiter's `await_ready()` check `handle_.done()` rather than unconditionally suspending?** Since `task<T>` uses lazy (`suspend_always`) initial suspend, a task that completes essentially instantly (e.g. `co_return 42;` with no real suspension inside) will have already run to completion the moment it's resumed for the first time — if that resumption happens to occur before the awaiter is even constructed in some flows, or synchronously within the awaiting coroutine's flow, forcing a full suspend/resume round-trip anyway would be pure overhead for no benefit; checking `done()` lets the fast path skip that unnecessary suspension.

**Why is the arena's `operator delete` for `AllocatorAwarePromise` a no-op rather than actually freeing the frame?** This mirrors [P-4.4](../../level-4/arena-pool-allocators/SOLUTION.md)'s arena design directly: the arena's whole model is that individual frees are absorbed as no-ops, with real reclamation happening only via a bulk `reset()`. A coroutine frame allocated from the arena is subject to the same tradeoff — this is a deliberate, documented choice suited to workloads where many short-lived coroutine frames are created and destroyed together and then the whole arena is reset at once, not a general-purpose replacement for per-frame deallocation.

## Reference Implementation

The above shows `task<T>`'s core promise/awaiter mechanics with symmetric transfer, and the frame-allocator opt-in mechanism. Remaining work for the learner: `generator<T>`'s promise type and iterator adapter (including correct exception propagation at increment-time and clean teardown on early abandonment), `when_all`'s implementation (typically an awaiter that registers itself as the continuation for each constituent task and only actually resumes the caller once an atomic completion counter reaches the task count — preserving Design Rationale's out-of-order-safe result association by indexing results positionally, not by completion order), `sync_wait` (a small driver that runs a task to completion on the calling thread, used to bridge coroutine code into synchronous test code), and `task<void>`'s specialization.

## Testing Strategy

Write the deep-chain test early, even before `when_all` or the allocator work, and run it under a debug build with a deliberately small stack size configured — this makes a broken (non-symmetric-transfer) implementation fail loudly and immediately rather than only failing under the full 100,000-depth test at the end, shortening the feedback loop while implementing the trickiest part of this project.

## Performance Analysis

A well-implemented `task<T>` chain's per-hop cost is dominated by a coroutine-frame resume/suspend pair (comparable to a function call) rather than by any stack growth — the deep-chain test's success criterion is really a stack-*space* claim (O(1) regardless of depth), not primarily a claim about wall-clock time per hop, though both should be measured.

## Failure Modes

- A `final_suspend` awaiter that calls `.resume()` on the continuation directly instead of returning it — passes shallow tests, silently stack-overflows only once chain depth crosses some threshold dependent on available stack size and per-frame size, making this bug easy to miss without a dedicated deep-chain test.
- Eagerly starting a `task<T>`'s coroutine at creation time (`suspend_never` for `initial_suspend`) rather than lazily — changes the fast-path/ordering semantics in ways that can silently break `when_all`'s "start all, then wait for all" model if not accounted for.
- An `operator new`/`operator delete` pair on the promise type where `operator new` routes through the arena but `operator delete` still calls the global `delete`, or vice versa — a mismatched pair that can corrupt the arena's or the global allocator's internal state.

## Extensions

- A `cancellation_token`-aware `task<T>` integrating with `std::stop_token`, allowing a running task chain to be cooperatively cancelled mid-flight.
- Integrating `task<T>` with [P-4.1](../../level-4/event-loop/STATEMENT.md)'s event loop so `co_await` on an I/O-readiness awaiter genuinely suspends until the event loop signals readiness, rather than this project's synchronous-only scope.
