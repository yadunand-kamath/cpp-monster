# P-4.3 — Work-Stealing Thread Pool

**Level:** 4 (Systems component) · **Category:** Systems · **Requires:** Ch01–11 · **Est. effort:** L (16-24h)

## Objective

Build a thread pool where each worker thread owns a local task deque, pushing/popping its own tasks from one end while idle workers steal from the *other* end of a busy worker's deque — the classic work-stealing design — with cooperative cancellation via `std::stop_token`, correct exception capture per submitted task, and graceful drain-and-shutdown.

## Functional Requirements

1. Each worker thread has its own local deque of pending tasks; a worker pushes and pops from one end (its own "top," LIFO for cache-locality reasons) while other idle workers steal from the opposite end ("bottom," FIFO-ish, reducing contention with the owner and stealing older, likely-larger-grained work first).
2. `submit(callable) -> std::future<R>` (or an equivalent result-carrying handle) for arbitrary callables, including ones returning `void` and ones that throw — a thrown exception must be captured and rethrown to whoever observes the result, not silently swallowed or left to terminate the program.
3. Cooperative cancellation via `std::stop_token`: a long-running submitted task can poll a `stop_token` it's given (or the pool exposes one shared token) to voluntarily exit early; the pool does not forcibly kill threads.
4. A configurable shutdown mode: `shutdown(drain_pending)` where draining completes all currently-queued tasks before stopping workers, versus an immediate mode that requests cooperative cancellation and does not wait for not-yet-started queued tasks.
5. New tasks submitted after `shutdown()` has been called must be rejected clearly (not silently dropped, not crashing).
6. The stealing mechanism itself must be demonstrated to actually work — an artificially unbalanced workload (one thread submits many tasks, forcing other idle workers to steal rather than each worker only ever running self-submitted work) must show measurable load being redistributed, not just a correct-but-accidentally-single-threaded execution pattern.

## Input

Callables submitted via `submit()`, from any thread (including from within a running task itself, i.e. task-spawns-subtasks is supported).

## Output

A `std::future<R>` (or equivalent) per submitted task resolving to its return value or captured exception.

## Constraints

- C++20, `std::stop_token`/`std::jthread` for cancellation and worker lifetime management.
- The local-deque-plus-stealing structure is required — a single shared queue with a mutex is explicitly not this project's design (that's closer to a simpler thread-pool exercise; work-stealing's whole point is minimizing contention on a shared structure, which a single shared queue defeats).
- Must be race-free under ThreadSanitizer across a stress test that specifically exercises concurrent stealing (not just concurrent submission from one thread).

## Edge Cases

- A task that itself calls `submit()` to spawn subtasks (recursive task decomposition, e.g. a parallel divide-and-conquer algorithm) — must work correctly without deadlocking, including when the spawning task then waits on the subtasks' futures.
- Every worker idle simultaneously with an empty pool (no pending tasks anywhere) — workers must not busy-spin burning CPU while waiting for new submissions.
- A steal attempt racing exactly against the deque owner's own pop of the same (last remaining) task — only one of the two must "win" that task; it must never be run twice or lost.
- `shutdown(drain_pending=true)` called while new tasks are still being actively submitted by another thread — document and enforce the exact cutover point (e.g. `shutdown()` first stops accepting new submissions, then drains whatever was already queued at that moment).

## Error Handling

- A task that throws — captured into its `future`, rethrown on `.get()`, and never causes the worker thread itself to terminate or the pool to become corrupted.
- `submit()` called after `shutdown()` — a clear rejection (thrown exception or an error-carrying return), consistently documented.

## Acceptance Criteria

- A demonstration of measurable work redistribution via stealing under an artificially unbalanced submission pattern (e.g. one thread submits 10,000 tiny tasks; per-worker task-execution counts are logged and shown to be roughly balanced across workers rather than concentrated on the submitting thread's own worker).
- The recursive-subtask-spawning case (e.g. a parallel quicksort or parallel Fibonacci) runs correctly and without deadlock.
- The empty-pool idle case shows near-zero CPU usage (measured), not busy-spinning.
- ThreadSanitizer-clean run of a stress test specifically targeting the steal-vs-owner-pop race on nearly-empty deques.
- Both shutdown modes demonstrated explicitly with their documented, distinct behavior.

## Testing Requirements

- Basic submit/get-result correctness, including a `void`-returning task and a task that throws.
- The stop_token cooperative-cancellation test (a task that checks the token and exits early once requested).
- The recursive-subtask-spawning correctness test.
- The steal-vs-owner-pop race stress test under ThreadSanitizer.
- Both shutdown-mode tests, including the "new submission during drain" cutover behavior.
- The idle-CPU-usage measurement.

## Hints

### Hint 1 — Direction
Start by building the single-worker case correctly (one thread, one local deque, no stealing at all) and get task submission, result futures, and exception capture fully working there — this isolates "does my task/future/exception-propagation machinery work" from "does my work-stealing algorithm work," which is a much harder question you don't want to debug simultaneously with the first.

### Hint 2 — Technique
The classic work-stealing deque (originally from the Chase-Lev paper, widely reimplemented since) has the owning thread treat one end as a stack (push/pop from the "top," cheap and usually uncontended since only the owner touches it) while thieves steal from the "bottom" (the oldest, and typically largest-grained, task) — this asymmetry is deliberate: it minimizes contention between the owner (who touches the top far more often) and thieves (who only occasionally need the bottom), and it tends to steal coarser-grained work, which amortizes stealing's overhead better than stealing the owner's most recently (and often most finely) decomposed work.

### Hint 3 — Implementation
For the steal-vs-owner-pop race on a deque's last remaining task, a compare-exchange on a shared "bottom" index (or an atomic tag per slot, depending on your chosen deque design) is what arbitrates the race — design it so that exactly one of "owner pops it" or "thief steals it" succeeds, and the loser observes the deque as empty rather than getting a stale or duplicate task. This is structurally similar to [P-4.2](../bounded-mpmc-queue/STATEMENT.md)'s slot-claiming logic, and reusing that same care around memory ordering is directly applicable here.

### Hint 4 — Debugging/Design
If your recursive-subtask-spawning test deadlocks, check whether a worker thread that submitted subtasks and is now waiting (via `.get()`) on their futures is doing so by blocking without also being willing to execute other pending work (including stolen work, or even the very subtasks it's waiting on) while it waits — a worker that only ever blocks-and-does-nothing while waiting on a future can deadlock the whole pool if every worker ends up in that same waiting state simultaneously with no thread left actually running tasks; a common fix is having the waiting thread itself participate in running pending/stealable work while it polls its awaited future.
