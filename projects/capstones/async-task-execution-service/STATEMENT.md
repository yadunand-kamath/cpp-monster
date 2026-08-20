# C-2 — Asynchronous Task Execution Service

**Level:** Capstone · **Est. effort:** XL (40h+, multi-week)

## Objective

Build a coroutine-based asynchronous task execution service that runs a large number of concurrent, cancellable, structured tasks on top of a work-stealing thread pool and an event loop, correctly propagating cancellation through a tree of dependent tasks and applying backpressure when work arrives faster than it can be processed. As with C-1, the specific scheduling policy, cancellation-propagation mechanism, and backpressure strategy are not named here — designing and justifying them is the point.

This capstone follows the same five-phase structure as C-1. Do not advance a phase until its bar is met.

## Phase 1 — Core Correctness

A minimal but correct task-scheduling core: coroutine-based tasks can be submitted, run to completion on worker threads, and their results retrieved; a task can await another task and correctly resume once the awaited task completes, without blocking a worker thread while waiting (the awaiting task's coroutine frame suspends and the worker thread goes on to do other work). No cancellation, no backpressure, no distributed/multi-thread stress yet. Exit bar: a correctness test suite demonstrating single-task execution, task-awaiting-task composition, and correct result/exception propagation, all passing under normal (non-adversarial) conditions.

## Phase 2 — Structured Concurrency and Cancellation

Implement structured concurrency: a group of related tasks (e.g. spawned together under one parent scope) has a well-defined lifetime, and cancelling the parent scope propagates cancellation to every task still running within it, cooperatively (a task observes cancellation at its own await points and unwinds/cleans up — this is not preemptive termination). A task's cleanup (RAII destructors) must run correctly even when unwound due to cancellation mid-execution. Exit bar: a test suite demonstrating cancellation of a running task tree correctly stops all in-flight leaf tasks, cleanup code runs for each, and no task tree can outlive its parent scope's cancellation (no dangling background work).

## Phase 3 — Scheduling, Backpressure, and Determinism

Run the task scheduler on a genuine work-stealing thread pool ([P-4.3](../../level-4/work-stealing-thread-pool/STATEMENT.md) is a natural foundation) integrated with an event loop for I/O-bound tasks ([P-4.1](../../level-4/single-threaded-event-loop/STATEMENT.md)), such that CPU-bound and I/O-bound tasks can be mixed without one starving the other. Apply backpressure when task submission outpaces processing capacity, with an explicit, documented policy (block the submitter, reject with an error, or queue with a bound — the specific choice must be stated and justified). Build a deterministic test scheduler — a mode where task interleaving is controlled/replayable rather than genuinely concurrent — specifically to make race-condition-sensitive tests reproducible rather than flaky. Exit bar: mixed CPU/I/O workload correctness and fairness demonstrated; backpressure policy demonstrated under an intentionally overloading test; the deterministic test scheduler is used to reproduce at least one interleaving-sensitive scenario reliably across repeated runs.

## Phase 4 — Performance

Establish and meet a stated throughput or latency target (e.g. tasks/second under a specific task-size distribution, or scheduling overhead per task) using [P-5.1](../../level-5/allocator-container-benchmark-harness/STATEMENT.md)'s harness, including at least one measured, profile-guided optimization with before/after numbers (a common target here: reducing per-task scheduling overhead, e.g. via the coroutine frame allocator pattern from [P-5.2](../../level-5/coroutine-task-generator-library/STATEMENT.md)). Exit bar: documented methodology, target, result, and optimization evidence.

## Phase 5 — Production Hardening

Address at least three of: a task-tree observability surface (in-flight task count, queue depth, per-task-type latency histograms); graceful drain-and-shutdown (in-flight tasks complete or are cleanly cancelled, no work is silently dropped); a documented policy and test for what happens when a task itself panics/throws unexpectedly outside normal error channels; resource limits (bounding total in-flight task count or memory under adversarial submission rates); a fairness test confirming no task type can starve another under sustained mixed load. Exit bar: each addressed area has a specific test or demonstration.

## Constraints

- C++20 coroutines (`<coroutine>`) as the task abstraction; no third-party async/coroutine framework for the core scheduler.
- Direct reuse of [P-4.3](../../level-4/work-stealing-thread-pool/STATEMENT.md) (thread pool), [P-4.1](../../level-4/single-threaded-event-loop/STATEMENT.md) (event loop), and [P-5.2](../../level-5/coroutine-task-generator-library/STATEMENT.md) (task/coroutine primitives) is expected and encouraged rather than reimplementing from scratch.
- Cancellation must be cooperative, not preemptive (no forcibly terminating a thread mid-instruction) — the correctness argument for cancellation safety depends on this.
- Must build and run correctly on MSVC and WSL-GCC/Clang; ThreadSanitizer verification (WSL) required for Phase 3's concurrency claims.

## Documentation Deliverable

A design document covering: the scheduling architecture (how CPU-bound and I/O-bound tasks are unified under one submission model), the structured-concurrency and cancellation-propagation mechanism with a correctness argument (why a cancelled task tree cannot leak a still-running orphan task), the backpressure policy and its justification, and an honest accounting of phases reached.

## Acceptance Criteria

- Phase 1 through at least Phase 3's exit bars are met, with Phase 4 and 5 attempted and honestly documented to whatever extent reached.
- The structured-concurrency cancellation test suite (Phase 2) demonstrates zero orphaned/leaked background tasks after a parent scope is cancelled, across repeated runs.
- The mixed CPU/I/O fairness test and the backpressure-under-overload test (Phase 3) both pass.
- The deterministic test scheduler successfully reproduces at least one interleaving-sensitive scenario across repeated runs (not merely "usually reproduces").
- The design document is complete and honest about scope reached.

## Hints

### Hint 1 — Direction
Resist building yet another independent coroutine-task type from scratch — this capstone's `task<T>` should be built directly on (or be a thin, purpose-specific evolution of) [P-5.2](../../level-5/coroutine-task-generator-library/STATEMENT.md)'s existing `task<T>`, with the scheduling and cancellation concerns layered on top as separate, additional pieces (a scheduler that decides which worker thread resumes a suspended coroutine; a cancellation token threaded through the awaiting chain) rather than baked into the coroutine promise type itself.

### Hint 2 — Technique
For cooperative cancellation, a `cancellation_token` (or `std::stop_token`-based) design that a task checks at its own await points — and that an awaiter can consult to decide "throw a cancellation exception here instead of resuming normally" — lets cancellation unwind naturally through C++'s existing exception/RAII machinery: a cancelled task's stack unwinds through its own local objects' destructors exactly as if it had thrown normally, which is what makes "cleanup runs correctly on cancellation" tractable without bespoke cleanup-tracking machinery.

### Hint 3 — Implementation
For a deterministic test scheduler, the key idea is to make "which ready task runs next" a controllable decision rather than an OS-thread-scheduling accident: run all tasks on a single logical thread (or a small controlled set) and drive resumption from an explicit, test-controlled loop that picks the next ready coroutine handle to resume according to a scripted or randomized-but-seeded order — this turns "does this specific interleaving reveal a bug" from a flaky, rare occurrence into a test you can run the exact same way every time.

### Hint 4 — Debugging/Design
If a cancellation test shows a task tree's cleanup running, but a *background* task spawned from within one of those tasks keeps running after cancellation, the bug is almost always a missing propagation link — the child task was created without being registered into the parent scope's structured-concurrency tracking, so the parent scope's cancellation had no way to know that child existed. Structured concurrency's core discipline is that no task may be spawned without its lifetime being tied to some enclosing scope; any exception to this discipline reintroduces exactly the "orphaned background work" failure mode Phase 2 is designed to catch.
