# C-2 — Progressive Hints

Use these in order. As with C-1, these hints are spaced at the architecture-decision level, not the implementation level.

## Hint 1 — Direction

Draw the ownership/lifetime tree you intend to build *before* writing scheduler code: which object owns a `TaskScope`, which object owns the worker threads, and — critically — what happens to a `TaskScope`'s children when the scope itself is destroyed while children are still running. If you can't yet answer that last question precisely, structured concurrency isn't designed yet; get that answer first, since Phase 2's entire correctness argument rests on it.

## Hint 2 — Technique

Don't build cancellation as a side channel bolted onto an already-working scheduler — thread a `CancellationToken` (or a `std::stop_token`-shaped equivalent) through every awaiter's `await_suspend`/`await_resume` from the start, even during Phase 1, so that by the time Phase 2 asks for cancellation, every await point in the codebase already has the hook it needs to check "was I cancelled?" and translate that into a thrown exception rather than a normal resume — retrofitting this into Phase 1 code written without it in mind is a common source of Phase 2 schedule slippage.

## Hint 3 — Implementation

The work-stealing pool from [P-4.3](../../level-4/work-stealing-thread-pool/STATEMENT.md) already solves "many worker threads, each with a local queue, stealing work from each other when idle" — the new problem this capstone adds on top is *what gets pushed into those queues*: not raw function objects, but resumable coroutine handles. A `task<T>`'s `await_suspend` that would normally just suspend can instead push its continuation handle onto the scheduler's work queue and return, so that "resume this coroutine" becomes just another schedulable unit of work indistinguishable from a fresh task submission.

## Hint 4 — Debugging/Design

If your deterministic test scheduler "mostly" reproduces the same interleaving but occasionally diverges, look for any place where real wall-clock time, real OS thread scheduling, or a real system call (rather than your test scheduler's own controlled notion of time and readiness) leaks into a decision about what runs next — a deterministic scheduler is only as deterministic as its most impatient dependency; one real `sleep()` or one real socket read anywhere in the path under test will reintroduce exactly the flakiness this piece of infrastructure exists to eliminate.
