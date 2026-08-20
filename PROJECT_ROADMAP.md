# PROJECT_ROADMAP.md — Project & Assessment Master Plan

> Derived from [`CURRICULUM.md`](CURRICULUM.md)'s chapter prerequisite graph. Full problem statements live in each project's own `STATEMENT.md`. This file tracks pointer metadata only: what exists, what it needs, what it's for. Generation status and completion status are independent axes — everything below is generated; the `☐ Not started` column tracks whether *you've solved it*, per `PROGRESS.md`'s Definitions of Done.

## Difficulty Levels (`INSTRUCTIONS.md:265-291`)

| Level | Name | Definition |
|---|---|---|
| 1 | Focused component | One major concept |
| 2 | Multi-concept component | Several related concepts |
| 3 | Realistic utility | Useful standalone program/library |
| 4 | Systems component | Concurrency, resource management, performance, or OS interaction |
| 5 | Production-style project | Multiple components, tests, error handling, performance, maintainability |
| 6 | Capstone | Large system requiring architecture and significant engineering judgment |

Per-project directory layout: `projects/level-N/<slug>/{STATEMENT.md, TESTS.md, HINTS.md, SOLUTION.md}` — four separate files so the solution can't be scrolled into by accident.

Per Deviation #3 (`CURRICULUM.md` §12): the source specs define 6 project levels but only sketch 4 directories + capstones in the file tree. This roadmap follows `INSTRUCTIONS.md` and adds `projects/level-5/` explicitly.

**Note on project selection:** per explicit user instruction, the spec's example project list (`PROMPT.md:547-611`) is a floor, not a menu. Several projects below deliberately replace or exceed the spec's suggestions with more creative/useful alternatives. None of the anti-examples at `PROMPT.md:535-541` (calculator, basic todo list, student management system, guessing game, tic-tac-toe, trivial CRUD) appear anywhere in this roadmap.

---

## Master Table

| ID | Title | Level | Category | Directory | Requires Ch | Primary Concepts | Est. Effort | Status |
|---|---|---|---|---|---|---|---|---|
| P-1.1 | Strong Typedef & Unit-Safe Quantity Type | 1 | Libraries | `projects/level-1/strong-typedef/` | 01 | Operator overloading, implicit/explicit conversions, templates | S (4-8h) | ☐ Not started |
| P-1.2 | Scoped Resource Handle Family (`scoped_file`, `scoped_timer`, generic `scope_exit`) | 1 | Systems | `projects/level-1/scoped-resource/` | 01-02 | RAII, custom deleters, move-only types | S (4-8h) | ☐ Not started |
| P-1.3 | `small_vector<T,N>` with inline capacity | 1 | Libraries | `projects/level-1/small-vector/` | 01-03 | Placement construction, move semantics, alignment | M (8-16h) | ☐ Not started |
| P-1.4 | Copy/Move Instrumentation Harness | 1 | Performance | `projects/level-1/copy-move-harness/` | 01-03 | Copy elision, NRVO, `move_if_noexcept` | S (4-8h) | ☐ Not started |
| P-2.1 | Log Line Indexer (1 GB log, bounded memory) | 2 | Dev Tools | `projects/level-2/log-line-indexer/` | 01-04 | `string_view`, iterators, algorithms, containers | M (8-16h) | ☐ Not started |
| P-2.2 | SBO Variant Storage (`inplace_any<N>`) | 2 | Libraries | `projects/level-2/inplace-any/` | 01-04, 07 | Alignment, triviality, `variant`/`optional` design | M (8-16h) | ☐ Not started |
| P-2.3 | `function_ref` + `unique_function` | 2 | Libraries | `projects/level-2/function-ref-unique-function/` | 01-05 | Type erasure, forwarding references, `std::invoke` | M (8-16h) | ☐ Not started |
| P-2.4 | Result/Error Propagation Library | 2 | Libraries | `projects/level-2/result-error-propagation/` | 01-06 | `std::expected`, monadic composition, exception guarantees | M (8-16h) | ☐ Not started |
| P-2.5 | Compile-Time Reflection-Free Serializer | 2 | Libraries | `projects/level-2/reflection-free-serializer/` | 01-05, 07 | Variadic templates, fold expressions, endianness | L (16-24h) | ☐ Not started |
| P-3.1 | Binary Object File Inspector (ELF + PE) | 3 | Dev Tools | `projects/level-3/binary-object-inspector/` | 01-09 | Symbol resolution, name mangling, ABI, memory mapping | L (16-24h) | ☐ Not started |
| P-3.2 | Cross-Platform File Watcher | 3 | Systems | `projects/level-3/file-watcher/` | 01-09 | `inotify` vs `ReadDirectoryChangesW`, event coalescing | M (8-16h) | ☐ Not started |
| P-3.3 | Reusable Library Template & Test Harness | 3 | Dev Tools | `projects/level-3/library-template-harness/` | 08-10 | CMake targets, `PUBLIC`/`PRIVATE`/`INTERFACE`, GoogleTest/CTest, CI | L (16-24h) | ☐ Not started |
| P-3.4 | Content-Addressed Duplicate File Detector | 3 | Dev Tools | `projects/level-3/duplicate-file-detector/` | 01-09 | Hashing pipelines, streaming I/O, resumability | L (16-24h) | ☐ Not started |
| P-3.5 | Declarative Command-Line Parser | 3 | Libraries | `projects/level-3/declarative-cli-parser/` | 01-06 | Compile-time spec, `std::expected` error design | M (8-16h) | ☐ Not started |
| P-3.6 | Layered Configuration Loader | 3 | Libraries | `projects/level-3/layered-configuration-loader/` | 01-06, 09 | Precedence merging, schema validation, hot reload | M (8-16h) | ☐ Not started |
| P-3.7 | Length-Prefixed TCP Request-Response Service | 3 | Networking | `projects/level-3/tcp-echo-request-response/` | 01-09 | Blocking sockets, length-prefix framing, thread-per-connection | M (8-16h) | ☐ Not started |
| P-4.1 | Single-Threaded Event Loop (epoll + IOCP, one interface) | 4 | Systems | `projects/level-4/event-loop/` | 01-09 | Readiness vs completion I/O models, timers, cancellation | L (16-24h) | ☐ Not started |
| P-4.2 | Bounded MPMC Queue (lock-based + lock-free) | 4 | Systems | `projects/level-4/bounded-mpmc-queue/` | 01-11 | Atomics, memory ordering, data-race freedom (TSan-clean) | L (16-24h) | ☐ Not started |
| P-4.3 | Work-Stealing Thread Pool | 4 | Systems | `projects/level-4/work-stealing-thread-pool/` | 01-11 | `jthread`/`stop_token`, futures, exception capture | L (16-24h) | ☐ Not started |
| P-4.4 | Arena & Pool Allocator Suite | 4 | Systems | `projects/level-4/arena-pool-allocators/` | 01-07, 12 | Placement new, `std::pmr`, object lifetime rules | M (8-16h) | ☐ Not started |
| P-4.5 | Concurrent Sharded Cache (memory-bounded, TTL) | 4 | Storage | `projects/level-4/concurrent-sharded-cache/` | 01-12 | Sharded locking, eviction, hit-rate metrics | L (16-24h) | ☐ Not started |
| P-4.6 | Process Supervisor (restart w/ backoff) | 4 | Systems | `projects/level-4/process-supervisor/` | 01-09, 11 | Signals vs SEH, pipe capture, graceful/forced termination | L (16-24h) | ☐ Not started |
| P-5.1 | Allocator & Container Benchmark Harness | 5 | Performance | `projects/level-5/allocator-container-benchmark-harness/` | 01-12 | Statistically sound benchmarking, optimizer-defeat, CI regression gate | L (16-24h) | ☐ Not started |
| P-5.2 | Coroutine Task & Generator Library | 5 | Libraries | `projects/level-5/coroutine-task-generator-library/` | 01-13 | `co_await`/`co_yield`, symmetric transfer, frame allocation | XL (24h+) | ☐ Not started |
| P-5.3 | Concurrent TCP Protocol Server (10k connections) | 5 | Networking | `projects/level-5/concurrent-tcp-protocol-server/` | 01-11 | Sockets, backpressure, measured p99 latency | XL (24h+) | ☐ Not started |
| P-5.4 | Write-Ahead Log with Crash Recovery | 5 | Storage | `projects/level-5/write-ahead-log-crash-recovery/` | 01-09, 11 | CRC framing, group commit, crash-injection testing | XL (24h+) | ☐ Not started |
| P-5.5 | HTTP/1.1 Client (connection pool + rate limiter) | 5 | Networking | `projects/level-5/http-client-pool-rate-limiter/` | 01-11 | Keep-alive, chunked transfer, token-bucket rate limiting | L (16-24h) | ☐ Not started |
| P-5.6 | Plugin Host with a Stable C ABI Boundary | 5 | Systems | `projects/level-5/plugin-host-stable-abi/` | 01-10 | Runtime `.so`/`.dll` loading, versioned interfaces, safe unload | L (16-24h) | ☐ Not started |
| C-1 | Embeddable Persistent Key-Value Store | 6 | Storage | `projects/capstones/embeddable-persistent-kv-store/` | all | WAL, page cache, snapshot reads, compaction | XL (40h+) | ☐ Not started |
| C-2 | Asynchronous Task Execution Service | 6 | Systems | `projects/capstones/async-task-execution-service/` | all | Coroutine scheduler, structured concurrency, cancellation, deterministic testing | XL (40h+) | ☐ Not started |
| C-3 | Cross-Platform Systems Profiler & Trace Viewer *(elective)* | 6 | Performance | `projects/capstones/systems-profiler-trace-viewer/` | all | Sampling + instrumented tracing, ELF/DWARF and PE/PDB symbolization | XL (40h+) | ☐ Not started |

**Effort key:** S = 4-8h, M = 8-16h, L = 16-24h, XL = 24h+ (capstones 40h+). These are estimates for someone who has completed the relevant chapters, not absolute floors.

---

## Grand Projects

A Grand Project is a fourth, sibling tier to the three Capstones (C-1/C-2/C-3), not a fourth capstone — deliberately kept out of the Master Table above and out of the C-1..C-3 numbering. It shares a Capstone's structural weight (five phases, `all` chapters required, concepts deliberately unnamed) but is scoped around a domain the Capstones don't touch, and is optional in the same sense the elective C-3 is. Registered separately so a learner scanning the Master Table sees exactly three capstones, as intended, and finds Grand Projects as a distinct, clearly-labeled option alongside them.

| ID | Title | Level | Category | Directory | Requires Ch | Primary Concepts | Est. Effort | Status |
|---|---|---|---|---|---|---|---|---|
| GP-1 | Algorithmic Trading System (Indian Cash Equity Markets) | 6 (Grand Project) | Networking | `projects/grand-projects/algorithmic-trading-system/` | all | Deterministic backtest/live parity, broker-agnostic adapter seam, ordered risk-check pipeline, lock-free hot-path optimization | XL+ (50h+) | ☐ Not started |

---

## Category × Level Coverage Grid

Confirms no category is empty and every category from `PROMPT.md:547-611` is represented at multiple levels.

| Category | L1 | L2 | L3 | L4 | L5 | L6 (Capstone) | Grand Project |
|---|---|---|---|---|---|---|---|
| Developer Tools | — | — | P-3.1, P-3.3, P-3.4 | — | — | C-3 (elective) | — |
| Systems | P-1.2 | — | P-3.2 | P-4.1, P-4.2, P-4.3, P-4.4, P-4.6 | P-5.6 | C-2 | — |
| Networking | — | — | P-3.7 | — | P-5.3, P-5.5 | — | GP-1 |
| Storage | — | — | — | P-4.5 | P-5.4 | C-1 | — |
| Libraries | P-1.1, P-1.3 | P-2.2, P-2.3, P-2.4, P-2.5 | P-3.5, P-3.6 | — | P-5.2 | — | — |
| Performance | P-1.4 | — | — | — | P-5.1 | C-3 (elective) | — |

Every row has at least one entry; no category is orphaned at the systems/production tiers where it matters most (Systems, Storage, Networking all reach L4-L6). Networking's L6 cell is filled by GP-1 rather than a capstone — the Grand Project column is intentionally separate from the Capstone column since GP-1 is not a fourth capstone.

---

## Concept → Project Inverse Index

Cross-reference against [`CONCEPT_INDEX.md`](CONCEPT_INDEX.md)'s per-concept `Related Projects` column — this table is its mirror, organized by project instead of by concept.

| Concept Area | Projects That Exercise It |
|---|---|
| RAII / ownership | P-1.2, P-1.3, P-2.1, P-4.4, P-5.6 |
| Move semantics / copy elision | P-1.3, P-1.4, P-2.1 |
| Type erasure | P-2.2, P-2.3, P-5.2 |
| Templates / concepts / metaprogramming | P-1.1, P-1.3, P-2.3, P-2.5 |
| Error handling (`expected`, error design) | P-2.4, P-3.5, P-3.6 |
| Object model / ABI | P-2.2, P-3.1, P-5.6 |
| Compilation, linking, build systems | P-3.1, P-3.3, P-5.6 |
| Systems programming (fd/HANDLE, mmap, signals, I/O models) | P-1.2, P-3.2, P-3.4, P-4.1, P-4.6, P-5.6 |
| Concurrency & memory model | P-4.2, P-4.3, P-4.5, P-5.3, P-5.4, C-2 |
| Performance / allocators / benchmarking | P-1.4, P-4.4, P-5.1, C-3 |
| Networking | P-4.1, P-3.7, P-5.3, P-5.5, GP-1 |
| Storage / durability | P-4.5, P-5.4, C-1 |
| Coroutines / modern C++ | P-5.2, C-2 |
| Architecture / API design | P-2.4, P-3.5, P-5.2, P-5.6, C-1, C-2, C-3, GP-1, PL-1..4 |

---

## PHASE 1-5 Structure

Per `PROMPT.md:628-644`, larger projects are built in five phases rather than delivered as a single monolithic spec. This applies to the 3 capstones and, since they're substantial (L/XL, several 24h+) production-style projects in their own right, to all six Level-5 projects (P-5.1–P-5.6) as well — each has its own `STATEMENT.md`-specific phase breakdown fitted to what it's actually building. Capstones additionally — per `PROMPT.md:692` — have their concepts deliberately **not named** in the problem statement; identifying which chapters apply is part of the exercise. Level-5 projects do name their concepts (per the Primary Concepts column above).

**Common phase skeleton** (elaborated per-project in each `STATEMENT.md`):

1. **Phase 1 — Core correctness.** Minimal single-threaded/single-node version that is functionally correct with no crash-safety or performance requirements yet.
2. **Phase 2 — Durability / correctness under failure.** Crash-consistency, recovery, or data-race freedom introduced as a hard requirement.
3. **Phase 3 — Concurrency / scale.** Multi-threaded or multi-connection operation under load.
4. **Phase 4 — Performance.** Measured throughput/latency targets, benchmarked and regression-gated.
5. **Phase 5 — Production hardening.** Observability, graceful degradation, documented failure modes, API stability.

| ID | Title | Category | Notable Phase-2+ Requirement |
|---|---|---|---|
| C-1 | Embeddable Persistent Key-Value Store | Storage | Crash-consistency suite (kill -9 mid-write, verify recovery) |
| C-2 | Asynchronous Task Execution Service | Systems | Deterministic test scheduler (reproducible interleavings) |
| C-3 | Cross-Platform Systems Profiler & Trace Viewer *(elective)* | Performance | Ring buffer under 3% steady-state overhead |
| GP-1 | Algorithmic Trading System *(Grand Project, elective)* | Networking | Backtest/live parity test (byte-identical `OrderIntent` sequences, zero tolerance) |

---

## Assessments — Blind Challenges & Principal-Level Problems

Per `PROMPT.md:698-735`: requirements only, target concepts deliberately unnamed. These live in `assessments/` with a grading rubric, never a single canonical solution.

### Blind Challenges (`assessments/blind-challenges/`)

| ID | Placement | Prompt (as given to the learner) |
|---|---|---|
| BC-1 | After Ch03 | "Cut this pipeline's allocation count 10x without changing its public API." |
| BC-2 | After Ch06 | "This library reports errors three inconsistent ways. Unify it. The public API may not break." |
| BC-3 | After Ch09 | "This service leaks handles under load — on Windows only." |
| BC-4 | After Ch11 | "This queue passes 10,000 test runs and fails in production once a week." |
| BC-5 | After Ch12 | "This code got 3x slower after a compiler upgrade. The source did not change." |

### Principal-Level Design Problems (`assessments/principal-level/`)

| ID | Prompt |
|---|---|
| PL-1 | Design a high-throughput task execution service. |
| PL-2 | Design a storage engine for a write-heavy time-series workload. |
| PL-3 | Design the ABI and versioning policy for a library shipped to external customers on two platforms for five years. |
| PL-4 | "Our C++ build takes 40 minutes. Design the fix." *(deliberately under-specified)* |

Graded by rubric (architecture soundness, trade-off articulation, edge-case awareness) — not against a single reference answer, per `INSTRUCTIONS.md:82-87`.

---

## Sequencing Recommendation

Projects are not meant to be done in a single end-of-book block. Interleave with chapters as prerequisites are met:

| After completing... | Do these projects |
|---|---|
| Ch01 | P-1.1 |
| Ch02 | P-1.2 |
| Ch03 | P-1.3, P-1.4, **BC-1** |
| Ch04 | P-2.1 |
| Ch05 | P-2.3, P-2.5 |
| Ch06 | P-2.4, P-3.5, P-3.6, **BC-2** |
| Ch07 | P-2.2, P-4.4 (allocator portion) |
| Ch08 | — (infrastructure chapter; P-3.1 needs Ch09 too) |
| Ch09 | P-3.1, P-3.2, P-3.4, P-3.7, P-4.1, P-4.6, **BC-3** |
| Ch10 | P-3.3, P-5.6 |
| Ch11 | P-4.2, P-4.3, P-5.3, P-5.4, **BC-4** |
| Ch12 | P-4.5, P-5.1, **BC-5** |
| Ch13 | P-5.2, PL-1..4 |
| All chapters | C-1, C-2, C-3 (elective) — in any order, per interest |
| All chapters | GP-1 (Grand Project, elective) — a fourth, optional synthesis project, not part of the C-1..C-3 capstone set |

This ordering keeps every project gradable against acceptance criteria that are actually achievable at that point in the curriculum (e.g., no project claims "TSan-clean" before Ch10's `wsl-clang-tsan` preset exists, and none claims a throughput target before Ch12's benchmarking methodology is taught).
