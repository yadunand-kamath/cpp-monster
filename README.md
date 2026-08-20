# C++ Monster

A self-study workbook that will make you a C++ monster.

This is **not** a book to read. It is ~568 problems and 27 projects (plus 3 capstones, 1 Grand Project, 5 Blind Challenges, and 4 Principal-Level design problems) organized into 13 chapters, designed to be *solved*.

## What This Is

- Source specs: [`PROMPT.md`](PROMPT.md) (master generation prompt) and [`INSTRUCTIONS.md`](INSTRUCTIONS.md) (solution/project-statement quality bar). Both are read-only references — don't edit them; if something here conflicts with them, the conflict is deliberate and recorded in [`CURRICULUM.md`](CURRICULUM.md) §12 (Deviations).
- Planning documents: [`CURRICULUM.md`](CURRICULUM.md), [`CONCEPT_INDEX.md`](CONCEPT_INDEX.md), [`PROJECT_ROADMAP.md`](PROJECT_ROADMAP.md), [`PROGRESS.md`](PROGRESS.md), and this file.
- Chapter content, project statements, and solutions: **all generated** — 13 chapters, 27 projects, 3 capstones, 1 Grand Project, 5 Blind Challenges, 4 Principal-Level problems. See Status below. (Generation status and completion status are independent axes — generated does not mean solved.)

## How To Use It

Each chapter follows: **READ → ATTEMPT → CHECK → UNDERSTAND → APPLY**, without needing anything outside the chapter to close the loop.

1. Read the Crash Course (~15 minutes) and Common Misconceptions — don't linger here, it's orientation, not the content.
2. Do the Quick Checks.
3. Work the Problems in order — they run L1 (Recognition) through L7 (Principal Reasoning), with debugging problems interleaved by difficulty and tagged by type rather than segregated (so you don't know a problem is "the broken one" before reading it).
4. Attempt the Integration Challenge and any chapter Project(s) before opening `SOLUTIONS.md`.
5. Check your answer against `SOLUTIONS.md` — solutions live in a **separate sibling file**, never inline, so you can't scroll past a problem and accidentally see the answer.
6. If you needed the solution (or a hint past tier 3) to finish, mark it `◐ Assisted` in [`PROGRESS.md`](PROGRESS.md), not `☑ Done` — see that file's Definitions of Done. Assisted problems get re-queued for a later cold attempt.

Rough time budget per the source spec: ~15 minutes of reading per chapter section, then 2-5 hours of doing. If a chapter is taking a fraction of that, you're likely skimming, not solving.

## Repository Layout

```
cpp-workbook/
├── README.md                      (this file)
├── PROMPT.md                      (source spec — read-only)
├── INSTRUCTIONS.md                (source spec — read-only)
├── CURRICULUM.md                  (source of truth: architecture, coverage, deviations)
├── CONCEPT_INDEX.md               (random-access lookup by concept + symptom→concept reverse index)
├── PROJECT_ROADMAP.md             (all projects/capstones/assessments, sequencing)
├── PROGRESS.md                    (your tracked completion state)
│
├── 01-core-semantics/             ┐
├── 02-lifetime-raii/              │
├── 03-value-categories/           │
├── 04-stl/                        │
├── 05-generic-programming/        │  each contains CONCEPTS.md (content)
├── 06-error-handling/             │  + SOLUTIONS.md (sibling file)
├── 07-object-model/               │
├── 08-compilation-abi/            │
├── 09-systems-programming/        │  (Linux/Windows paired sections)
├── 10-build-systems/              │
├── 11-concurrency/                │
├── 12-performance/                │
├── 13-modern-cpp-architecture/    ┘
│
├── projects/
│   ├── level-1/ … level-5/        each project: STATEMENT.md, TESTS.md, HINTS.md, SOLUTION.md
│   ├── capstones/                 C-1, C-2, C-3 — PHASE 1-5 structure, concepts unnamed
│   └── grand-projects/            GP-1 — sibling tier to capstones, own domain (algorithmic trading)
│
└── assessments/
    ├── blind-challenges/          BC-1..5 — requirements only, concepts unnamed
    └── principal-level/           PL-1..4 — rubric-graded design problems
```

All chapter/project/assessment directories above exist on disk with generated content.

## Toolchain Setup

**Primary: MSVC, Visual Studio 2022.**
- `/std:c++20` for stable material; `/std:c++latest` where a problem specifically needs a C++23 feature (e.g. `std::expected`, `std::generator`).
- `/W4 /permissive- /Zc:preprocessor` as the baseline warning/conformance flags.
- `/fsanitize=address` for ASan (available on MSVC since VS 2019 16.9).
- `dumpbin /symbols`, `dumpbin /exports` for symbol inspection (Ch08).

**Secondary: GCC/Clang via WSL2.** Needed for:
- `nm -C`, `objdump -T`, `readelf -d` (Ch08 — Itanium ABI symbol/name-mangling problems have no MSVC equivalent tool).
- `-fvisibility=hidden` (Ch08, Ch10 — no direct MSVC equivalent; MSVC uses explicit `__declspec(dllexport/dllimport)` instead).
- ThreadSanitizer and MemorySanitizer (see matrix below).

### Sanitizer Availability Matrix

| Sanitizer | MSVC (Windows) | Clang/GCC (WSL) | Used By |
|---|---|---|---|
| AddressSanitizer (ASan) | ✅ `/fsanitize=address` | ✅ `-fsanitize=address` | Ch02, 07, 09, 12 |
| UndefinedBehaviorSanitizer (UBSan) | ❌ not available | ✅ `-fsanitize=undefined` | Ch01, 07, 08 |
| ThreadSanitizer (TSan) | ❌ **not available** | ✅ `-fsanitize=thread` | Ch11 — required for "data-race-free" acceptance criteria |
| MemorySanitizer (MSan) | ❌ **not available** | ✅ `-fsanitize=memory` (Clang only) | Ch07, 09 (uninitialized-read problems) |

**Consequence:** any project or problem whose acceptance criteria says "TSan-clean" or "sanitizer-clean concurrent execution" **cannot be graded on MSVC alone**. Ch10 establishes a dedicated CMake preset, `wsl-clang-tsan`, as required infrastructure before Ch11's concurrency projects are attempted — this is not optional tooling, it's the actual grading gate.

### Build System

CMake ≥ 3.24, using presets (`CMakePresets.json` per project/chapter, once generated):

- `msvc-debug` / `msvc-release` — primary development loop.
- `wsl-clang-debug` / `wsl-gcc-debug` — cross-toolchain correctness check.
- `wsl-clang-asan`, `wsl-clang-ubsan`, `wsl-clang-tsan`, `wsl-clang-msan` — one sanitizer per preset, never combined, since combining sanitizers is often unsupported or misleading.

**Test framework: GoogleTest**, pulled via `FetchContent`, discovered by CTest via `gtest_discover_tests()`. Every chapter's problems that have an objectively checkable answer (as opposed to open-ended design problems) get a GoogleTest case in that chapter's test harness as you write it.

## Build & Test Commands

The root [`CMakeLists.txt`](CMakeLists.txt) and [`CMakePresets.json`](CMakePresets.json) wire up all eight presets below plus GoogleTest via `FetchContent`. A `sandbox/` target with a trivial passing test proves the harness end-to-end before any chapter code depends on it — as you add a chapter or project's own `CMakeLists.txt`, add it as a subdirectory next to `sandbox/` in the root file.

Verified working on MSVC in this repo:

```bash
cmake --preset msvc-debug
cmake --build --preset msvc-debug
ctest --preset msvc-debug --output-on-failure
```

```bash
cmake --preset wsl-clang-tsan
cmake --build --preset wsl-clang-tsan
ctest --preset wsl-clang-tsan --output-on-failure
```

The `wsl-clang-*`/`wsl-gcc-*` presets require `clang`/`g++`/`cmake`/`ninja` installed inside your WSL distro — they are not exercised by this repo's own setup and need to be verified on your machine.

## Difficulty Levels

**Problems** (7 levels, `PROMPT.md`):

| Level | Name |
|---|---|
| 1 | Recognition |
| 2 | Prediction |
| 3 | Implementation |
| 4 | Debugging |
| 5 | Integration |
| 6 | Production |
| 7 | Principal Reasoning |

**Projects** (6 levels, `INSTRUCTIONS.md:265-291`) — see [`PROJECT_ROADMAP.md`](PROJECT_ROADMAP.md) for the full table: Focused component → Multi-concept component → Realistic utility → Systems component → Production-style → Capstone.

## Study Cadence

Suggested pace, not a requirement — see [`PROJECT_ROADMAP.md`](PROJECT_ROADMAP.md)'s Sequencing Recommendation for exactly which projects unlock after which chapter:

- Work chapters roughly in dependency order (see `CURRICULUM.md`'s Dependency Graph — a few chapters can be reordered, most can't).
- Don't bank more than 2 chapters ahead of your projects — the projects are where a concept actually sticks.
- Take a Blind Challenge as soon as it unlocks (BC-1 after Ch03, through BC-5 after Ch12) — they're deliberately concept-blind and lose value if you know exactly what they're testing.
- Revisit the Review Queue in [`PROGRESS.md`](PROGRESS.md) regularly — spaced review chapters contain a real, new problem re-testing an earlier concept, not a repeat of the same one.

## Rules of Engagement

- Don't open `SOLUTIONS.md` (or a project's `SOLUTION.md`) before a genuine attempt. If you must, log it honestly as `◐ Assisted` in `PROGRESS.md` — the entire tracking system is only useful if this is accurate.
- Hints are progressive (4 tiers) and designed to guide reasoning, not hand you the implementation (`INSTRUCTIONS.md:332-346`). Use tier 1 first; reaching for tier 4 immediately defeats the purpose.
- Hidden tests exist for implementation projects specifically to catch "looks done, isn't robust" — passing the visible tests is necessary, not sufficient.
- When a problem allows multiple valid designs, the reference solution is *a* reasonable answer, not *the* answer — see `INSTRUCTIONS.md:87`.

## Status

| Item | State |
|---|---|
| `CURRICULUM.md` | ✅ Generated |
| `CONCEPT_INDEX.md` | ✅ Generated |
| `PROJECT_ROADMAP.md` | ✅ Generated |
| `PROGRESS.md` | ✅ Generated (empty logs — ready to use) |
| `README.md` | ✅ Generated (this file) |
| Chapter 01 — Core Semantics | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 02 — Lifetime, Ownership, RAII | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 03 — Value Categories, Move Semantics, Forwarding | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 04 — The Standard Library: Containers, Iterators, Algorithms | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 05 — Generic Programming: Templates, Concepts, and Compile-Time Design | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 06 — Error Handling and API Failure Design | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 07 — The Object Model: Layout, Polymorphism, and Raw Memory | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 08 — Compilation, Linking, and ABI | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 09 — Systems Programming: Linux and Windows Side by Side | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 10 — Build Systems, Testing Infrastructure, and CI | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 11 — Concurrency and the C++ Memory Model | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 12 — Performance: Memory, Caches, Allocators, and Measurement | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| Chapter 13 — Modern C++ and Architecture: Ranges, Coroutines, Modules, Design | ✅ Generated (`CONCEPTS.md` + `SOLUTIONS.md`) |
| `projects/` content (27 projects + 3 capstones + 1 Grand Project) | ✅ Generated (`STATEMENT.md`/`TESTS.md`/`HINTS.md`/`SOLUTION.md` each) |
| `assessments/` content (BC-1–5, PL-1–4) | ✅ Generated |
| `CMakeLists.txt` / `CMakePresets.json` | ✅ Generated and verified (`msvc-debug`, `msvc-release`) |

**Next step:** everything is generated. Start solving — see [`PROGRESS.md`](PROGRESS.md) for where to begin and how completion is tracked. As you work a chapter or project, give it its own `CMakeLists.txt` and register it as a subdirectory in the root [`CMakeLists.txt`](CMakeLists.txt), following the `sandbox/` target as a template.
