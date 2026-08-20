# PROGRESS.md — Completion Tracking

> Completion is defined by problems **solved**, never by reading (`PROMPT.md:848-850`). Structurally enforced: there is no "read" checkbox anywhere in this file. Reading a solution moves a problem to `◐ Assisted`, not `☑ Done`.

## Definitions of Done

**Problem-level:**
- `☑ Done` — solved unaided, then self-checked against the reference solution. Counts toward mastery.
- `◐ Assisted` — solved only after reading a hint tier ≥3, or after reading the reference solution. Does **not** count toward mastery. Automatically re-queued in Review Queue.
- `☐ Not Attempted` — default state.
- `✗ Attempted, Unresolved` — a real attempt was made and abandoned without reading the solution. Distinct from Not Attempted because it's diagnostic (see Failure Mode below); does not count toward mastery.

**Concept-level** (mirrors `CONCEPT_INDEX.md` Status column, kept in sync manually):
- Complete when ≥80% of its Related Problems are `☑ Done` **and** at least one of those is at difficulty ≥4.

**Chapter-level:**
- Complete when: all L1-L2 problems `☑`, ≥80% of L3-L4 problems `☑`, ≥1 L5+ problem `☑`, the Integration Challenge `☑`, and any chapter-linked project at PHASE 2 or later.

**Project-level** (see Project Log below): tracked by phase reached, not binary done/not-done, since `INSTRUCTIONS.md`'s acceptance criteria (tests passing, sanitizer-clean, performance targets) are graduated.

---

## Dashboard

Derived, not authoritative — recompute from Problem Log / Project Log when they diverge. All chapter content and all project/assessment content now exist in full (13/13 chapters, 26/26 projects, 3/3 capstones, 1/1 Grand Project, 5/5 Blind Challenges, 4/4 Principal-Level problems generated). Every row below is still at 0% because no problem has actually been attempted yet — generation status and completion status are independent axes; this table tracks the latter.

Each chapter's crash-course/problems live in `NN-topic/CONCEPTS.md`; solutions live in the sibling `NN-topic/SOLUTIONS.md`.

| Ch | Title | Problems Done | % | Integration Challenge | Chapter Project(s) | Chapter Status |
|---|---|---|---|---|---|---|
| 01 | Core Semantics | 0 / 45 | 0% | ☐ | P-1.1 ☐ | Content ready, not started |
| 02 | Lifetime, Ownership, RAII | 0 / 43 | 0% | ☐ | P-1.2 ☐ | Content ready, not started |
| 03 | Value Categories & Move Semantics | 0 / 40 | 0% | ☐ | P-1.3 ☐, P-1.4 ☐ | Content ready, not started |
| 04 | STL | 0 / 48 | 0% | ☐ | P-2.1 ☐ | Content ready, not started |
| 05 | Generic Programming | 0 / 50 | 0% | ☐ | P-2.3 ☐, P-2.5 ☐ | Content ready, not started |
| 06 | Error Handling & API Design | 0 / 38 | 0% | ☐ | P-2.4 ☐, P-3.5 ☐, P-3.6 ☐ | Content ready, not started |
| 07 | Object Model | 0 / 43 | 0% | ☐ | P-2.2 ☐ | Content ready, not started |
| 08 | Compilation, Linking, ABI | 0 / 39 | 0% | ☐ | — | Content ready, not started |
| 09 | Systems Programming (Linux/Windows) | 0 / 50 | 0% | ☐ | P-3.1 ☐, P-3.2 ☐, P-3.4 ☐, P-3.7 ☐, P-4.1 ☐, P-4.6 ☐ | Content ready, not started |
| 10 | Build Systems, Testing, CI | 0 / 35 | 0% | ☐ | P-3.3 ☐, P-5.6 ☐ | Content ready, not started |
| 11 | Concurrency & Memory Model | 0 / 55 | 0% | ☐ | P-4.2 ☐, P-4.3 ☐, P-5.3 ☐, P-5.4 ☐ | Content ready, not started |
| 12 | Performance | 0 / 41 | 0% | ☐ | P-4.5 ☐, P-5.1 ☐ | Content ready, not started |
| 13 | Modern C++ & Architecture | 0 / 41 | 0% | ☐ | P-5.2 ☐ | Content ready, not started |
| **Total** | | **0 / 568** | **0%** | | 27 projects, 3 capstones, 1 Grand Project | |

Blind Challenges: 0 / 5 attempted (BC-1 … BC-5 ready in `assessments/blind-challenges/`). Principal-Level: 0 / 4 attempted (PL-1 … PL-4 ready in `assessments/principal-level/`).

---

## Problem Log

Append-only. One row per attempt (a re-attempt after `◐`/`✗` gets its own new row, not an edit to the old one) — this is what makes Weak Areas and Recurring Mistakes computable rather than vibes.

**Failure Mode vocabulary (fixed — do not invent new values):** `misread-spec`, `wrong-mental-model`, `syntax`, `lifetime`, `UB-missed`, `concurrency`, `perf-assumption`, `api-design`, `toolchain`.

| ID | Ch | Level | Date | Outcome | Time | Failure Mode | Re-attempt Due |
|---|---|---|---|---|---|---|---|
| _(no entries yet — log begins once Ch01 is generated and attempted)_ | | | | | | | |

---

## Project Log

All project content (`STATEMENT.md`/`TESTS.md`/`HINTS.md`/`SOLUTION.md`) now exists for every row below. "Phase Reached" is `—` for Level 1-5 projects (they don't use the 5-phase structure) until started, then becomes a plain status; capstones use the 5-phase structure explicitly.

| ID | Title | Phase Reached | Tests Passing | Sanitizer-Clean | Perf Target Met | Last Updated | Notes |
|---|---|---|---|---|---|---|---|
| P-1.1 | Strong Typedef & Unit-Safe Quantity Type | Not started | — | — | — | — | |
| P-1.2 | Scoped Resource Handle Family | Not started | — | — | — | — | |
| P-1.3 | `small_vector<T, N>` | Not started | — | — | — | — | |
| P-1.4 | Copy/Move Instrumentation Harness | Not started | — | — | — | — | |
| P-2.1 | Log Line Indexer | Not started | — | — | — | — | |
| P-2.2 | SBO Variant Storage | Not started | — | — | — | — | |
| P-2.3 | `function_ref` + `unique_function` | Not started | — | — | — | — | |
| P-2.4 | Result/Error Propagation Library | Not started | — | — | — | — | |
| P-2.5 | Compile-Time Reflection-Free Serializer | Not started | — | — | — | — | |
| P-3.1 | Binary Object File Inspector | Not started | — | — | — | — | |
| P-3.2 | Cross-Platform File Watcher | Not started | — | — | — | — | |
| P-3.3 | Reusable Library Template & Test Harness | Not started | — | — | — | — | |
| P-3.4 | Content-Addressed Duplicate File Detector | Not started | — | — | — | — | |
| P-3.5 | Declarative Command-Line Parser | Not started | — | — | — | — | |
| P-3.6 | Layered Configuration Loader | Not started | — | — | — | — | |
| P-3.7 | Length-Prefixed TCP Request-Response Service | Not started | — | — | — | — | |
| P-4.1 | Single-Threaded Event Loop | Not started | — | — | — | — | |
| P-4.2 | Bounded MPMC Queue | Not started | — | — | — | — | |
| P-4.3 | Work-Stealing Thread Pool | Not started | — | — | — | — | |
| P-4.4 | Arena & Pool Allocator Suite | Not started | — | — | — | — | |
| P-4.5 | Concurrent Sharded Cache | Not started | — | — | — | — | |
| P-4.6 | Process Supervisor | Not started | — | — | — | — | |
| P-5.1 | Allocator & Container Benchmark Harness | Not started | — | — | — | — | |
| P-5.2 | Coroutine Task & Generator Library | Not started | — | — | — | — | |
| P-5.3 | Concurrent TCP Protocol Server | Not started | — | — | — | — | |
| P-5.4 | Write-Ahead Log with Crash Recovery | Not started | — | — | — | — | |
| P-5.5 | HTTP/1.1 Client with Connection Pool & Rate Limiter | Not started | — | — | — | — | |
| P-5.6 | Plugin Host with a Stable C ABI Boundary | Not started | — | — | — | — | |
| C-1 | Embeddable Persistent Key-Value Store | Phase 0 (not started) | — | — | — | — | 5-phase structure |
| C-2 | Asynchronous Task Execution Service | Phase 0 (not started) | — | — | — | — | 5-phase structure |
| C-3 | Cross-Platform Systems Profiler & Trace Viewer | Phase 0 (not started) | — | — | — | — | Elective |
| GP-1 | Algorithmic Trading System (Indian Cash Equity Markets) | Phase 0 (not started) | — | — | — | — | Grand Project, elective — not a capstone |

For capstones, "Phase Reached" refers to the 5-phase structure in [`PROJECT_ROADMAP.md`](PROJECT_ROADMAP.md#capstones--phase-1-5-structure). GP-1 (Grand Project) uses the same 5-phase structure but is tracked as a distinct tier from C-1/C-2/C-3.

---

## Assessments Log

Blind Challenges have no canonical solution — "Done" means you produced a working fix meeting the stated requirements and wrote the required explanation, self-assessed honestly against its Self-Assessment Questions. Principal-Level problems are design documents, not code — "Done" means the design document exists and was evaluated against its own rubric checklist.

| ID | Title | Placement | Status | Date | Notes |
|---|---|---|---|---|---|
| BC-1 | Allocation Reduction Under a Frozen Public API | After Ch03 | Not started | — | |
| BC-2 | Unifying Inconsistent Error Reporting | After Ch06 | Not started | — | |
| BC-3 | Windows-Only Handle Leak | After Ch09 | Not started | — | |
| BC-4 | Rare Concurrent Queue Race | After Ch11 | Not started | — | |
| BC-5 | 3x Slower After a Compiler Upgrade | After Ch12 | Not started | — | |
| PL-1 | High-Throughput Task Execution Service Design | Any time after Ch11-13 | Not started | — | |
| PL-2 | Time-Series Storage Engine Design | Any time after Ch09-12 | Not started | — | |
| PL-3 | Five-Year Two-Platform ABI & Versioning Policy | Any time after Ch08-09 | Not started | — | |
| PL-4 | "Our Build Takes 40 Minutes" | Any time after Ch10 | Not started | — | |

---

## Concept Mastery

Mirrors [`CONCEPT_INDEX.md`](CONCEPT_INDEX.md)'s Status column — keep both in sync when updating either. Summarized by group here; see CONCEPT_INDEX.md for the full ~230-row detail.

| Coverage Group | Concepts Complete | Concepts Total | % |
|---|---|---|---|
| Language & Core Semantics | 0 | 20 | 0% |
| Object Lifetime & Resource Management | 0 | 13 | 0% |
| Value Categories | 0 | 9 | 0% |
| STL | 0 | 15 | 0% |
| Generic Programming | 0 | 15 | 0% |
| Error Handling | 0 | 10 | 0% |
| Object Model | 0 | 15 | 0% |
| Compilation & Linking | 0 | 13 | 0% |
| Systems Programming | 0 | 12 | 0% |
| Build Systems, Testing, CI | 0 | 12 | 0% |
| Concurrency | 0 | 16 | 0% |
| Performance | 0 | 16 | 0% |
| Modern C++ & Architecture | 0 | 16 | 0% |
| **Total** | **0** | **~230** | **0%** |

---

## Weak Areas

Computed from Problem Log: any concept where `◐`/`✗` outcomes outnumber `☑` outcomes across ≥2 attempts. Empty until logging begins.

| Concept | ☑ | ◐ | ✗ | Dominant Failure Mode | Recommended Action |
|---|---|---|---|---|---|
| _(none yet)_ | | | | | |

---

## Recurring Mistakes

An entry qualifies only once it cites **≥2 problem IDs** — a single miss is not a pattern. Empty until logging begins.

| Pattern | Problem IDs | Failure Mode | First Seen | Status |
|---|---|---|---|---|
| _(none yet)_ | | | | |

---

## Review Queue

Spaced repetition queue. Seeded from two sources: (1) `CURRICULUM.md`'s Spaced Review Schedule (concepts due for re-testing in a later chapter, regardless of prior outcome), and (2) every problem currently marked `◐` or `✗` in the Problem Log (due for re-attempt on its own timeline, not tied to chapter order).

| Concept / Problem ID | Source | Due At (Chapter or Date) | Priority |
|---|---|---|---|
| _(queue is empty — populates once Problem Log has entries and/or chapters with scheduled revisits exist)_ | | | |

Reference — concepts with a scheduled revisit per `CURRICULUM.md` (populate this queue automatically once those chapters exist):

| Concept | Introduced | Revisit At |
|---|---|---|
| RAII | 02 | 06, 09, 11, 12 |
| Move semantics | 03 | 04, 05, 06, 12 |
| Iterator invalidation | 04 | 07, 11, 12 |
| Alignment/padding | 07 | 09, 11, 12 |
| Placement new / lifetime rules | 07 | 09, 11, 12 |
| ABI | 08 | 09, 10, 13 |

(Full 24-row table lives in `CURRICULUM.md` §7.)

---

## Session Log

One entry per working session. Purely descriptive — not used for completion computation, but useful for noticing pacing/drift.

| Date | Duration | Chapters/Projects Touched | Summary |
|---|---|---|---|
| 2026-08-19 | — | Planning docs | Generated `CURRICULUM.md`, `CONCEPT_INDEX.md`, `PROJECT_ROADMAP.md`, `PROGRESS.md` (this file). No chapter content yet. |
| 2026-08-19 | — | Ch01–13, all 26 projects, 3 capstones, 5 Blind Challenges, 4 Principal-Level problems | Full content generation pass across multiple sessions: all 13 chapters' `CONCEPTS.md`/`SOLUTIONS.md`, every Level 1–5 project's four-file set, all three capstones, and the full `assessments/` tree. This file's Dashboard, Project Log, and Assessments Log updated to reflect the now-complete ID sets — all still at 0% completion since no problem has actually been attempted. |
| 2026-08-20 | — | GP-1 (Grand Project): Algorithmic Trading System | Added GP-1 under `projects/grand-projects/` as a new sibling tier to the three capstones — same structural weight (5-phase, all-chapters, concepts unnamed) but a distinct category, per explicit instruction not to register it as a fourth capstone. Cash-equity Indian-market scope only; F&O/derivatives explicitly out of scope. Registered across `PROJECT_ROADMAP.md`, `PROGRESS.md`, `README.md`, `CURRICULUM.md`. |

---

## Notes on Using This File

- Update the Problem Log the moment you finish an attempt — not at the end of a session from memory. Memory of *why* something failed degrades fast; the Failure Mode field is worthless if it's a guess reconstructed hours later.
- Never backfill a `☑` for a problem you only read. If you're unsure whether an attempt counts, it's `◐`.
- The Dashboard and Concept Mastery tables are meant to be recomputed (by hand or by a future script) from the Problem/Project Logs — if they ever disagree, the Logs are the source of truth.
