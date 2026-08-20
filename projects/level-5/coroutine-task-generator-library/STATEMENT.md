# P-5.2 — Coroutine Task & Generator Library

**Level:** 5 (Production-style) · **Category:** Libraries · **Requires:** Ch01–13 · **Est. effort:** XL (24-32h)

## Objective

Build a small but real coroutine library on top of C++20's raw `co_await`/`co_yield`/`co_return` machinery: an eagerly-or-lazily-started `task<T>` for single-value async results, a `generator<T>` for lazy synchronous sequences, a `when_all` combinator, and a custom frame allocator — understanding symmetric transfer well enough to avoid stack overflow on deeply chained coroutines.

This project is structured in phases. Each phase has its own exit bar; don't move on until the current one is met.

## Phase 1 — `task<T>` Core

Get a single, non-chained `task<T>` working: awaitable from another coroutine, correctly returning its result, correctly propagating an exception thrown inside its body to the awaiter. Exit bar: a test awaits a `task<int>` and a `task<void>` and gets the right result/exception in both the success and throwing case.

## Phase 2 — `generator<T>`

Build the lazy synchronous generator, consumable via range-based for. Exit bar: a generator producing an infinite or very large sequence is consumed lazily (proven by observing side effects only happen as each value is pulled, not eagerly up front) and correctly stops at a `co_return`.

## Phase 3 — Symmetric Transfer

Make chained `task<T>` awaits not grow the call stack. Exit bar: a 100,000-deep chain of awaited tasks runs to completion without stack overflow, and you can point to the specific mechanism (returning a coroutine handle from `await_suspend` rather than resuming inline) that makes this true — verify with a stack-depth-instrumented build or a deliberately naive (non-symmetric-transfer) version shown to overflow at a much shallower depth.

## Phase 4 — `when_all` and Custom Frame Allocation

Add the multi-task combinator and redirect coroutine frame allocation to a custom allocator (e.g. P-4.4's arena). Exit bar: `when_all` over tasks of different `T`s correctly waits for all and yields a tuple of results; a frame-allocation counter proves coroutine frames are actually being allocated through the custom allocator, not the global one.

## Phase 5 — Hardening

Confirm exception propagation and resource cleanup hold up under adversarial cases: a task destroyed before completion, an exception thrown from deep in a symmetric-transfer chain, `when_all` where one of several tasks throws. Exit bar: each adversarial case has a passing test with no leaked coroutine frames (verified under ASan) and no unhandled-exception termination.

## Functional Requirements

1. `task<T>`: an awaitable type wrapping a coroutine that produces a single `T` (or `void`) result, awaitable from another coroutine (`co_await some_task()`), propagating exceptions thrown inside the coroutine to the awaiter.
2. `generator<T>`: a lazily-evaluated synchronous range of `T` values produced via `co_yield`, consumable via a range-based for loop (i.e. it must satisfy enough of the iterator/range interface for `for (auto v : my_generator())` to work).
3. `when_all`: given multiple `task<T>`s (possibly of different `T`s), returns a single awaitable that completes when all of them have completed, yielding their results together (e.g. as a `std::tuple`).
4. Symmetric transfer for `task<T>` chains: a `task<T>` that awaits another `task<T>` must not grow the call stack per chained `co_await` — a deeply chained sequence of awaited tasks (e.g. 100,000 deep) must not stack-overflow, demonstrating actual symmetric transfer, not merely a recursive await that happens to work for shallow chains.
5. A custom allocator for coroutine frames (coroutine frame allocation is customizable via a `get_return_object`/promise-type mechanism or an allocator-aware `operator new` on the promise type) — demonstrating that coroutine frame allocation is not implicitly tied to the global allocator and can be redirected, e.g. to an arena from [P-4.4](../../level-4/arena-pool-allocators/STATEMENT.md).
6. Correct exception propagation: an exception thrown inside a `task<T>`'s coroutine body is rethrown at the `co_await` point in the awaiter, not silently swallowed or terminating the program via an unhandled-exception-in-coroutine path.

## Input

Coroutine bodies written by the library's users using `co_await`/`co_yield`/`co_return` against this library's `task<T>`/`generator<T>` types.

## Output

Computed results (via `task<T>`'s eventual value, retrieved by awaiting it), yielded sequences (via `generator<T>`'s iteration), and combined results (via `when_all`).

## Constraints

- C++20 coroutines (`<coroutine>`), no third-party coroutine library (e.g. no cppcoro) — the promise types, awaiters, and coroutine handle management must be written by you, since that's the actual content being learned.
- The symmetric-transfer requirement (#4) must be demonstrated, not assumed — a deep-chain test that would stack-overflow a naive `await_suspend` implementation is part of Acceptance Criteria, not optional.
- Coroutine frames must be correctly destroyed exactly once regardless of completion path (normal completion, exception, or the owning `task<T>` being destroyed before completion) — no double-destroy, no leak.

## Edge Cases

- A `task<T>` that is created but never awaited and never has its result observed — must not leak its coroutine frame, and must not crash on destruction.
- A `task<T>` whose coroutine completes synchronously before `co_await` even suspends the awaiting coroutine (the "coroutine already finished by the time we tried to await it" fast path) — must be handled correctly and efficiently, not always forced through a full suspend/resume round-trip.
- `when_all` given tasks that complete in a different order than they were passed in — the combined result must still correctly correspond each result to its originating task, in the original argument order.
- A `generator<T>` whose iteration is abandoned partway through (the range-for loop is broken out of, or the generator object is destroyed mid-iteration) — the coroutine frame must be cleanly torn down at that point, not run to completion or leaked.

## Error Handling

- An exception thrown inside a `generator<T>`'s coroutine body during iteration — propagated to the iteration point (e.g. rethrown when the iterator is incremented), not silently ending iteration as if it were a normal completion.
- Destroying a `task<T>` whose coroutine is still suspended mid-execution (not yet completed) — must cleanly cancel/destroy the coroutine frame without undefined behavior, documented explicitly since this is a common source of coroutine-library bugs.

## Acceptance Criteria

- The deep-chain symmetric-transfer test (100,000+ deep chained `task<T>` awaits) completes without stack overflow.
- Exception propagation through both `task<T>` (rethrow at await point) and `generator<T>` (rethrow at iteration point) is demonstrated.
- `when_all` correctly associates out-of-order-completing tasks' results with their original argument positions.
- The custom frame allocator is demonstrated actually routing coroutine frame allocations through it (not the global allocator) — verifiable via instrumentation counting allocations through each path.
- `generator<T>` correctly supports early abandonment (partial iteration) without leaking or misbehaving.

## Testing Requirements

- Basic correctness: a `task<T>` returning a computed value, awaited and its value observed correctly.
- The synchronous-completion fast-path test (task completes before the awaiter suspends).
- The deep-chain stack-overflow-avoidance test (this is the project's signature test — must be present and must actually exercise a chain deep enough that a naive implementation would fail it).
- Exception propagation tests for both `task<T>` and `generator<T>`.
- The `when_all` out-of-order-completion correctness test.
- The custom allocator routing-verification test.
- The early-abandonment-of-a-generator test.

## Hints

### Hint 1 — Direction
Before attempting `task<T>`, build `generator<T>` first — it's the simpler of the two (synchronous, no actual suspension across an async boundary, just "produce a value, pause, resume on demand") and will force you to get comfortable with promise types, `co_yield`, and the coroutine handle lifecycle in a setting where mistakes are easier to observe and debug than in `task<T>`'s async-chaining scenario.

### Hint 2 — Technique
Symmetric transfer means `await_suspend` returns a `std::coroutine_handle<>` (rather than `void` or `bool`) — that returned handle is resumed via a tail call by the coroutine machinery itself, rather than your code calling `.resume()` and thereby adding a stack frame. Getting a chain of `task<T>`s to not grow the stack means the `task<T>` awaiter's `await_suspend`, when the awaited task completes, must return the *awaiting* coroutine's handle (to resume it) rather than calling `handle.resume()` directly from inside `await_suspend`.

### Hint 3 — Implementation
For a `task<T>` whose result may need to be observed by a coroutine that hasn't started yet, or by one that's already finished by the time it's awaited, the promise type typically stores both the eventual result (or exception) and a `std::coroutine_handle<>` for "who to resume when I'm done" — set by the awaiter at suspend time, and checked/resumed in `final_suspend`'s awaiter. Get the synchronous-fast-path case right by having your `await_ready()` check whether the task's coroutine has already reached its final state before deciding whether to suspend at all.

### Hint 4 — Debugging/Design
If your deep-chain test stack-overflows despite believing you implemented symmetric transfer, check every `await_suspend` in the chain, not just the outermost one — a single link in the chain that calls `.resume()` directly (even one that looks like it should be harmless, e.g. inside `final_suspend`) reintroduces a stack frame at that point, and a long enough chain will still overflow even if only one link in a hundred thousand does this; symmetric transfer has to be maintained end-to-end, not just mostly.
