# C-3 — Cross-Platform Systems Profiler & Trace Viewer (Elective)

**Level:** Capstone (Elective) · **Est. effort:** XL (40h+, multi-week)

## Objective

Build a low-overhead profiler that captures both sampling-based (periodic stack snapshots) and instrumented (explicit begin/end markers) trace data from a running process on both Linux and Windows, symbolizes captured addresses back to function names and source locations (ELF/DWARF on Linux, PE/PDB on Windows), and stores the resulting trace in a format a viewer can render as a timeline. This is marked elective: it is the most systems-programming-heavy and platform-divergent of the three capstones, and depends most heavily on Ch09's paired Linux/Windows material.

## Phase 1 — Core Correctness

Instrumented tracing first: a lightweight API (`TRACE_SCOPE("name")` or equivalent) that records begin/end timestamps for named regions into an in-memory ring buffer, correctly nested (regions properly parent/child even when nested calls span the same thread), with correct behavior across multiple threads (each thread's events attributed correctly, no cross-thread event corruption). Exit bar: a correctness test suite demonstrating correct nesting, correct multi-thread attribution, and correct ring-buffer wraparound behavior (old events correctly overwritten/evicted under sustained load, no corruption of remaining valid events).

## Phase 2 — Sampling and Symbolization

Add sampling-based profiling: periodically capture the call stack (program counter chain) of a running thread without the thread's cooperation (using platform-appropriate mechanisms — e.g. a signal-based or separate-thread-based sampler on Linux, `SuspendThread`/`GetThreadContext`/`StackWalk64` on Windows), and symbolize captured addresses into function names and source locations using the target's actual debug information (DWARF for ELF binaries, PDB for PE binaries) — not just raw addresses. Exit bar: a demonstration that sampling a known synthetic call stack (a program with deliberately named, deliberately deep functions) produces correctly symbolized, correctly ordered stack traces on both platforms.

## Phase 3 — Overhead and Ring Buffer Correctness Under Load

Demonstrate the instrumented-tracing overhead stays under a stated budget (e.g. under 3% wall-clock overhead) on a realistic workload, and that the ring buffer and sampler remain correct (no corruption, no crash, no unbounded memory growth) under sustained high-frequency tracing across many threads. Exit bar: a measured overhead report against the stated budget, and a stress test sustaining high-frequency multi-threaded tracing for an extended duration with no corruption.

## Phase 4 — Trace Viewer

Build a viewer (a standalone tool, need not be graphical — a well-organized textual/HTML timeline export is acceptable) that renders captured trace data as a readable timeline: per-thread lanes, nested instrumented regions shown as nested intervals, sampled stacks shown in a way that makes hot call paths identifiable (e.g. a simple flame-graph-style aggregation). Exit bar: a captured trace from a real (not synthetic) multi-threaded workload is rendered and visibly shows that workload's actual structure (its thread count, its nested call structure, its hot paths) correctly.

## Phase 5 — Production Hardening

Address at least three of: safe behavior when symbolization data (DWARF/PDB) is missing or mismatched (degrade to raw addresses, don't crash); a documented and tested policy for sampling/tracing a process other than the profiler's own (out-of-process, if attempted) versus in-process only (a valid, simpler scope choice if documented as such); resource limits on the ring buffer and symbol cache; a fuzz-tested trace-file parser (if the trace format is a file the viewer reads back); an observability/self-check mode confirming the profiler's own overhead measurement is itself accurate. Exit bar: each addressed area has a specific test or demonstration.

## Constraints

- C++20. Platform-specific stack-walking, symbolization, and sampling APIs are expected and should follow Ch09's paired-platform presentation style — a genuine model difference (readiness-vs-completion-style divergence), not just different function names for the same idea.
- In-process profiling of the profiler's own host process is the required minimum scope; out-of-process profiling of a separate target process is an optional, harder extension, not a baseline requirement.
- No third-party profiling library for the core capture/ring-buffer/symbolization logic; parsing DWARF/PDB debug information may use a well-scoped existing library for the on-disk format parsing itself (writing a full DWARF or PDB parser from scratch is out of scope for this capstone's value), but the decision of what to use and why belongs in the design document.

## Documentation Deliverable

A design document covering: the ring-buffer and instrumented-tracing design and its multi-thread-safety argument; the sampling mechanism on each platform and the specific divergence between them; the symbolization pipeline; the measured overhead methodology and result against the stated budget; and an honest accounting of phases reached.

## Acceptance Criteria

- Phase 1 through at least Phase 3's exit bars are met, with Phase 4 and 5 attempted and honestly documented to whatever extent reached.
- The multi-thread nesting-correctness test suite (Phase 1) passes.
- Sampling and symbolization are demonstrated correctly on both Linux and Windows against a known synthetic call stack (Phase 2).
- The measured overhead report (Phase 3) states a specific budget and a specific measured result against a realistic workload.
- If reached, the trace viewer (Phase 4) is demonstrated against a real captured trace, not only synthetic data.

## Hints

### Hint 1 — Direction
Build the instrumented tracing (Phase 1) completely before touching sampling (Phase 2) — they are largely independent capture mechanisms feeding the same underlying ring-buffer/storage format, and instrumented tracing's correctness (nesting, thread attribution, ring-buffer wraparound) is far easier to test deterministically than sampling's correctness, since instrumented events happen exactly when your code says they happen.

### Hint 2 — Technique
For low-overhead capture, avoid any lock or allocation on the hot path of recording an event — a per-thread, lock-free (or single-writer-per-thread, so no lock needed at all) ring buffer that each thread writes into independently, combined at analysis/export time rather than at capture time, is the standard technique that makes sub-3%-overhead achievable; a shared, mutex-protected buffer written from every thread will make that budget very difficult to hit under real multi-threaded load.

### Hint 3 — Implementation
For symbolization, treat "resolve this address to a function name and source location" as a batch, post-capture step rather than something done live during tracing — capturing raw addresses during the hot path and symbolizing them afterward (when exporting or viewing the trace) keeps the hot path's overhead minimal and lets you cache/batch the comparatively expensive DWARF/PDB lookup work.

### Hint 4 — Debugging/Design
If your measured overhead is far above budget, profile the profiler itself before guessing — a common, non-obvious culprit is the timestamp-acquisition call itself (some clock sources are far more expensive than others on a given platform) rather than the ring-buffer write; measure the cost of *just* the timestamp call in isolation before concluding the ring buffer design is the bottleneck.
