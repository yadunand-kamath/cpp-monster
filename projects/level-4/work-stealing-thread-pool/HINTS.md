# P-4.3 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch11's concurrency-primitives material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Get task submission, `std::future`-based result delivery, and exception capture fully correct with a single worker thread and a single plain queue first — no stealing, no multiple workers. This gives you a trusted foundation for "the task/future machinery works" before you introduce the substantially harder concurrent-deque-with-stealing problem on top of it.

## Hint 2 — Technique

The owning worker treats its deque asymmetrically from thieves: it pushes and pops from one end (commonly called the "top," cheap because usually uncontended — only the owner touches it in the common case), while idle thieves steal from the opposite end (the "bottom," the oldest queued task). This asymmetry isn't arbitrary — it minimizes how often the owner's hot-path operations contend with a thief's occasional steal, and it tends to steal larger, coarser-grained work (older tasks in a divide-and-conquer decomposition tend to represent bigger subproblems) which better amortizes the cost of the steal itself.

## Hint 3 — Implementation

The genuinely hard correctness moment is when the deque has exactly one task left and both the owner (popping from the top) and a thief (stealing from the bottom) reach for it simultaneously — exactly one of them must succeed, and the other must observe the deque as empty. This is typically arbitrated with a compare-exchange on a shared index (or a versioned/tagged slot), structurally similar to [P-4.2](../bounded-mpmc-queue/STATEMENT.md)'s per-slot claiming logic — treat it with the same seriousness around memory ordering, and validate it the same way (a targeted high-iteration stress test under ThreadSanitizer).

## Hint 4 — Debugging/Design

If a test where a task spawns subtasks and then waits on their futures deadlocks, the likely cause is that the waiting thread is a worker that's now doing nothing but blocking — if every worker ends up blocked waiting on subtask futures simultaneously, and none of them are willing to actually execute pending (including stolen) work while waiting, no thread remains available to ever run the subtasks being waited on. The standard fix is to make a worker's "wait for this future" operation also participate in running other available work (its own queue, or stealing) rather than being a pure, work-refusing block.
