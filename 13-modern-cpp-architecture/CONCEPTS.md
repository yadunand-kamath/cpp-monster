# Chapter 13 — Modern C++ and Architecture: Ranges, Coroutines, Modules, Design

## Crash Course

### Ranges, Deep: Lazy Composition and Views

A range adaptor (`std::views::filter`, `std::views::transform`, `std::views::take`, ...) does not produce a container — it produces a lightweight *view*: an object that wraps its underlying range and computes each element on demand, when an iterator over the view is actually dereferenced. `v | views::filter(pred) | views::transform(f)` builds nothing until iterated; each `*it` call runs `pred` and `f` for exactly that one element, no more. This is why chaining five adaptors over a million-element vector costs one pass, not five — the composition is lazy, not eagerly materialized at each stage.

A **borrowed range** is one whose iterators remain valid after the range object itself is destroyed — true for `T&` and for ranges specifically marked as borrowed (e.g. `std::span`, `std::string_view`, `views::all` of a range you still own elsewhere). A **dangling range** results from adapting a temporary: `views::filter(make_vector(), pred)` returns iterators into a `vector` that no longer exists once the full expression ends, and the range library detects this at compile time for functions returning `views::all(temporary)`-style constructs, yielding `std::ranges::dangling` instead of a usable iterator — a compile error, not a runtime UB surprise, precisely because the library tracks borrowed-ness in the type system.

Writing a custom view means inheriting from `std::ranges::view_interface<YourView>`, which supplies `empty()`, `size()` (if applicable), `front()`, `back()`, and `operator[]` automatically from just `begin()`/`end()` — you write the iterator, the view interface writes the rest of the range's ergonomics.

### Coroutines: `co_await`, `co_yield`, and the Frame

A function containing `co_await`, `co_yield`, or `co_return` becomes a coroutine — the compiler transforms it into a state machine backed by a **coroutine frame**: heap-allocated (usually) storage holding the function's parameters, local variables that survive a suspension point, and a resume point. Calling a coroutine does not run its body to completion; it runs up to the first suspension point and returns a handle (wrapped in whatever type the coroutine returns, e.g. a custom `task<T>`) to the caller.

Every coroutine's behavior is defined by its **promise type** (found via `std::coroutine_traits`), which supplies `initial_suspend()`, `final_suspend()`, `return_value()`/`return_void()`, `unhandled_exception()`, and (for generators) `yield_value()`. `co_await expr` requires `expr` to be — or convert to — an **awaiter**: an object with `await_ready()`, `await_suspend(handle)`, and `await_resume()`, the three-method protocol that decides whether to suspend at all, what to do while suspended (e.g. register a callback, schedule work), and what value `co_await` itself evaluates to on resumption.

**HALO** (Heap Allocation eLision Optimization) lets the compiler prove a coroutine frame's lifetime is provably nested inside its caller and elide the heap allocation entirely, allocating the frame on the caller's stack instead — but this is an optimization the compiler is *permitted*, not *required*, to perform, and it typically requires the whole call chain to be visible for inlining (no virtual calls, no separate translation units without LTO) — never assume HALO happened without checking generated code or a profiler.

**Symmetric transfer** is the technique of resuming another coroutine directly from `await_suspend`, by returning a `coroutine_handle` from it rather than calling `.resume()` yourself — this avoids growing the call stack by one frame per chained `co_await`, which matters because a long chain of naive, non-symmetric-transfer suspensions can overflow the stack; symmetric transfer instead becomes a tail call the compiler can turn into a jump.

Custom allocators for coroutine frames are supported via a promise type's `operator new`/`operator delete` — letting a coroutine-heavy system (e.g. thousands of in-flight `task<T>`s) route frame allocation through an arena or pool from Ch12 instead of the default global allocator.

### Modules

`export module foo;` / `import foo;` replace textual `#include`-based composition with a compiled, binary interface unit — the compiler parses a module's interface once and reuses that parsed representation for every importer, in principle eliminating the repeated-parsing cost that headers impose and removing macro leakage across translation-unit boundaries entirely (a module's macros do not leak to its importers unless explicitly exported, which is itself restricted).

**As of this writing, module tooling support is uneven and evolving.** MSVC (Visual Studio 2022, sufficiently recent updates) has the most mature practical support for standard modules in mixed module/header codebases. Clang and GCC support has improved significantly but build-system integration (CMake's `CXX_MODULES` support, dependency-scanning for incremental builds) remains newer and less battle-tested than header-based toolchains across all three compilers as of C++20/23 adoption. **Do not treat modules as a universally production-ready drop-in replacement for headers** — evaluate current toolchain support against your specific build system before committing a large codebase to a modules-only structure; this chapter treats modules as a language feature to understand and use in isolated, controlled contexts, not as a currently-safe default for a mixed-toolchain production codebase.

### Modern Error Handling, Synthesized

Ch06 established the mechanics: exceptions for exceptional control flow with automatic propagation and RAII-based cleanup; `std::expected<T,E>` for expected, recoverable failure with an explicit, visible-at-the-call-site error channel; error codes for C-boundary and legacy interop; `std::optional<T>` for "absent," which is not itself an error. The architectural question this chapter adds is *which one, at which layer* — an internal, deeply-nested computation typically prefers `expected` (cheap, composable failure with no stack-unwinding cost), a public library boundary crossing into caller code with heterogeneous handling needs and possibly exceptions-disabled builds should document and pick deliberately rather than default to whatever the implementation happened to use internally, and a genuinely exceptional, rare, unrecoverable-at-this-layer condition (out of memory, a broken invariant) still legitimately warrants an exception specifically because forcing every caller up the chain to check a return value for a condition they cannot locally act on adds real cost and noise without adding real safety.

### API Design, Ownership Design, and Coupling

A public API's *stability* obligation is to its **observable contract** — signatures, preconditions, postconditions, and documented behavior — not to its implementation. An internal rewrite that changes a data structure, an algorithm, or an allocation strategy is invisible to correctly-written callers exactly when the observable contract does not move; this is the same discipline exercised at smaller scale throughout this workbook (Ch10's benchmark-harness internals, Ch12's allocation-hotspot fix) and is now the organizing principle of an entire library's evolution over years.

**Ownership design** at the API level means choosing, for every value crossing a boundary, one of: caller retains ownership (pass by reference/pointer, callee must not outlive the call), callee takes ownership (pass by value or `unique_ptr`, caller must not use it after), or shared ownership genuinely required by the domain (`shared_ptr`, with the real cost of atomic refcounting accepted because the alternative — an unclear or undocumented ownership story — is worse). Reaching for `shared_ptr` because ownership wasn't thought through is a design smell, not a safety net.

**Coupling** (how much one module knows about another's internals) and **cohesion** (how tightly a module's own responsibilities belong together) trade off directly against flexibility and comprehensibility — high cohesion and low coupling is the target, but it is a spectrum decision made against a specific set of expected future changes, not a rule applied uniformly; over-decoupling a module that will never independently vary from its one caller adds real indirection cost for a flexibility that's never exercised.

### Testing, Observability, and Backwards Compatibility as Design Axes

Testability is a *design* property, not something added after the fact — a function that hardcodes `std::chrono::system_clock::now()` or reaches directly into a global singleton cannot be unit-tested for time-dependent or state-dependent behavior without a design change (injecting a clock, injecting the dependency) made *before* the test is attempted, not after. Observability (structured logging, metrics, tracing hooks) has the same property: a system designed without any seams for inserting instrumentation cannot have it bolted on cheaply later. Backwards compatibility is a standing constraint, not a one-time migration: a public library's versioning policy (semantic versioning, ABI-stability windows, deprecation cycles) determines what "done" means for every subsequent change, and that policy must be decided deliberately, in writing, rather than emerging accidentally from whatever the first release happened to promise.

### Architectural Trade-offs

Every non-trivial design decision in this chapter — expected vs. exceptions, unique_ptr vs. shared_ptr, header vs. module, coroutine-based vs. thread-based concurrency, tight coupling for simplicity vs. loose coupling for flexibility — is a trade-off evaluated against a specific set of constraints (performance budget, team size, expected rate of change, toolchain maturity, existing codebase conventions), not a search for the one objectively correct answer. This chapter's Level 7 problems in particular have no single reference answer; they are graded on the quality and self-awareness of the reasoning, per `INSTRUCTIONS.md:87`.

## Common Misconceptions

- **"Ranges are just prettier iterator syntax."** The laziness is the actual feature — a chain of adaptors is a zero-intermediate-allocation, single-pass pipeline; writing the equivalent by hand with explicit loops and temporary containers is both more code and often slower.
- **"A coroutine is inherently async/non-blocking."** `co_await`/`co_yield` describe a *suspension mechanism*, not a concurrency model — a coroutine can be a purely synchronous, single-threaded generator (`co_yield`-based) with no async behavior at all; whether a `task<T>` runs concurrently depends entirely on what its awaiter's `await_suspend` actually does (schedule onto a thread pool, or resume inline).
- **"Coroutine frames are always heap-allocated, so coroutines are inherently expensive."** HALO can elide the allocation when the compiler can prove the frame's lifetime is safely stack-nestable — but this is never guaranteed and must be checked, not assumed either way.
- **"Modules are a safe default replacement for headers in any codebase today."** Toolchain and build-system support is real but still maturing unevenly across MSVC/Clang/GCC as of this writing — evaluate the specific toolchain before committing, per this chapter's explicit exit criterion.
- **"`std::expected` should replace all exceptions everywhere."** The right error-handling mechanism is chosen per boundary and per failure category, not applied uniformly as an ideology — a genuinely unrecoverable, rare invariant violation is still a legitimate exceptions use case.
- **"`shared_ptr` is the safe choice when you're not sure who owns something."** Not thinking through ownership and defaulting to `shared_ptr` hides a design question rather than answering it, and pays a real, ongoing atomic-refcounting cost for ownership ambiguity that a clear design would not have.
- **"Loose coupling is always better design."** Decoupling has a real cost (indirection, more moving parts, harder to read as a whole) that must be justified by an actual, expected axis of independent variation — decoupling against a change that will never happen is pure overhead.
- **"Backwards compatibility is a migration task you do once, at a major version bump."** It is a standing constraint on every subsequent change once a public contract exists, not an event.

## Quick Checks

**13-QC1.** Why does chaining `views::filter` then `views::transform` over a large range typically outperform building an intermediate filtered `vector` by hand and then transforming it?

**13-QC2.** What makes a range "borrowed" rather than "dangling," and why does the range library detect a dangling range at compile time rather than leaving it as a runtime UB risk?

**13-QC3.** A function is written with `co_yield` and no `co_await`. Is it necessarily asynchronous? Justify your answer in terms of what actually determines whether suspension implies concurrency.

**13-QC4.** What are the three methods an awaiter must supply, and what does each one control?

**13-QC5.** Under what specific condition can a coroutine frame be elided from the heap entirely (HALO), and why is this never something you should assume happened without checking?

**13-QC6.** What is the practical, as-of-this-writing caveat this chapter requires you to state about modules before recommending them for a mixed-toolchain production codebase?

**13-QC7.** Give a concrete example of a boundary where an exception is still the right error-handling choice, and explain what makes it different from the cases where `std::expected` is preferable.

**13-QC8.** Why is choosing `shared_ptr` "to be safe" when ownership hasn't been thought through considered a design smell rather than a defensible default?

## Problems

### Level 1 — Recognition

**13-P01.** Given `auto r = v | std::views::filter(pred) | std::views::transform(f);` followed by a loop over `r`, state precisely when `pred` and `f` actually execute for a given element — at the point `r` is constructed, or at the point that element is dereferenced during iteration? Justify your answer from what a view is.

---

**13-P02.** A function signature is `std::generator<int> counter(int start)` and its body contains `co_yield start++;` in a loop, with no `co_await` anywhere. Classify this coroutine's suspension behavior: is control returned to the caller synchronously at each `co_yield`, and does anything here imply the use of multiple threads?

### Level 2 — Prediction

**13-P03.**
```cpp
auto make_dangling() {
    std::vector<int> v{1,2,3,4,5};
    return v | std::views::filter([](int x){ return x % 2 == 0; });
}
auto r = make_dangling();
for (int x : r) { /* ... */ }
```
Predict whether this compiles. If it does not, explain what specifically the range library detected and why "it happened to work in my quick test" would be a dangerous conclusion to draw from a single successful run of similar-looking code.

---

**13-P04.** Two versions of the same generator-style coroutine are compared: version A's caller immediately consumes and discards the coroutine handle within the same function, with no virtual calls or cross-TU boundaries involved; version B's caller stores the returned `task<T>` in a `std::vector` for later, cross-translation-unit consumption. Predict which version is the more plausible candidate for HALO eliding its frame's heap allocation, and state what you would check to confirm it either way, rather than trusting the prediction alone.

---

**13-P05.** A team is deciding between `std::expected<Data, ErrorCode>` and a thrown exception for a function that parses a possibly-malformed but *routinely expected* user-supplied config file. Predict which choice better fits the call site's likely pattern (a caller that expects failure often and wants to branch on it locally) and justify it in terms of the cost difference (stack-unwinding vs. a returned value) at the volume implied by "routinely expected."

---

**13-P06.** A public library's `Widget` class currently stores its data in a `std::vector<int>` internally, exposed only through `size()`/`operator[]`/`begin()`/`end()`. The maintainer wants to switch the internal storage to a custom small-buffer-optimized structure for a performance win. Predict whether this change breaks any correctly-written caller, and state precisely what "correctly-written" must mean here for your prediction to hold.

---

**13-P07.** A module `mymath` exports a function using a macro `MYMATH_VERSION` internally for a version check, without an explicit `export` on that macro. Predict whether an importer of `mymath` can see or use `MYMATH_VERSION`, and contrast this with what a header-based equivalent using the same internal macro would expose to its includers.

---

**13-P08.** A function's design hardcodes a call to `std::chrono::system_clock::now()` directly inside its body to compute a timeout. Predict what happens when a unit test attempts to test this function's timeout behavior deterministically, and state the specific design change (made at authoring time, not test time) that would have avoided the problem.

### Level 3 — Implementation

**13-P09.**
Implement a custom lazy view `stride_view` that, given a range and a stride `n`, yields every `n`-th element (indices 0, n, 2n, ...), built by inheriting from `std::ranges::view_interface<stride_view<R>>` and supplying only a custom iterator's `begin()`/`end()`/increment logic. Confirm via a static assertion or a runtime check that `view_interface` correctly derives `empty()` and (if the underlying range is sized) `size()` from your minimal iterator interface without you writing them directly.

---

**13-P10.**
Implement a `task<T>` coroutine type with a promise type supplying `initial_suspend()` (returning `std::suspend_always`), `return_value(T)`, `unhandled_exception()` (storing the exception for later rethrow), and `final_suspend()` (returning an awaiter that resumes a continuation if one was registered). Write a minimal awaiter so that `co_await someTask()` works from within another coroutine, and demonstrate two `task<T>`s chained via `co_await` where the second only begins after the first completes.

---

**13-P11.**
Implement a `generator<T>` coroutine type using `co_yield`, with a promise type supplying `yield_value(T)` that stores the yielded value for retrieval, and an input/output iterator pair so that a `generator<int>` can be used directly in a range-`for` loop. Demonstrate it producing an infinite sequence (e.g. natural numbers) safely consumed lazily via `std::views::take(gen, 10)`.

---

**13-P12.**
Implement symmetric transfer in a chain of at least four `task<T>`-returning coroutines, each `co_await`-ing the next, such that resuming the chain does not grow the native call stack by one frame per link — do this by having each `await_suspend` return a `coroutine_handle` (the next coroutine to resume) rather than calling `.resume()` directly inside `await_suspend`. Demonstrate the distinction empirically: show the naive (non-symmetric-transfer, `.resume()`-inside-`await_suspend`) version overflowing the stack at a chain depth where the symmetric-transfer version does not.

---

**13-P13.**
Write a minimal two-file C++20 module (`export module shapes;` defining a `Circle` and a `Rectangle` with an area function each) and a consumer translation unit that `import shapes;`s it, building successfully on your primary toolchain (per this workbook's MSVC-primary setup). Record, as a comment or short note alongside the code, exactly which toolchain and version you built it with and any build-system wiring (CMake module support) required — directly exercising this chapter's exit criterion of an explicit, dated support note rather than an assumption of universal readiness.

---

**13-P14.**
Design and implement an `expected`-based internal computation pipeline (at least three chained fallible steps, each returning `std::expected<T,E>`, composed via `and_then`/`or_else`/`transform`) for a scenario where failures are frequent and routinely handled locally (e.g. a batch record-validation pipeline where most records are expected to sometimes fail validation) — then implement the same logic using exceptions, and benchmark both under a workload where roughly 30% of inputs fail, reporting concrete numbers for which approach is cheaper at this failure rate, per Ch12's "no L3+ problem without a number" discipline carried forward.

---

**13-P15.**
Given a `Connection` class where ownership is currently ambiguous (constructed via `std::shared_ptr<Connection>` everywhere despite exactly one call site ever needing to extend its lifetime beyond the creator), redesign its ownership to make ownership explicit at each site — the creator holds a `unique_ptr`, the one call site that legitimately needs shared access explicitly documents why, and every other consumer takes `Connection&` or `Connection*` (non-owning) — and justify, in writing, why this design communicates intent that the original blanket `shared_ptr` did not.

---

**13-P16.**
Design a coroutine frame allocator: implement a promise type's `operator new`/`operator delete` that routes coroutine frame allocation through a `FixedPool<T>`-style pool allocator (from Ch12's `12-P17`) rather than the global allocator, sized for a specific expected frame size, and benchmark allocation cost for creating and destroying 100,000 short-lived coroutines with and without the pooled allocator, reporting the measured numbers.

### Level 4 — Debugging

**13-P17.** [DEBUG] A generator-based lazy pipeline `data | views::filter(is_valid) | views::transform(normalize)` is built once and then iterated twice in two separate loops, with the surprising symptom that `is_valid` and `normalize` are each observed (via added logging) to run twice as many times total as expected, and a colleague suspects the range library is caching incorrectly. Diagnose the actual cause (what re-iterating a lazy view actually does) and explain why "caching incorrectly" is the wrong mental model here.

---

**13-P18.** [DEBUG] A `task<T>` coroutine's `unhandled_exception()` implementation is empty (does nothing with the caught exception), and a bug report describes an exception thrown deep inside a `co_await`-chained pipeline as "just disappearing" — no crash, no visible error, the program continues with seemingly default/garbage results. Diagnose why an empty `unhandled_exception()` produces exactly this silent-swallowing symptom rather than a crash, and specify the minimal fix.

---

**13-P19.** [DEBUG] A `generator<T>` type's promise stores each yielded value in a class member and the iterator's `operator*` returns a reference to that member — a caller writes `auto v = gen_range | std::views::take(3) | std::ranges::to<std::vector>();` and observes all three collected values are identical (the last yielded value, repeated three times) rather than the three distinct expected values. Diagnose the aliasing/lifetime issue (what the reference is actually pointing at across resumptions) and specify the fix.

---

**13-P20.** [DEBUG] A team migrated a stable, widely-consumed internal header to a C++20 module and reports the build now fails intermittently only in CI, never on any single developer's machine, with an error suggesting a module interface file was read before it was fully written by a dependent build step. Diagnose this as a build-system dependency-scanning/ordering gap (a known area of immaturity flagged in this chapter's Crash Course) rather than a language-level module bug, and state what about CI (parallelism, clean-build-from-scratch, whatever the local dev machines happen to cache) makes it reproduce there specifically.

---

**13-P21.** [DEBUG] A public API's maintainer confidently ships an internal rewrite of a class's storage (per this chapter's stability discussion) and it breaks a downstream consumer anyway, who was calling `sizeof(Widget)` and hardcoding it into a serialization format. Diagnose precisely which part of the "observable contract" this consumer was relying on that the maintainer's stability reasoning did not account for, and state what the API should have done differently (e.g. an opaque-handle/PIMPL boundary) to make `sizeof` genuinely not part of the contract.

### Level 5 — Integration

**13-P22.**
Build a lazy log-processing pipeline over a `views::filter` (drop malformed lines) → `views::transform` (parse into a struct) → `views::filter` (drop out of a date range) → `views::take(N)` chain over a large (simulated multi-GB) log source modeled as an input range, and benchmark it against the equivalent eager, intermediate-`vector`-per-stage implementation — report peak memory and wall-clock numbers for both, demonstrating the lazy composition's practical benefit at this scale.

---

**13-P23.**
Combine a `task<T>` coroutine scheduler (built on `13-P10`'s `task<T>`) with the Ch11 work-stealing thread pool, so that `co_await someAsyncWork()` resumes the coroutine on a pool thread rather than inline — implement the awaiter's `await_suspend` to submit the resumption to the pool (using symmetric-transfer-compatible signatures from `13-P12` where the chain is deep) and demonstrate at least three concurrently-running `task<T>` chains completing correctly and in parallel, verified TSan-clean per Ch11's grading gate.

---

**13-P24.**
Design and implement a plugin interface with a stable C ABI boundary (extern "C" function table, versioned via an explicit interface-version field checked at load time) that a `.dll`/`.so` plugin implements and a host loads at runtime (per Ch09's dynamic-loading mechanisms) — then demonstrate evolving the *internal* C++ implementation behind that boundary (changing a plugin's internal data structures) without breaking ABI compatibility with an already-compiled host, and demonstrate the version check correctly rejecting a plugin built against an incompatible interface version.

---

**13-P25.**
Take a function currently hardcoding `std::chrono::system_clock::now()` and a global logger singleton directly in its body (per `13-P08`'s diagnosed problem), and redesign it for testability: inject a clock abstraction and a logging sink as dependencies (constructor injection or a template parameter), then write a deterministic unit test that controls simulated time and captures log output, proving the redesign actually closes the gap `13-P08` identified.

---

**13-P26.**
Design and implement an observability-first version of a request-processing pipeline (structured logging with correlation IDs, a metrics counter/histogram interface, and a tracing span abstraction) built in from the start, then demonstrate retrofitting the *same* instrumentation onto an existing pipeline that was not designed with these seams — compare the amount of code change and the number of call sites touched for each, concretely showing why "testability/observability is a design property, not an add-on" is not just a slogan but a measurable difference in retrofit cost.

---

**13-P27.**
Take the Integration Challenge's `task<T>` API (or `13-P10`'s) and perform a deliberate internal rewrite of its scheduling strategy (e.g. switching from immediate inline resumption to pool-based resumption per `13-P23`) while holding its public surface (the `task<T>` template, `co_await` usage, `.get()`/result-retrieval semantics) completely fixed — demonstrate this with a test suite written against the *old* implementation that passes unmodified against the *new* one, concretely proving the stability claim rather than asserting it.

### Level 6 — Production

**13-P28.** A production library currently versions its public headers with no formal policy — breaking changes have shipped in what callers assumed were patch releases twice in the last year, each time discovered only when a downstream consumer's build broke. Design a concrete versioning policy (semantic versioning rules mapped to specific categories of change — signature change, new virtual function, ABI layout change, behavior-only change — a deprecation-cycle length, and an enforcement mechanism, e.g. an ABI-compatibility-checking CI gate) and justify each specific rule against the two actual incidents described, not against versioning best-practice in the abstract.

---

**13-P29.** A codebase has religiously applied "loose coupling" via an interface-and-dependency-injection layer between every single pair of classes, including several pairs that have exactly one implementation each, will realistically never have a second, and are maintained by the same one engineer. New team members report the codebase is unusually hard to navigate — following any single piece of logic requires jumping through several interface indirections that resolve to only one concrete implementation each. Diagnose this as a coupling/cohesion trade-off applied without judgment, and design a concrete, justified rule for when an interface boundary earns its complexity cost versus when it should be a direct, concrete dependency.

---

**13-P30.** A library was shipped with `std::expected`-based error handling exclusively, including for a rare, genuinely unrecoverable heap-exhaustion path deep in its allocator layer, which now silently propagates an error value through several layers of `and_then` chains — three separate downstream teams have each independently written their own ad hoc "if this specific error code appears, just abort" handling at their boundary, duplicating the same judgment call three times. Design a concrete policy (which failure categories get `expected`, which get exceptions, and the specific criterion distinguishing them) that would have prevented this duplication, and justify it against this specific incident.

---

**13-P31.** A team wants to adopt C++20 modules across their entire production codebase in the next quarter, citing faster build times as the primary justification, on a project that currently builds successfully across MSVC, Clang (WSL), and GCC (WSL) per this workbook's toolchain matrix. Design a recommendation: either a staged, scoped adoption plan with explicit go/no-go checkpoints against measured build-time and toolchain-compatibility evidence, or a recommendation against full adoption at this time — justified against this chapter's explicit exit criterion (a dated, toolchain-specific maturity note, not a blanket "modules are/aren't ready" claim) and against the specific three-toolchain constraint this team actually has.

---

**13-P32.** A coroutine-heavy service reports intermittent latency spikes correlated with garbage-collector-like pause behavior, and profiling shows a large fraction of time in the global allocator during coroutine frame allocation/deallocation under load. Design a production fix using the pooled coroutine-frame-allocator technique from `13-P16`, sized and validated against this service's actual measured frame-size distribution (not a guess), and specify what monitoring/observability (per this chapter's observability-as-design-axis principle) you would add so a recurrence is caught by a metric rather than rediscovered via a fresh latency-spike incident.

---

**13-P33.** A plugin-host system (per `13-P24`'s pattern) is now in production with third-party plugin vendors, and one vendor's plugin, compiled against an older interface version, was loaded anyway due to a missing version check in one code path, causing a crash reported as "random and unreproducible" for weeks before being traced to this specific gap. Design a systematic fix: audit every plugin-loading code path for the version check's presence (not just patch the one found gap), specify what the version-check failure mode should be (reject-and-log vs. attempt-degraded-compatibility) and justify the choice, and specify a test (using `13-P24`'s deliberately-incompatible-version demonstration as a template) that would have caught this specific gap before it reached production.

### Level 7 — Principal Reasoning

**13-P34.** A new team lead proposes rewriting a mature, stable, thread-based concurrent service to use coroutines throughout, arguing "coroutines are more modern and efficient." Evaluate this proposal: under what concrete conditions would a coroutine-based rewrite actually deliver a measurable benefit over the existing thread-based design, under what conditions would it deliver none (or a regression), and what evidence would you require before approving the rewrite — treating "more modern" explicitly as not itself a justification, per this chapter's framing of every such choice as a trade-off against specific constraints.

---

**13-P35.** You are asked to decide your organization's default stance on adopting C++20 modules for new, greenfield internal libraries (not the mixed legacy codebase from `13-P31` — a fresh start with no existing header-based investment). Reasoning from this chapter's explicit, dated maturity caveat and from the specific toolchains your organization actually uses, construct the decision criteria you would apply, including what would have to be true about your toolchain and build-system maturity for you to recommend modules-first even in a greenfield setting, and what you would recommend today given the maturity state described in this chapter's Crash Course.

---

**13-P36.** A public library's maintainers are debating whether `std::expected<T,E>` should become the library's *sole* sanctioned error-reporting mechanism going forward, banning exceptions entirely from the public API, citing performance and explicitness benefits. Argue both the strongest case for this policy and the strongest case against it, then state and justify your own recommendation — specifically addressing the unrecoverable-invariant-violation case this chapter's Crash Course identifies as a legitimate exceptions use case, and explain what a "no exceptions in the public API" policy should actually do when such a case arises internally.

---

**13-P37.** Design the ownership and lifetime-management architecture for a plugin-host system (extending `13-P24`) where plugins can hand objects to the host, the host can hand objects to plugins, and either side might be unloaded while the other still holds a reference — reason explicitly about which ownership model (opaque handles with host-owned storage, reference counting across the ABI boundary, or a strict "no cross-boundary raw ownership, only borrowed access with explicit revocation") fits this specific hazard, and justify why the alternatives you rejected would each fail specifically at the unload-while-referenced case.

---

**13-P38.** Your organization's build takes 40 minutes and is the single largest complaint in the engineering-satisfaction survey; you are asked to design the fix, with no further specifics given. Reason through what you would actually need to know before proposing a specific technical fix (module adoption, unity builds, distributed build caching, dependency-graph restructuring, PCH strategy, or some combination), what you would measure first, and why jumping straight to "adopt modules" (this chapter's newest, least battle-tested lever) without that measurement would be professionally irresponsible even though modules are a real available answer to part of this problem.

---

**13-P39.** A senior engineer argues that `shared_ptr` should be the organization's default smart-pointer choice for all non-trivial ownership situations, "since it's always safe and we can optimize later if profiling shows it matters." Evaluate this argument on its own terms: what does defaulting to `shared_ptr` actually cost (both at runtime and at the design-clarity level), what specifically does "optimize later" require to be actually feasible once `shared_ptr` semantics are baked into an API's public signatures, and construct the counter-policy you would propose along with the concrete decision procedure (per this chapter's ownership-design framing) that replaces "default to shared, then reconsider" with a deliberate choice made once, up front.

---

**13-P40.** Design the architecture-level testing and observability strategy for a new coroutine-based concurrent service from scratch — specifically addressing what makes coroutine-based control flow harder to test deterministically than thread-based code (interleaving via suspension points rather than OS scheduling) and what specific design seam (an injectable/deterministic scheduler for tests, a way to force specific suspension orderings) you would build in from day one to make that testable, rather than discovering the gap after the service is already in production, mirroring `13-P25`'s and `13-P26`'s smaller-scale lessons at the scale of an entire service's architecture.

---

**13-P41.** Reflecting on this entire workbook's arc — from Ch01's core semantics through this chapter's architectural judgment — articulate the single most important shift in *how you evaluate a C++ design decision* between how you would have approached it before this workbook and how you approach it now, grounded in at least three specific concepts from at least three different chapters (not just this one), and explain concretely how that shift changes what evidence you now require before calling any given design "correct."

## Integration Challenge

**13-IC1.** Design and implement a coroutine-based `task<T>` scheduled onto the Ch11 work-stealing thread pool, exposing a public API stable enough to survive an internal rewrite of its scheduling strategy without callers noticing. Specifically:

1. **Implement `task<T>`** with a promise type and awaiter protocol (per `13-P10`) such that `co_await someTask()` composes correctly across chained coroutines, using symmetric transfer (per `13-P12`) so a deep chain does not grow the native call stack.
2. **Integrate with the Ch11 thread pool** (per `13-P23`) so that resuming a suspended `task<T>` dispatches onto a pool worker rather than resuming inline, and verify the result is TSan-clean under at least three concurrently-executing `task<T>` chains.
3. **Define the public contract precisely** — the `task<T>` template's interface, what `co_await`ing it means, what happens on an unhandled exception inside it (per `13-P18`'s diagnosed failure mode, deliberately avoided here) — separate from its scheduling implementation.
4. **Prove the stability claim, not just assert it** (per `13-P27`): write a test suite against the initial scheduling implementation, then perform a real internal rewrite of the scheduling strategy (e.g. swapping the pool-dispatch mechanism, or adding the pooled coroutine-frame allocator from `13-P16`) and show the same test suite passes unmodified against the rewritten internals.

This exercises ranges-adjacent lazy-evaluation thinking (a suspended coroutine is itself a kind of lazy, on-demand computation), coroutines, ownership design (who owns a `task<T>`'s frame and result), and ABI/API stability judgment simultaneously — the terminal synthesis this chapter's Exit Criteria require.

## Chapter Project

This chapter feeds directly into:
- **P-5.2 Coroutine Task & Generator Library** — builds directly on 13-P10, 13-P11, 13-P12, 13-P16, and 13-IC1's `task<T>`/`generator<T>`/symmetric-transfer/frame-allocator work.
- **P-5.6 Plugin Host with a Stable C ABI Boundary** — builds directly on 13-P24, 13-P33, and 13-P37's versioned-interface and cross-boundary-ownership design work.

This chapter is also the terminal on-ramp to the capstones (C-1, C-2, C-3) and the Principal-Level design problems (PL-1–4) — the architectural judgment exercised here (13-P28–P41) is the same judgment those problems require at a larger, less-scaffolded scale.
