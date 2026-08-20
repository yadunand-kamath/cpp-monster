# CURRICULUM.md — C++ Workbook Architecture

> Authoritative source of truth for structure. `CONCEPT_INDEX.md`, `PROJECT_ROADMAP.md`, and `PROGRESS.md` are derived from this document and must stay consistent with it. Source specs: [`PROMPT.md`](PROMPT.md), [`INSTRUCTIONS.md`](INSTRUCTIONS.md).

---

## 1. Design Principles

1. **20% reference, 80% doing.** Every concept gets a one-line definition, at most a tiny example, then problems. (`PROMPT.md:26-39`)
2. **Teach by doing, not by reading.** The loop is CONCEPT → MENTAL MODEL → PREDICTION → SMALL PROBLEM → IMPLEMENTATION → DEBUGGING → COMBINED PROBLEM → REALISTIC PROJECT. (`PROMPT.md:45-68`)
3. **No solution leakage.** Problems never name the data structure or technique that solves them unless that choice is the exercise. Solutions live in a sibling `SOLUTIONS.md`, never inline. (`INSTRUCTIONS.md:314-328`, deviation #5 below)
4. **Spaced revisit.** Every foundational concept is deliberately re-tested in a later, unrelated-looking context — never taught once and abandoned.
5. **Cross-platform pairing.** Where the OS model genuinely diverges (Linux vs Windows), both are taught side by side in one place rather than duplicated or picked arbitrarily.
6. **Measurement over folklore.** Performance claims are backed by a number from a benchmark harness, not asserted.
7. **Completion is doing, not reading.** No document in this workbook has a "read" checkbox. See `PROGRESS.md`.

**Scale commitment:** ~470 problems across 13 chapters, 27 projects across 6 levels (including 2 mandatory + 1 elective capstone), plus 1 elective Grand Project (a sibling tier to the capstones, not a fourth capstone), 5 Blind Challenges, and 4 Principal-Level design problems. This number exists so "are there enough problems" (`PROMPT.md:860`) is checkable rather than a feeling. If a chapter's nuances demand more to reach the "second nature" bar, problems are added — this is a floor, not a cap.

---

## 2. Master Table of Contents

| Ch | Directory | Title | Scope | Band | Prereqs | Problems | Projects |
|----|-----------|-------|-------|------|---------|----------|----------|
| 01 | `01-core-semantics/` | Core Semantics: Initialization, Const-ness, and Name Binding | How a name acquires a type, a value, and a meaning | L1–L6 | — | 45 | P-1.1 |
| 02 | `02-lifetime-raii/` | Object Lifetime, Ownership, and RAII | Tying resource validity to object lifetime | L1–L6 | 01 | 43 | P-1.2, P-1.4 |
| 03 | `03-value-categories/` | Value Categories, Move Semantics, and Forwarding | The expression taxonomy that decides whether a copy happens | L2–L6 | 01,02 | 40 | P-1.3 |
| 04 | `04-stl/` | The Standard Library: Containers, Iterators, Algorithms | Choosing and correctly using standard containers/algorithms | L1–L6 | 01–03 | 48 | P-2.1 |
| 05 | `05-generic-programming/` | Generic Programming: Templates, Concepts, and Compile-Time Design | Writing code correct for a set of types, and constraining that set | L2–L7 | 01,03,04 | 50 | P-2.3, P-2.5 |
| 06 | `06-error-handling/` | Error Handling and API Failure Design | Deciding how a function reports failure and what the caller is guaranteed | L2–L7 | 02,03,05 | 38 | P-2.4, P-3.5 |
| 07 | `07-object-model/` | The Object Model: Layout, Polymorphism, and Raw Memory | What an object actually is in memory, and what the standard guarantees | L2–L7 | 01–03 | 43 | P-2.2 |
| 08 | `08-compilation-abi/` | Compilation, Linking, and ABI | How translation units become one program, and what breaks when they disagree | L2–L7 | 05,07 | 39 | P-3.1 |
| 09 | `09-systems-programming/` | Systems Programming: Linux and Windows Side by Side | Talking to the OS directly on both platforms | L2–L7 | 02,06,07 | 50 | P-3.2, P-3.4, P-4.1, P-4.6 |
| 10 | `10-build-systems/` | Build Systems, Testing Infrastructure, and CI | Making the project reproducibly buildable, testable, and shippable | L1–L7 | 08 | 35 | P-3.3 |
| 11 | `11-concurrency/` | Concurrency and the C++ Memory Model | Correct, efficient shared-state programming from `jthread` to `memory_order` | L2–L7 | 02,03,06,07,10 | 55 | P-4.2, P-4.3, P-5.3 |
| 12 | `12-performance/` | Performance: Memory, Caches, Allocators, and Measurement | Making speed decisions from measurements, not folklore | L2–L7 | 04,07,10,11 | 41 | P-4.4, P-4.5, P-5.1 |
| 13 | `13-modern-cpp-architecture/` | Modern C++ and Architecture: Ranges, Coroutines, Modules, Design | The last three major language features, then the judgment layer | L3–L7 | all | 41 | P-5.2, P-5.6 |

**Total: 568 problems** (see §9 — actual sums exceed the 470 floor once every chapter is itemized; recorded honestly below rather than trimmed to match the original estimate).

Difficulty band legend: L1 Recognition · L2 Prediction · L3 Implementation · L4 Debugging · L5 Integration · L6 Production · L7 Principal Reasoning. (`PROMPT.md:71-140`)

---

## 3. Chapter Template (canonical)

Resolves the structural conflict between `PROMPT.md:143-244` and `INSTRUCTIONS.md:9-17` (see Deviation #3).

```
NN-topic/
├── CONCEPTS.md
│     ## Crash Course              (one-line def / why it exists / key rule / tiny example, per concept)
│     ## Common Misconceptions      (3-8 items)
│     ## Quick Checks               (short prediction questions, no answers here)
│     ## Problems                   (L1 -> L7, increasing; debugging & implementation
│                                    problems are interleaved by difficulty and tagged
│                                    by type, not segregated into their own section)
│     ## Integration Challenge      (combines this chapter + earlier chapters)
│     ## Chapter Project(s)         (pointer to projects/ — full statement lives there)
│
└── SOLUTIONS.md
      ## Quick Check Answers
      ## Problem Solutions          (Approach / Reference Solution / Explanation /
                                     Why It Works / Common Wrong Approaches / Complexity /
                                     C++ Considerations — per INSTRUCTIONS.md:27-58)
```

**Why debugging problems are interleaved, not segregated:** a section titled "Debugging Problems" tells the learner the code is broken before they read a line of it, which removes most of the diagnostic value — recognizing *that* something is wrong is itself the skill (`PROMPT.md:99-107`). Each problem instead carries a `Type:` tag (`predict` / `implement` / `debug` / `refactor` / `benchmark` / `design`) so the mixture requirement of `PROMPT.md:498-526` is auditable without pre-announcing the answer shape.

**Why solutions live in a sibling file, not chapter-end:** `INSTRUCTIONS.md:17-19` puts solutions at chapter end specifically so the chapter can be attempted without spoiling itself; a long scroll in one file still risks an accidental glance. A separate `SOLUTIONS.md` makes that structurally impossible while satisfying the same intent.

---

## 4. Stable ID Scheme

Join key across all five documents. IDs are immutable once assigned — new material appends, never renumbers.

| Kind | Format | Example |
|---|---|---|
| Problem | `NN-Pnn` | `07-P12` |
| Quick check | `NN-QCn` | `03-QC4` |
| Project | `P-L.n` | `P-4.2` |
| Capstone | `C-n` | `C-1` |
| Blind Challenge | `BC-n` | `BC-3` |
| Principal-Level | `PL-n` | `PL-2` |

---

## 5. Chapter Detail

### Ch01 — Core Semantics: Initialization, Const-ness, and Name Binding

**Scope:** How a name acquires a type, a value, and a meaning — before any resource management enters the picture.

**Concepts Owned:** initialization (default/copy/direct/list/aggregate/narrowing), `const`, `constexpr`, `consteval`, `constinit`, scope, storage duration, references, pointers, `auto`, `decltype`, `decltype(auto)`, overload resolution, implicit conversions, explicit conversions, operator overloading, lambdas (captures, `mutable`, generic lambdas), function objects, `std::function`, `std::invoke`.

**Concepts Referenced But Not Owned:** lifetime (→ Ch02); move semantics (→ Ch03); templates (→ Ch05).

**Prerequisites:** none — entry chapter, assumes basic C++ syntax fluency.

**Problem Distribution:** L1: 8 · L2: 15 · L3: 12 · L4: 6 (interleaved) · L5: 3 · L6: 1 · L7: 0 → **45**

**Chapter Project:** P-1.1 Strong Typedef & Unit-Safe Quantity Type.

**Integration Challenge:** Build a small `constexpr`-evaluable unit-conversion table using operator overloading and overload resolution across implicit/explicit conversions.

**Exit Criteria:** every concept has a one/two-sentence crash-course entry; ≥3 prediction problems on overload resolution; ≥2 debugging problems on narrowing/implicit-conversion surprises; problems increase in difficulty; wording is concise; no forward dependency on lifetime/move.

> Note: `lifetime` is deliberately **not** owned here even though it sits next to `scope`/`storage duration`. Ch01 owns *storage duration* (when memory exists); Ch02 owns *lifetime* (when an object exists). This split is the single highest-value pedagogical cut in the book.

---

### Ch02 — Object Lifetime, Ownership, and RAII

**Scope:** Tying resource validity to object lifetime, and expressing ownership in types.

**Concepts Owned:** constructors, destructors, initialization order, RAII, rule of 0/3/5, ownership, `unique_ptr`, `shared_ptr`, `weak_ptr`, custom deleters, custom resources, exception safety (introduced as a lifetime property), object lifetime.

**Concepts Referenced But Not Owned:** exception guarantees formalized (→ Ch06); move semantics interaction (→ Ch03); control-block layout (→ Ch07).

**Prerequisites:** Ch01.

**Problem Distribution:** L1: 6 · L2: 10 · L3: 12 · L4: 8 · L5: 5 · L6: 2 · L7: 0 → **43**

**Chapter Projects:** P-1.2 Scoped Resource Handle Family; P-1.4 Copy/Move Instrumentation Harness.

**Integration Challenge:** Implement a family of RAII wrappers over a shared "resource registry" and prove no leak/double-free under every construction path (copy, move, exception during construction).

**Exit Criteria:** rule of 0/3/5 tested explicitly for each of "own nothing / own one resource / own a polymorphic resource"; ≥1 debugging problem showing a leak from a hand-rolled destructor; ≥1 problem on `shared_ptr` cycles.

---

### Ch03 — Value Categories, Move Semantics, and Forwarding

**Scope:** The expression taxonomy that determines whether a copy happens, and the machinery that avoids it.

**Concepts Owned:** lvalue, xvalue, prvalue, glvalue, rvalue, temporary materialization, move semantics, copy elision, NRVO, forwarding references, reference collapsing, `std::move`, `std::forward`, move/copy costs (semantics half).

**Concepts Referenced But Not Owned:** measured move/copy cost (→ Ch12); `noexcept` and move selection (→ Ch06, back-referenced here as a stub).

**Prerequisites:** Ch01, Ch02.

**Problem Distribution:** L1: 3 · L2: 16 · L3: 8 · L4: 7 · L5: 4 · L6: 2 · L7: 0 → **40**

**Chapter Project:** P-1.3 `small_vector<T, N>`.

**Integration Challenge:** Given a class hierarchy with a mix of owned resources, predict and then verify (via instrumentation) every copy/move/elision decision the compiler makes across five call-site shapes.

**Exit Criteria:** ≥10 pure prediction problems (this chapter is prediction-heavy by design — L1 is nearly vacuous here); explicit treatment of reference collapsing with worked examples; ≥1 debugging problem on a dangling reference from a forwarding-reference misuse.

---

### Ch04 — The Standard Library: Containers, Iterators, Algorithms

**Scope:** Choosing and correctly using the standard containers, iterators, and algorithms — including when *not* to.

**Concepts Owned:** containers, iterator categories, iterator invalidation, algorithms, `vector`, `deque`, `list`, associative containers, unordered containers, `string` (+ `string_view` lifetime traps), `optional`, `variant`, `any`, ranges (introduction only: `views::filter/transform`, `ranges::sort`).

**Concepts Referenced But Not Owned:** allocator model (→ Ch12; here only as the container's `Allocator` template parameter); lazy view composition and custom views (→ Ch13); `expected` (→ Ch06); type erasure via `any`/`function` (→ Ch05).

**Prerequisites:** Ch01, Ch02, Ch03.

**Problem Distribution:** L1: 6 · L2: 14 · L3: 12 · L4: 8 · L5: 6 · L6: 2 · L7: 0 → **48**

**Chapter Project:** P-2.1 Log Line Indexer.

**Integration Challenge:** Build a multi-index over a dataset using at least two associative/unordered containers and one `ranges` pipeline, with a documented iterator-invalidation-free mutation protocol.

**Exit Criteria:** every container has ≥1 invalidation-specific problem; `string_view` dangling covered explicitly; ranges introduction does not go deeper than lazy filter/transform/sort (deep composition is Ch13's job).

---

### Ch05 — Generic Programming: Templates, Concepts, and Compile-Time Design

**Scope:** Writing code that is correct for a *set* of types, and constraining that set legibly.

**Concepts Owned:** function templates, class templates, specialization, partial specialization, variadic templates, parameter packs, fold expressions, type traits, SFINAE, concepts, constraints, compile-time programming, CRTP, policy-based design, type erasure.

**Concepts Referenced But Not Owned:** ODR/instantiation cost (→ Ch08); code-bloat/build-time tradeoffs (→ Ch10); templated allocators (→ Ch12).

**Prerequisites:** Ch01, Ch03 (forwarding is mandatory), Ch04 (iterator concepts need container fluency).

**Problem Distribution:** L1: 4 · L2: 12 · L3: 14 · L4: 8 · L5: 7 · L6: 3 · L7: 2 → **50**

**Chapter Projects:** P-2.3 `function_ref` + `unique_function`; P-2.5 Compile-Time Reflection-Free Serializer.

**Integration Challenge:** Design a constrained generic algorithm that works over both a hand-rolled type and a standard container, using concepts rather than SFINAE, and show the SFINAE-era equivalent for comparison.

**Exit Criteria:** first chapter with genuine L7 problems (constraint-vocabulary design for a library boundary); CRTP and type erasure each get a dedicated implementation problem; SFINAE is taught but concepts are the default idiom thereafter.

---

### Ch06 — Error Handling and API Failure Design

**Scope:** Deciding how a function reports failure, and what the caller is guaranteed when it does.

**Concepts Owned:** exceptions, stack unwinding, `noexcept` (incl. effect on move selection), exception guarantees (basic/strong/nothrow, formalized), error codes, `std::error_code`/`error_category`, `optional` as error channel, `variant` as error channel, `std::expected` (C++23), error propagation, API error design.

**Concepts Referenced But Not Owned:** exceptions across thread boundaries (→ Ch11); cost of exception tables (→ Ch12); errno vs `GetLastError` unification (→ Ch09, applied).

**Prerequisites:** Ch02 (RAII is the precondition for exception safety), Ch03 (`noexcept` and move), Ch05 (generic error propagation, monadic ops).

**Problem Distribution:** L1: 4 · L2: 8 · L3: 8 · L4: 8 · L5: 5 · L6: 3 · L7: 2 → **38**

**Chapter Projects:** P-2.4 Result/Error Propagation Library; P-3.5 Declarative Command-Line Parser (error-surface design).

**Integration Challenge:** Take a function with three inconsistent error-reporting styles and unify it behind one policy without breaking any existing caller — a smaller rehearsal for BC-2.

**Exit Criteria:** all four exception guarantee levels tested with a debugging problem each; `expected` vs `variant`-as-error-channel tradeoffs explicitly compared; ≥1 problem on `noexcept`'s effect on `vector` reallocation strategy (ties back to Ch03/Ch04).

---

### Ch07 — The Object Model: Layout, Polymorphism, and Raw Memory

**Scope:** What an object actually *is* in memory, and what the standard does and does not guarantee about that.

**Concepts Owned:** object representation, alignment, padding, triviality, standard layout, inheritance, virtual functions, vtables/vptrs, RTTI, multiple inheritance, virtual inheritance, object lifetime rules (`[basic.life]`), aliasing / strict aliasing, `std::launder`, `bit_cast`/`start_lifetime_as`, placement construction, low-level object manipulation.

**Concepts Referenced But Not Owned:** ABI consequences of layout (→ Ch08); atomics on non-trivial types (→ Ch11); false sharing / SoA (→ Ch12).

**Prerequisites:** Ch01, Ch02, Ch03.

**Problem Distribution:** L1: 5 · L2: 12 · L3: 10 · L4: 8 · L5: 5 · L6: 2 · L7: 1 → **43**

**Chapter Project:** P-2.2 SBO Variant Storage (`inplace_any<N>`).

**Integration Challenge:** Implement a small in-place polymorphic buffer that correctly handles alignment, placement construction/destruction, and `launder`, then justify every choice against the `[basic.life]` rules.

**Exit Criteria:** the pivot chapter from "language user" to "implementer" — every implementation problem must reference the specific standard rule it satisfies; ≥1 strict-aliasing UB-finding problem; vtable layout shown concretely (not just described) for single and multiple inheritance.

> This chapter must precede Ch08 (ABI is object layout crossing a binary boundary), Ch11 (atomics need a representation model), and Ch12 (data layout / false sharing are layout consequences). It could topologically go as early as Ch04 but is placed at 07 deliberately — it lands with far more motivation after `vector<bool>`, SBO, and `variant` storage surprises from Ch04, and after `noexcept`-driven move selection from Ch06.

---

### Ch08 — Compilation, Linking, and ABI

**Scope:** How translation units become one program, and what breaks when two of them disagree.

**Concepts Owned:** preprocessing, translation units, declarations, definitions, ODR (incl. silent ODR-violation UB), `inline` (functions and variables), templates and instantiation (`extern template`, vague linkage), symbol resolution, name mangling, static libraries, shared libraries, dynamic linking, ABI, binary compatibility, pimpl / export-boundary techniques.

**Concepts Referenced But Not Owned:** target-graph modeling of this material (→ Ch10); runtime dynamic loading (→ Ch09, applied); module ODR semantics (→ Ch13).

**Prerequisites:** Ch05 (template instantiation drives most ODR/bloat problems), Ch07 (layout is ABI).

**Problem Distribution:** L1: 5 · L2: 10 · L3: 8 · L4: 8 · L5: 4 · L6: 2 · L7: 2 → **39**

**Chapter Project:** P-3.1 Binary Object File Inspector (ELF + PE).

**Integration Challenge:** Given two shared libraries built with mismatched flags/ABI assumptions, diagnose the resulting crash from symbol/layout evidence alone.

**Exit Criteria:** MSVC-first tooling (`dumpbin /symbols /exports /dependents`, `__declspec(dllexport/dllimport)`, `.def` files, MSVC mangling), with GCC/Clang tooling (`nm -C`, `objdump -T`, `readelf -d`, Itanium mangling, `-fvisibility=hidden`) as secondary in every relevant problem; ≥2 problems requiring actual `dumpbin`/`nm` output to be read.

---

### Ch09 — Systems Programming: Linux and Windows Side by Side

**Scope:** Talking to the operating system directly, and understanding what the C++ standard library is hiding — on both platforms at once.

**Concepts Owned:** processes, threads (OS-level realization of `std::thread`), virtual memory, system calls, file descriptors, files, sockets, TCP, memory mapping, shared-library runtime loading, signals, debugging tools.

**Structure — paired sections.** Each concept: one concept statement → Linux realization → Windows realization → divergence note. Six mandated pairings:

| Concept | Linux | Windows | Key Divergence |
|---|---|---|---|
| Kernel object handles | `int` fd, `open`/`read`/`close` | `HANDLE`, `CreateFile`/`ReadFile`/`CloseHandle` | fd is a small int, inherited by default (+`O_CLOEXEC`); HANDLE is opaque, inheritance is explicit via `bInheritHandle` |
| Memory mapping | `mmap`/`munmap`/`mprotect`/`madvise` | `CreateFileMapping`/`MapViewOfFile`, `VirtualAlloc` reserve-vs-commit | Linux overcommits and conflates reserve/commit; Windows separates them and charges commit against the pagefile |
| Async signalling / interruption | `signal`/`sigaction`, async-signal-safety, `EINTR` | SEH, vectored exception handlers, `SetConsoleCtrlHandler` | POSIX signals interrupt in-flight syscalls; Windows has no equivalent — SEH is stack-based unwind, not an interrupt |
| Scalable I/O readiness | `epoll` (level/edge readiness) | IOCP (completion) | Readiness vs completion model — the structural difference driving event-loop design |
| Dynamic loading | ELF `.so`, `dlopen`/`dlsym`, `RTLD_*`, `RPATH` | PE `.dll`, `LoadLibrary`/`GetProcAddress`, DLL search order, `DllMain` reentrancy rules | Global symbol interposition vs per-module import tables |
| Inspection / observability | `ptrace`, gdb, `strace`, `perf` | WinDbg, ETW, Process Monitor, `!analyze` | Single-process attach vs system-wide event tracing |

**Prerequisites:** Ch02 (RAII is how you own an fd/HANDLE), Ch06 (errno vs `GetLastError` is an error-channel design problem), Ch07 (mmap'd bytes → objects needs the lifetime rules).

**Problem Distribution:** L1: 6 · L2: 10 · L3: 14 · L4: 8 · L5: 6 · L6: 4 · L7: 2 → **50** (largest chapter; roughly half of L3+ problems require both platforms or an explicit portability abstraction)

**Chapter Projects:** P-3.2 Cross-Platform File Watcher; P-3.4 Content-Addressed Duplicate File Detector; P-4.1 Single-Threaded Event Loop; P-4.6 Process Supervisor.

**Integration Challenge:** Wrap one concept pair (learner's choice) behind a single portable interface and prove behavioral equivalence with a test suite that runs unmodified on both platforms.

**Exit Criteria:** every one of the six pairings has ≥1 problem exercising *both* sides, not just one; ≥1 problem where the "same" POSIX and Win32 error surfaces genuinely different failure information.

---

### Ch10 — Build Systems, Testing Infrastructure, and CI

**Scope:** Making the project reproducibly buildable, testable, and shippable on two toolchains.

**Concepts Owned:** CMake, targets, `PRIVATE`/`PUBLIC`/`INTERFACE`, generator expressions, toolchains, dependency management (FetchContent, `find_package`, presets), testing (GoogleTest + CTest wiring), installation, packaging, CI, testing (general practice), fuzzing, sanitizers.

**Concepts Referenced But Not Owned:** target-graph as applied to a real ODR/ABI problem (→ Ch08, prerequisite direction); observability/maintainability as design axes (→ Ch13).

**Prerequisites:** Ch08 (you cannot reason about a target graph without understanding linking).

**Problem Distribution:** L1: 5 · L2: 6 · L3: 10 · L4: 6 · L5: 4 · L6: 3 · L7: 1 → **35**

**Chapter Project:** P-3.3 Reusable Library Template & Test Harness — its output becomes the harness every later project is submitted against.

**Integration Challenge:** Take a two-target library (static + shared) with a leaking `PUBLIC` dependency and fix the target graph so consumers only see what they should.

**Exit Criteria:** MSVC primary (`/std:c++20`, `/std:c++latest`, `/W4 /permissive- /Zc:preprocessor`, `/fsanitize=address`); GCC/Clang via WSL secondary; **explicit, documented statement that TSan and MSan are unavailable under MSVC**, with a `wsl-clang-tsan` CMake preset defined and exercised by at least one problem — this is the gate the Concurrency chapter depends on, not an afterthought.

---

### Ch11 — Concurrency and the C++ Memory Model

**Scope:** Correct and efficient shared-state programming, from `jthread` down to `memory_order_relaxed`.

**Concepts Owned:** threads, `jthread`/`stop_token`, mutexes, locks, condition variables, futures, promises, `async`, atomics, data races, happens-before, synchronization, memory ordering, lock-free programming, thread pools, work queues, producer/consumer systems, deadlocks, starvation, livelock, false sharing (correctness angle), `latch`/`barrier`/`semaphore`.

**Concepts Referenced But Not Owned:** false sharing measurement (→ Ch12); scheduling as a coroutine substrate (→ Ch13).

**Prerequisites:** Ch02, Ch03, Ch06 (exception propagation across threads), Ch07 (atomics on non-trivial types), Ch10 (TSan harness is mandatory to grade these).

**Problem Distribution:** L1: 5 · L2: 14 · L3: 12 · L4: 10 · L5: 7 · L6: 4 · L7: 3 → **55** (largest problem count; L4 debugging block is intentionally oversized — concurrency bugs are the highest-value debugging practice in the book)

**Chapter Projects:** P-4.2 Bounded MPMC Queue; P-4.3 Work-Stealing Thread Pool; P-5.3 Concurrent TCP Protocol Server.

**Integration Challenge:** Diagnose and fix a producer/consumer system that deadlocks under load roughly once per ten thousand runs, using only the TSan harness and reasoning about happens-before — no source-level guessing.

**Exit Criteria:** every problem claiming thread-safety must be checked in the `wsl-clang-tsan` preset per its own acceptance criterion; memory-ordering problems span all six orders, not just `seq_cst`/`relaxed`; ≥3 problems are genuinely non-deterministic bugs requiring repeated runs to reproduce.

---

### Ch12 — Performance: Memory, Caches, Allocators, and Measurement

**Scope:** Making informed speed decisions from measurements rather than folklore.

**Concepts Owned:** algorithmic complexity (as a measured property), allocations, memory locality, cache behavior, branch prediction, data layout (AoS/SoA), false sharing (measured), profiling, benchmarking, compiler optimization, inlining, vectorization, move/copy costs (measured), allocators (the model: `std::pmr`, arenas, pools), object pools, arenas.

**Concepts Referenced But Not Owned:** none owned elsewhere leak in here; this chapter is the terminal owner of "allocators" as a model (Ch04 only exposes the template parameter, Ch07 supplies the alignment/placement-new prerequisite).

**Prerequisites:** Ch04, Ch07, Ch10 (benchmark harness), Ch11 (false sharing, contention).

**Problem Distribution:** L1: 4 · L2: 8 · L3: 10 · L4: 6 · L5: 6 · L6: 4 · L7: 3 → **41**

**Rule for this chapter: no L3+ problem is complete without a number.** Every implementation/debugging/design problem beyond L2 requires a benchmark artifact as part of the expected outcome.

**Chapter Projects:** P-4.4 Arena & Pool Allocator Suite; P-4.5 Concurrent Sharded Cache; P-5.1 Allocator & Container Benchmark Harness.

**Integration Challenge:** Given a profile showing an unexpected allocation hotspot, cut allocation count without changing the public API, and prove the improvement with before/after benchmark numbers (a smaller rehearsal for BC-1).

**Exit Criteria:** false sharing shown both as a correctness risk (Ch11) and a measured cost (here) with the same code; ≥1 problem where the "obvious" optimization makes things slower and the benchmark is what reveals it.

---

### Ch13 — Modern C++ and Architecture: Ranges, Coroutines, Modules, Design

**Scope:** The last three major language features, then the judgment layer that decides when to use any of it.

**Concepts Owned:** ranges (deep: lazy composition, custom views, `view_interface`, dangling/borrowed ranges), views, coroutines, `co_await`, `co_yield`, coroutine frames (allocation, HALO, custom promise types, awaiter protocol, symmetric transfer), modules (and the current MSVC-vs-Clang maturity gap), modern error handling (synthesis pass over Ch06), API design, ownership design, dependency management, coupling, cohesion, abstraction, testing (as a design axis), observability, backwards compatibility, maintainability, architectural trade-offs.

**Concepts Referenced But Not Owned:** none — this is the terminal chapter; everything here is either new (coroutines/modules/deep ranges) or a synthesis of prior chapters.

**Prerequisites:** all of Ch01–Ch12.

**Problem Distribution:** L1: 2 · L2: 6 · L3: 8 · L4: 5 · L5: 6 · L6: 6 · L7: 8 → **41** (weighted to L6/L7 — this chapter is mostly design)

**Chapter Projects:** P-5.2 Coroutine Task & Generator Library; P-5.6 Plugin Host with a Stable C ABI Boundary.

**Integration Challenge:** Design a coroutine-based `task<T>` scheduled onto the Ch11 thread pool, exposing an API stable enough to survive an internal rewrite — requires ranges, coroutines, ownership design, and ABI stability judgment simultaneously.

**Exit Criteria:** modules covered with an explicit, dated note on MSVC/Clang support maturity (do not imply universal production-readiness); every architecture concept is exercised via a project decision, not a definition quiz; this chapter feeds directly into the capstones and the Principal-Level problems.

---

## 6. Dependency Graph

```
01 --+--> 02 --+--> 03 --+--> 04 --+--> 05 --+--> 06 --+
     |         |         |        |         |         |
     +---------+---------+---> 07 +---------+         |
                               |                        |
                               +--> 08 --> 10 ----------+
                               |                        |
                        02,06,07 --> 09                 |
                                                        |
                    02,03,06,07,10 --> 11 --------------+
                                                        |
                  04,07,10,11 --> 12 --------------------+
                                                        |
                          all --> 13 <------------------+
```

**Topological justification:**

1. **01→02→03 is forced.** Destructors are unteachable without storage duration; move semantics is only interesting because it changes *who destroys what* — teaching move before RAII produces the broken mental model "move = fast copy."
2. **03 before 04.** Iterator invalidation, `emplace_back` vs `push_back`, and `string_view` dangling are all unteachable without value categories first.
3. **04 before 05.** Concepts are most legible written against containers/iterators the learner already knows (`std::ranges::range`, `sortable`); templates taught abstractly-first produce syntax without judgment.
4. **05 before 06.** Modern error handling (`expected`, monadic `and_then`) is generic code; `noexcept` interacts with template-deduced move operations.
5. **07 sits after 03** rather than as early as it could topologically go (Ch04), because it is far more motivating after `vector<bool>`, SBO, and `variant` storage surprises, and after `noexcept`-driven move selection.
6. **08 before 10.** CMake's `PRIVATE`/`PUBLIC`/`INTERFACE` is literally a model of the include/link graph; teaching CMake first produces cargo-culting.
7. **10 before 11, 12.** Infrastructure gate — TSan/ASan/CTest/benchmark harness must exist before any problem can be graded on "no data races" or "sustains 100k ops/sec."
8. **09 sits before 10/11 but off the critical path** (only needs 02/06/07). It gives concrete OS grounding — what a thread and virtual memory actually *are* — right before the chapters where that grounding pays off most.
9. **13 last, by construction.** Coroutines need lifetime (02), value categories (03), type erasure (05), exception propagation (06), allocation (12); architecture needs everything.

---

## 7. Spaced Review Schedule

Contractual: every listed revisit chapter must contain at least one problem re-testing the concept in a new context. `CONCEPT_INDEX.md`'s **Review Checkpoint** column mirrors this table exactly.

| Concept (first taught) | Revisit Chapters | What the Revisit Adds |
|---|---|---|
| RAII (02) | 06, 09, 11, 12 | exception guarantees; owning fd/HANDLE; lock guards & `jthread`; arena scope guards |
| Ownership (02) | 05, 08, 11, 13 | ownership in generic interfaces; ownership across DLL boundaries; ownership across threads; ownership as an API-design axis |
| `shared_ptr` (02) | 07, 11, 12 | control-block layout & `enable_shared_from_this`; atomic refcount cost & `atomic<shared_ptr>`; refcount contention measured |
| Move semantics (03) | 04, 05, 06, 12 | container reallocation & `move_if_noexcept`; perfect forwarding in factories; `noexcept` and the strong guarantee; measured move-vs-copy cost |
| Copy elision / NRVO (03) | 08, 12 | ABI return-slot conventions; observing elision in disassembly |
| Iterator invalidation (04) | 07, 11, 12 | as an object-lifetime-rule consequence; concurrent invalidation; invalidation vs. stable-address allocators |
| `variant`/`optional` (04) | 06, 07, 13 | as error channels; SBO storage & layout; as sum types in API design |
| Ranges (04, intro) | 13 | lazy composition, custom views, dangling ranges |
| Templates/instantiation (05) | 08, 10, 12 | ODR & vague linkage, code bloat; build-time cost & `extern template`; instantiation vs inlining |
| Type erasure (05) | 07, 11, 13 | vtable vs. manual dispatch table; type-erased tasks in a thread pool; type erasure as an ABI-stability tool |
| Concepts (05) | 06, 13 | constraining error-propagation interfaces; concepts as documented API contracts |
| Exception safety (06) | 09, 11, 12 | failure across syscall boundaries; exceptions across thread boundaries & `future`; cost of EH tables/`-fno-exceptions` |
| `noexcept` (06) | 03 (stub back-ref), 11, 12 | move selection; destructor/`terminate` semantics in threads; optimization impact |
| Alignment/padding (07) | 09, 11, 12 | mmap'd struct layout & endianness; `hardware_destructive_interference_size`; SoA and cache-line packing |
| Virtual dispatch (07) | 08, 12, 13 | vtable ABI & fragile base class; devirtualization & indirect-branch cost; when polymorphism is the wrong abstraction |
| Placement new / lifetime rules (07) | 09, 11, 12 | objects in mapped memory; lock-free node reuse & ABA; arena construction |
| ABI (08) | 09, 10, 13 | `.so`/`.dll` export surfaces; symbol visibility in CMake install/export; backwards compatibility policy |
| ODR (08) | 10, 13 | ODR violations from mismatched build flags; module vs header ODR semantics |
| Virtual memory (09) | 12 | page faults, TLB, huge pages, and why arenas help |
| epoll/IOCP (09) | 11, 13 | integrating a reactor with a thread pool; wrapping IOCP in a coroutine awaiter |
| Data races (11) | 12, 13 | false-sharing measurement; designing away shared state |
| Memory ordering (11) | 12, 13 | cost of `seq_cst` vs `acquire/release`; when relaxed is an unjustifiable risk |
| Thread pool (11) | 12, 13 | scheduling & queue-contention benchmarks; as a coroutine scheduler |
| Allocators (04 param → 12 model) | 13 | allocator-awareness as an API-design and ABI decision |

---

## 8. Coverage Matrix

Every concept group and bullet from `PROMPT.md:254-494` mapped to its owning chapter, so a gap is visibly impossible. "Revisit" repeats the chapters from §7 for cross-check convenience.

| Coverage Group (PROMPT.md) | Owning Ch | Revisit Ch(s) |
|---|---|---|
| Language & Core Semantics (init, const/constexpr/consteval/constinit, scope, refs, pointers, auto/decltype, overload resolution, conversions, operator overloading, lambdas, function objects, `std::function`/`invoke`) | 01 | — |
| Object Lifetime & Resource Management (ctors/dtors, init order, RAII, rule of 0/3/5, ownership, smart pointers, custom deleters/resources, exception safety) | 02 | 06, 09, 11, 12 |
| Value Categories (lvalue…rvalue, temporary materialization, move semantics, copy elision, NRVO, forwarding refs, reference collapsing, `move`/`forward`) | 03 | 04, 05, 06, 08, 12 |
| STL (containers, iterator categories/invalidation, algorithms, allocators [param only], vector/deque/list, associative/unordered, string, smart pointers [xref 02], optional/variant/any/function, ranges [intro]) | 04 | 07, 11, 12, 13 |
| Generic Programming (function/class templates, specialization, partial specialization, variadic templates, parameter packs, fold expressions, type traits, SFINAE, concepts, constraints, compile-time programming, CRTP, policy-based design, type erasure) | 05 | 06, 07, 08, 10, 12, 13 |
| Error Handling (exceptions, stack unwinding, `noexcept`, exception guarantees, error codes, optional/variant/expected, error propagation, API error design) | 06 | 03 (stub), 09, 11, 12 |
| Object Model (object representation, alignment, padding, triviality, standard layout, inheritance, virtual functions, vtables/vptrs, RTTI, multiple/virtual inheritance, object lifetime rules, aliasing, placement construction, low-level manipulation) | 07 | 08, 09, 11, 12, 13 |
| Compilation & Linking (preprocessing, TUs, decls/defs, ODR, inline, templates/instantiation, symbol resolution, name mangling, static/shared libs, dynamic linking, ABI, binary compatibility) | 08 | 09, 10, 13 |
| Build Systems (CMake, targets, PRIVATE/PUBLIC/INTERFACE, generator expressions, toolchains, dependency mgmt, testing, installation, packaging, CI) | 10 | 11, 12 (as infrastructure) |
| Linux & Systems (processes, threads[OS], virtual memory, syscalls, fds, files, sockets, TCP, memory mapping, shared libs [runtime], signals, debugging tools) — **paired with Windows equivalents per §5 Ch09 table** | 09 | 11, 12, 13 |
| Concurrency (threads, jthread, mutexes, locks, condition variables, futures/promises/async, atomics, data races, happens-before, synchronization, memory ordering, lock-free, thread pools, work queues, producer/consumer, deadlocks/starvation/livelock, false sharing [correctness]) | 11 | 12, 13 |
| Performance (algorithmic complexity [measured], allocations, memory locality, cache behavior, branch prediction, data layout, false sharing [measured], profiling, benchmarking, compiler optimization, inlining, vectorization, move/copy costs [measured], allocators [model], object pools, arenas) | 12 | 13 |
| Modern C++ (ranges [deep], views, concepts [xref 05], coroutines, co_await/co_yield, coroutine frames, modules, modern error handling [synthesis]) | 13 | — (terminal) |
| Engineering (API design, ownership design, dependency mgmt, coupling, cohesion, abstraction, testing, fuzzing, sanitizers, observability, backwards compatibility, maintainability, architectural trade-offs) | 13 (design axes) / 10 (testing, fuzzing, sanitizers as infrastructure) | applied via project acceptance criteria, every L4+ project |

**Resolved duplicate listings** (concepts appearing in more than one PROMPT.md group):

- **Concepts** — appears under both Generic Programming and Modern C++. Single owner: Ch05. Modern C++'s "concepts" line is satisfied by Ch05 with a Ch13 cross-reference for how concepts show up in coroutine/ranges constraints.
- **Ranges** — appears under both STL and Modern C++. Split ownership by design, not duplication: Ch04 owns the introduction (lazy `filter`/`transform`/`sort`), Ch13 owns the deep material (custom views, `view_interface`, dangling/borrowed ranges).
- **Allocators** — appears under STL, Performance, and implicitly Object Model. Single model-owner: Ch12. Ch04 exposes only the container's `Allocator` template parameter; Ch07 supplies the alignment/placement-new prerequisite Ch12's arenas need.
- **Smart pointers** — appears under both Object Lifetime and STL. Single owner: Ch02 (this is where ownership semantics belong); Ch04's STL section cross-references rather than re-teaches.
- **False sharing** — appears under both Concurrency and Performance. Split by aspect, not duplicated: Ch11 teaches it as a *correctness/contention* risk, Ch12 teaches the same code *measured*.

---

## 9. Scale Check

| Ch | Problems |
|---|---|
| 01 | 45 |
| 02 | 43 |
| 03 | 40 |
| 04 | 48 |
| 05 | 50 |
| 06 | 38 |
| 07 | 43 |
| 08 | 39 |
| 09 | 50 |
| 10 | 35 |
| 11 | 55 |
| 12 | 41 |
| 13 | 41 |
| **Total** | **568** |

Plus ~65 Quick Checks total (not counted as Problems), 27 projects, 3 capstones (2 mandatory + 1 elective), 1 Grand Project (elective, sibling tier to the capstones), 5 Blind Challenges, 4 Principal-Level problems. 568 exceeds the ~470 floor — intentional, per the user's directive that thoroughness (covering every nuance and exception) outranks hitting a round number. Individual chapters may grow further during generation if a concept turns out to need more drilling; this table is updated whenever that happens.

---

## 10. Assessments (`assessments/`)

Concepts are deliberately unnamed in all of the below, per `PROMPT.md:698-712` and `PROMPT.md:714-735`.

### Blind Challenges

| ID | Placement | Prompt |
|---|---|---|
| BC-1 | after Ch03 | "Cut this pipeline's allocation count 10x without changing its public API." |
| BC-2 | after Ch06 | "This library reports errors three inconsistent ways. Unify it. The public API may not break." |
| BC-3 | after Ch09 | "This service leaks handles under load — on Windows only." |
| BC-4 | after Ch11 | "This queue passes 10,000 test runs and fails in production once a week." |
| BC-5 | after Ch12 | "This code got 3x slower after a compiler upgrade. The source did not change." |

### Principal-Level Design Problems

| ID | Prompt |
|---|---|
| PL-1 | Design a high-throughput task execution service. |
| PL-2 | Design a storage engine for a write-heavy time-series workload. |
| PL-3 | Design the ABI and versioning policy for a C++ library shipped to external customers on two platforms for five years. |
| PL-4 | "Our C++ build takes 40 minutes. Design the fix." *(deliberately under-specified)* |

Each is evaluated with a rubric (API, ownership, concurrency, failure, backpressure, memory, observability, testing, scalability, performance — `PROMPT.md:722-733`), never a single reference solution.

---

## 11. Generation Order & Status

Per `PROMPT.md:883-896`: planning docs first, then chapters one at a time with a quality review after each.

| Artifact | Status |
|---|---|
| `README.md` | generated this pass |
| `CURRICULUM.md` | generated this pass (this file) |
| `CONCEPT_INDEX.md` | generated this pass |
| `PROJECT_ROADMAP.md` | generated this pass |
| `PROGRESS.md` | generated this pass |
| `01-core-semantics/` … `13-modern-cpp-architecture/` | not started |
| `projects/level-1/` … `level-5/`, `capstones/`, `grand-projects/` | not started |
| `assessments/` | not started |

Recommended next step: generate Chapter 01 (`CONCEPTS.md` + `SOLUTIONS.md`), then run the quality review in `PROMPT.md:856-869` before continuing to Chapter 02.

---

## 12. Deviations from the Source Specs (recorded, not silent)

| # | Conflict | Resolution |
|---|---|---|
| 1 | INSTRUCTIONS.md defines 6 project levels; PROMPT.md's tree shows only `level-1..4` + `capstones` | Follow INSTRUCTIONS.md: `projects/level-1/` … `level-5/` + `capstones/` |
| 2 | `PROJECT_ROADMAP.md` is required by PROMPT.md:889-892 but absent from its own file tree at PROMPT.md:785-812 | Added at repo root alongside the other four planning docs |
| 3 | PROMPT.md's chapter format (Crash Course → Misconceptions → Quick Checks → Problems → Debugging → Implementation → Integration) conflicts with INSTRUCTIONS.md's (Crash Course → Quick Checks → Problems → Debugging → Integration → Project → Solutions) | Union: Crash Course → Common Misconceptions → Quick Checks → Problems (L1-L7, debugging/implementation interleaved and type-tagged) → Integration Challenge → Chapter Project(s) → Solutions |
| 4 | PROMPT.md says Quick Checks get no immediate answers; INSTRUCTIONS.md says every problem gets a chapter-end solution | Quick Check answers live in `SOLUTIONS.md` under a distinct "Quick Check Answers" section, separate from problem solutions |
| 5 | Even chapter-end solutions risk accidental spoilage on scroll | Solutions moved to a sibling `SOLUTIONS.md` file entirely, not the bottom of the chapter file |
| 6 | No chapter obviously owns testing/fuzzing/sanitizers/observability/backwards-compatibility from PROMPT.md's Engineering group | testing/fuzzing/sanitizers → Ch10 (as infrastructure); observability/backwards-compat/maintainability/API design → Ch13 (as design axes); both applied via mandatory acceptance criteria on every L4+ project rather than taught as prose alone |
| 7 | PROMPT.md's "Linux & Systems" coverage list is Linux-only; user directed both Linux and Windows systems topics with an MSVC-primary toolchain | Ch09 redesigned as one chapter with six mandated Linux/Windows pairings; coverage matrix maps every Linux-named concept to both realizations |
| 8 | No scale target given, making "enough problems" (PROMPT.md:860) unverifiable | Committed to 568 problems / 27 projects / 3 capstones in this document (§9), with the explicit rule that thoroughness may push the number higher |
| 9 | `assessments/` directory exists in PROMPT.md's tree with no content spec | Populated with 5 Blind Challenges (BC-1..5) and 4 Principal-Level problems (PL-1..4), each carrying a rubric instead of a solution |
| 10 | "Concepts" and "Ranges" each appear in two PROMPT.md coverage groups | Single ownership assigned: Concepts → Ch05; Ranges split intro (Ch04) / deep (Ch13) |
| 11 | "Allocators" appears in three PROMPT.md coverage groups (STL, Performance, implicitly Object Model) | Single model-owner Ch12; Ch04 exposes only the template parameter; Ch07 supplies the layout prerequisite |
| 12 | No stable ID scheme specified, yet four documents must cross-reference each other | Adopted the immutable scheme in §4 |
| 13 | Spec's `09-linux-build` directory bundles OS APIs and CMake, two unrelated skills, into an already-heaviest chapter | Split into `09-systems-programming` (OS APIs, paired) and `10-build-systems` (CMake/CTest/CI), with 10 placed before 11/12 as an infrastructure gate |
| 14 | Spec's `12-modern-cpp` and `13-architecture` overlap heavily once Concepts/Ranges are reassigned above | Merged into a single `13-modern-cpp-architecture` chapter; still 13 chapters total |
