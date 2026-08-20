# PL-1 — Design a High-Throughput Task Execution Service

**Format:** Principal-level design problem. This document is a rubric for self- or peer-evaluation, not a solution — there is no single correct design, and a strong answer will make and defend specific tradeoffs rather than trying to satisfy every desirable property maximally.

## The Prompt

Design (on paper/in a design document — no code required, though sketching key interfaces is encouraged) a task execution service intended to run a very large number of short-lived, heterogeneous units of work (a mix of CPU-bound and I/O-bound tasks) submitted by many concurrent clients, at high throughput and with predictable tail latency. Assume this will be deployed as a long-running production service handling sustained real-world load, not a batch job.

Your design should address, at minimum:
- The task submission and scheduling model.
- How CPU-bound and I/O-bound work is unified (or deliberately kept separate) under one execution model.
- Backpressure and overload behavior.
- Failure handling: what happens when a task throws/panics, when a worker crashes, when the service itself needs to restart.
- Observability: what a human operating this service in production needs to be able to see.
- At least one explicit, named tradeoff you made and why you made it that way rather than the alternative.

## Evaluation Rubric

### Problem framing (does the design address the actual problem?)
- ☐ States explicit, concrete assumptions about scale (tasks/second, task size distribution, client count) rather than designing in a vacuum — a design that never commits to a scale target can't be evaluated against one.
- ☐ Distinguishes CPU-bound and I/O-bound task handling explicitly, rather than treating "task" as an undifferentiated unit of work.
- ☐ Addresses tail latency, not just average throughput — a design optimizing only for average throughput while ignoring p99 behavior under load is a common, serious gap.

### Architecture and correctness
- ☐ The scheduling/concurrency model is described precisely enough that a reader could identify potential race conditions or starvation scenarios from the design alone (not just "we'll use a thread pool").
- ☐ Backpressure has an explicit, stated policy (reject, block, queue-with-bound) rather than being left implicit or "handled by the OS."
- ☐ Failure handling covers task-level failure, worker-level failure, and whole-service restart/drain as three genuinely distinct scenarios, each addressed.
- ☐ If structured concurrency or task cancellation is part of the design, cancellation propagation is addressed explicitly (what happens to in-flight dependent work when a request is cancelled).

### Tradeoffs and judgment
- ☐ At least one tradeoff is named explicitly with a real alternative that was rejected, and a specific reason — not a vague "there are tradeoffs here."
- ☐ The design acknowledges at least one weakness or limitation of its own chosen approach, rather than presenting only upsides.
- ☐ Scaling behavior at 10x and 100x the assumed baseline scale is at least briefly addressed — does the design degrade gracefully, or does it hit a wall, and where?

### Communication
- ☐ A reader unfamiliar with this specific design could, from the document alone, correctly predict what happens to a specific example request under normal load, under overload, and under a worker crash.
- ☐ Diagrams or explicit interface sketches are used where they clarify structure faster than prose would.

## Suggested Self-Check

Before considering this complete, try to argue the *opposing* side of your single most consequential design decision as convincingly as you can. If you can't construct a serious counter-argument, you likely haven't stress-tested the decision enough yet. This capstone-adjacent design connects directly to [C-2](../../projects/capstones/async-task-execution-service/STATEMENT.md) — building a working implementation of a design very much like this one is a natural way to validate whether the design actually holds up under real construction.
