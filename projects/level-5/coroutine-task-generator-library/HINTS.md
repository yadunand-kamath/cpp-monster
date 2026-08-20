# P-5.2 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch13's coroutine material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Build in this order: `generator<T>` first (synchronous, simplest lifecycle), then a minimal `task<T>` that can only be awaited once and never chained (get the promise type and basic suspend/resume right without worrying about symmetric transfer yet), then upgrade that `task<T>` to properly implement symmetric transfer once you understand why the naive version would stack-overflow, and only then attempt `when_all` and the custom allocator — each stage depends on genuinely understanding the previous one, not just copying a pattern.

## Hint 2 — Technique

The key insight for symmetric transfer: `await_suspend` is allowed to return a `std::coroutine_handle<>` instead of `void`/`bool`, and when it does, the coroutine machinery performs a guaranteed tail call to resume that handle — no new stack frame is pushed for the resumption. For a `task<T>` awaiter, this means: when task B is awaited by task A, and B's `await_suspend` needs to eventually resume A once B completes, the mechanism for "resume A" must ultimately be a *returned handle* from some `await_suspend`, not a direct `.resume()` call anywhere in the chain.

## Hint 3 — Implementation

`task<T>`'s promise type's `final_suspend()` should return a custom awaiter type whose `await_suspend` checks whether a "continuation" handle was previously registered (by whoever awaited this task) and, if so, *returns* that continuation handle (achieving the tail-call resumption) rather than calling `.resume()` on it. The `task<T>`'s own awaiter (the thing returned by `task<T>::operator co_await()`) is what registers that continuation handle into the promise, inside its own `await_suspend`, before suspending the awaiting coroutine.

## Hint 4 — Debugging/Design

If the custom-allocator routing test shows frame allocations still going through the global allocator, remember that a promise type's frame allocation is customized by giving the promise type itself a class-scope `operator new`/`operator delete` (optionally taking extra arguments matched against the coroutine function's parameters, allowing an allocator instance to be threaded through) — simply having an `ArenaAllocator` instance in scope near the coroutine does nothing on its own; the promise type must explicitly opt into using it via that operator new overload resolution mechanism.
