# Chapter 08 — Compilation, Linking, and ABI

> Prerequisites: [Chapter 05](../05-generic-programming/CONCEPTS.md) (template instantiation drives most ODR/bloat problems), [Chapter 07](../07-object-model/CONCEPTS.md) (layout is ABI).
> This chapter is about what happens *between* "the compiler accepted my file" and "the program runs correctly" — translation units becoming one program, and what breaks when two of them silently disagree about what a name means.

## Crash Course

### Preprocessing, Translation Units, Declarations vs. Definitions

The preprocessor runs first, textually expanding `#include`, `#define`, and conditional blocks — by the time real compilation starts, there are no macros left, only the flattened result. Each `.cpp` file (with everything its headers pull in) becomes one **translation unit (TU)**, compiled independently into one object file. A **declaration** introduces a name and its type without necessarily describing storage or a body (`extern int x; void f();`); a **definition** actually allocates storage or provides a body. A name can be declared many times but defined only once *per program* (for non-inline, non-template entities) — this is the seed of the One Definition Rule.

### The One Definition Rule (ODR)

The **ODR** requires that any non-inline function or object with external linkage have exactly one definition across the whole program, and that any entity defined in multiple TUs (an inline function, a template specialization, a class defined in a shared header) have *token-for-token identical* definitions in every TU that defines it. Violating the first form is usually a linker error (multiple definition). Violating the second form — two TUs defining the same inline function or class differently, e.g. because of a macro or a `#ifdef`-guarded difference — is **silent undefined behavior**: the linker picks one definition arbitrarily (or the behavior is otherwise unspecified), no diagnostic is required, and the two halves of the program can end up with inconsistent assumptions about the same type.

### `inline`, Templates, and Instantiation

`inline` today means "may be defined identically in multiple TUs without violating ODR" far more than it means "please inline this call" (compilers decide inlining on their own heuristics regardless). `inline` variables (C++17) extend the same idea to data, solving the classic "header-only library needs a single shared global" problem without a `.cpp` file. Function templates and class template members are implicitly `inline`-like: each TU that uses a template instantiates its own copy, and the linker deduplicates identical instantiations across TUs (relying on ODR — they must indeed be identical). This can cause **code bloat** (the same instantiation compiled redundantly in every TU that uses it) and slows builds; `extern template` declares "don't instantiate this here, it's instantiated elsewhere" to move the cost to one TU, at the cost of needing to remember to actually provide that one explicit instantiation.

### Symbol Resolution and Name Mangling

Because C++ allows overloading, namespaces, and templates, a plain function name isn't a unique linker symbol — compilers **mangle** names into a scheme encoding the full signature (namespace, class, parameter types, template arguments) so the linker can distinguish `f(int)` from `f(double)` and `NS::f` from `::f`. Mangling schemes are compiler-specific and *not* standardized: the Itanium C++ ABI (used by GCC and Clang on Linux/macOS) and MSVC's own scheme produce completely different mangled names for the same C++ declaration — this is a root cause of why object files from one toolchain generally cannot link against object files from another. `extern "C"` disables mangling for a declaration, producing a plain C-linkage symbol, which is precisely why it's the standard technique for a stable cross-compiler/cross-language boundary.

### Static and Shared Libraries, Dynamic Linking

A **static library** (`.lib`/`.a`) is just an archive of object files; linking against one copies the needed object code directly into the final executable at link time. A **shared library** (`.dll`/`.so`) is linked *at load time (or runtime)* — the executable records which symbols it needs from which shared library, and the OS loader resolves and binds them when the program starts (or when `dlopen`/`LoadLibrary` is called explicitly). Shared libraries save disk/memory (one copy shared by many processes) and allow patching a dependency without recompiling dependents, but introduce **versioning risk**: if the shared library's ABI changes incompatibly, every dependent breaks at load time or, worse, at some specific runtime call, without a recompile ever happening.

### ABI, Binary Compatibility, and Pimpl

The **Application Binary Interface (ABI)** is everything the object-model rules from Chapter 7 (layout, alignment, vtable shape) plus calling conventions and name mangling add up to at the *binary* level — two binaries are ABI-compatible if one can call into the other without recompiling either. Adding a data member, adding a virtual function, or changing a base class can silently change a class's layout or vtable shape, breaking ABI for any already-compiled client that embeds the type by value or calls its virtuals — even though the *source* is perfectly backward compatible and recompiles fine. The **pimpl idiom** (pointer to an opaque implementation struct, forward-declared in the header, defined only in the `.cpp`) is the standard technique for insulating a public class's ABI from changes to its private implementation: clients only ever see a stable-size pointer member, so the actual implementation's layout can change freely without breaking already-compiled clients.

## Common Misconceptions

1. **"If my program compiles and links, the code is definitely correct with respect to ODR."** No — the linker only catches the *first* form of ODR violation (multiple definitions of a non-inline entity with external linkage). The far more dangerous *second* form — two TUs with token-for-token-different definitions of the same inline function, template specialization, or class — produces no diagnostic at all; the program silently runs with mismatched assumptions about the same name.

2. **"`inline` is a hint to the compiler to inline the function's calls."** Modern compilers make inlining decisions almost entirely by their own cost-heuristics, ignoring the `inline` keyword for that purpose in nearly all cases. `inline`'s actual, load-bearing meaning today is "this definition may legally appear, identically, in multiple translation units without violating ODR."

3. **"Name mangling is a standardized part of C++, so any compiler's output should link against any other's."** No — the standard says nothing about how names are mangled; each ABI (Itanium, MSVC) defines its own scheme, and they are mutually incompatible. Cross-compiler C++ linking is not generally supported; only `extern "C"` boundaries (which have no mangling to disagree about) are portable across compilers.

4. **"A shared library upgrade is safe as long as the new version's source is backward-compatible."** Source compatibility and *binary* (ABI) compatibility are different axes — adding a class member, changing a function's default argument, or reordering virtual functions can all be perfectly fine at the source level (existing client code still compiles) while silently breaking already-compiled client binaries that embedded the old layout or called through the old vtable slot order.

5. **"Static linking avoids all the problems dynamic linking has, so it's simply the safer choice."** Static linking avoids *load-time* symbol resolution risk and versioning skew between processes, but at the cost of larger binaries, no shared-memory savings across processes, and no ability to patch a vulnerable dependency without recompiling and redistributing every dependent — it trades one category of risk for a different one, not eliminating risk altogether.

6. **"A template function is instantiated once per program, like a normal function."** No — by default, each TU that uses a given template instantiation compiles its own copy, and the *linker* is relied upon to deduplicate identical copies across TUs at link time (this is only correct because ODR requires them to be identical). This is a direct cause of slower builds and larger intermediate object files as more TUs use the same instantiation — `extern template` exists specifically to opt out of this default.

## Quick Checks

**08-QC1.** What is the practical difference between a declaration and a definition, and why does the ODR care about that distinction?

**08-QC2.** Why is the "different inline-function definitions in different TUs" form of an ODR violation more dangerous in practice than the "two non-inline definitions of the same function" form?

**08-QC3.** What does the `inline` keyword actually guarantee today, as opposed to what its name suggests?

**08-QC4.** Why can't object files compiled by GCC and MSVC generally be linked together into one C++ program, even targeting the same OS/architecture?

**08-QC5.** What specific problem does `extern "C"` solve at a linking boundary, and why doesn't it need to solve an equivalent problem for overloaded functions?

**08-QC6.** Give one concrete change to a class's definition that is source-compatible (existing client code still compiles unmodified) but not binary-compatible (an already-compiled client binary breaks without recompiling).

**08-QC7.** What specific ABI-insulation property does the pimpl idiom provide that a plain public struct with private members does not?

**08-QC8.** Why does a shared library's version-compatibility risk generally exceed a statically-linked dependency's, and what does static linking give up in exchange for avoiding that risk?

## Problems

### Level 1 — Recognition

**08-P01.** Given `void f(int); void f(double);` declared in a header included by two different `.cpp` files, and each function defined once (matching, identically) across the program, is this an ODR violation? Explain what would make it one.

**08-P02.** A header defines `inline int helper() { return 42; }` and is included, unmodified, by five different `.cpp` files. Is this an ODR violation? Explain what specifically about `inline`'s guarantee makes this safe.

**08-P03.** Does a program that compiles and links successfully on GCC necessarily produce object files that can be linked against object files compiled by MSVC for the same function signatures? Explain the specific mechanism that makes the answer what it is.

**08-P04.** Given `class Widget { public: Widget(); private: struct Impl; std::unique_ptr<Impl> impl_; };` with `Impl`'s definition only in `Widget`'s `.cpp` file, can a client `.cpp` file that only includes `Widget`'s header, and never sees `Impl`'s definition, still compile code that constructs and uses a `Widget`? Explain why or why not.

**08-P05.** Is a `.dll`/`.so` shared library's code loaded and its symbols resolved at compile time, link time, or load/run time? State which, and name one practical consequence of that timing for detecting a missing symbol.

### Level 2 — Prediction

**08-P06.**
```cpp
// header.h
inline int counter() { static int n = 0; return ++n; }

// a.cpp
#include "header.h"
int use_a() { return counter(); }

// b.cpp
#include "header.h"
int use_b() { return counter(); }
```
Across the whole linked program, is there one shared `static int n`, or does each TU get its own independent copy? Explain what `inline`'s ODR guarantee, combined with `static` inside an inline function, actually produces here.

**08-P07.** A template `template<typename T> T max_of(T a, T b) { return a > b ? a : b; }` is called with `int` arguments in three different `.cpp` files of the same program. How many copies of `max_of<int>`'s machine code does the linker end up emitting into the final executable: three, one, or "compiler/linker-dependent, but never more than one by the time linking finishes"? Explain the mechanism (instantiation-per-TU followed by deduplication) that produces this answer.

**08-P08.** A shared library `libmath.so`/`math.dll` exports `double compute(double);`. The library is upgraded and its internal implementation of `compute` changes completely, but its signature and observable behavior for valid inputs stay the same. Does an already-compiled client executable that links against this library need to be recompiled to pick up the fix, or can it simply load the new shared library file? Explain what specifically must stay the same at the ABI level for this to work without recompilation.

**08-P09.** Two TUs both `#include` a header containing `class Point { public: int x, y; };`, but TU-A was compiled with a build flag that `#define`s a macro adding an extra member to `Point` only in TU-A's translation, while TU-B compiles the header without that macro defined. Predict what category of problem this causes when the program links and runs — a link error, a compile error, or something else — and name it precisely.

**08-P10.** A function is declared `extern template void process<int>(int);` in a header, with the actual explicit instantiation `template void process<int>(int);` provided in exactly one `.cpp` file. If a second `.cpp` file uses `process<int>` but that `.cpp` file is compiled and linked *without* ever linking against the object file containing the explicit instantiation, what happens: a compile error, a link error, or silently broken behavior? Explain why `extern template` shifts where (not whether) the instantiation must exist.

**08-P11.** A static library `mylib.lib`/`libmine.a` is linked into two different executables, `app1` and `app2`, both built from the same version of the library. If the static library is later rebuilt with an ABI-incompatible change (e.g., a class member added) and only `app1` is recompiled/relinked against the new static library while `app2` is not touched at all, does `app2`'s existing binary break? Explain why static linking's timing makes the answer different from the equivalent shared-library scenario.

**08-P12.** Given a class `Base` with virtual functions `f()` then `g()`, and `Derived : Base` overriding both, if a new version of `Base` reorders its virtual function declarations to `g()` then `f()` (keeping the same overall set), predict what happens when an *already-compiled* client binary (compiled against the old vtable order) calls `derived_ptr->f()` against a `Derived` object built with the *new* `Base` — does it call the right function, the wrong function, or is this unspecified/UB? Explain in terms of vtable slot order.

**08-P13.** A header-only library defines a non-`inline`, non-template, non-`static` free function with external linkage directly in the header (no `inline` keyword). It's included by two different `.cpp` files in the same program. Predict what happens at link time and name the specific rule being violated.

**08-P14.** A DLL/shared-object exports a C++ class by value across its public API boundary (a function returns a `std::vector<Widget>` by value, where `Widget` is defined in a header shared between the DLL and its client, both built with the *same* compiler and standard library version). Is this generally safe, and what specific matching requirement (beyond "same compiler") does it actually depend on that makes it fragile in practice across different build configurations of that same compiler?

**08-P15.** Two overloaded functions `void log(const std::string&);` and `void log(std::string_view);` are both declared `extern "C"` in an attempt to give them a stable, unmangled linkage name. Predict what happens when this code is compiled: does it compile successfully with two distinct `extern "C"` symbols, or does something go wrong? Explain why `extern "C"` and function overloading are fundamentally in tension.

### Level 3 — Implementation

**08-P16.** Implement a minimal reproduction of a silent ODR violation: write a header `bad.h` containing a class whose definition differs depending on whether a macro `FEATURE_X` is defined before the header is included, then write two `.cpp` files — one that defines `FEATURE_X` before including the header, one that doesn't — each of which constructs an instance of the class and calls a method on it, in a way whose behavior visibly differs between the two definitions. Compile and link both into one program (documenting the actual command lines used) and describe (or, if reproducible on your toolchain, demonstrate) the resulting behavior, tying it explicitly to the "no diagnostic required" nature of this ODR-violation category.

**08-P17.** Implement a small library-style component `template<typename T> class Box { public: explicit Box(T v); T get() const; private: T value_; };` with its member function bodies defined out-of-line in a `.cpp` file rather than inline in the header, and explicit instantiations `template class Box<int>; template class Box<std::string>;` provided in that `.cpp` file. Demonstrate that a separate client `.cpp` file, which only sees the class template's *declaration* (no member bodies) via the header, can still successfully use `Box<int>` and `Box<std::string>`, and explain why attempting to use `Box<double>` from the client would fail to link (not fail to compile) given this setup.

**08-P18.** Write a minimal pimpl-based class `class Logger { public: Logger(); ~Logger(); void log(const std::string&); private: struct Impl; std::unique_ptr<Impl> impl_; };` with `Impl`'s definition and all of `Logger`'s member bodies in a `.cpp` file. Demonstrate, by actually changing `Impl`'s internal data members (e.g., adding a new private field to `Impl`) and rebuilding only the `.cpp` file (not any client `.cpp` file, and not the header), that client code continues to compile and link correctly without being touched — then explain specifically why this would *not* have been true had `Impl`'s members been exposed directly as `Logger`'s own private members instead.

**08-P19.** Using your platform's tools (`dumpbin /symbols` on MSVC, or `nm -C` on GCC/Clang via WSL), compile a small `.cpp` file containing an overloaded function `void process(int); void process(double); namespace ns { void process(int); }` into an object file, then dump its symbol table. Identify the three distinct mangled names, and explain how the mangled names differ from each other in a way that lets the linker distinguish all three despite the shared base name `process`.

**08-P20.** Implement a tiny static library (`.lib`/`.a`, built via your toolchain's archiver) containing one function `int square(int)`, and a tiny shared library (`.dll`/`.so`) containing a different function `int cube(int)`, then build one executable that links against both. Document the actual build commands used (compiling to object files, archiving into the static lib, building the shared lib with the appropriate export mechanism — `__declspec(dllexport)` on MSVC or default visibility on GCC/Clang — and the final link step), and explain, from your own build log, which of the two library's code physically ends up inside the final executable file versus remaining in a separate file loaded at runtime.

**08-P21.** Write a function template `template<typename T> void log_value(const T& v)` used with `int` and `std::string` in three different `.cpp` files of a small program. Build the program twice: once relying on implicit per-TU instantiation, and once using an `extern template void log_value(const int&);` declaration in the header plus one explicit instantiation definition in a single `.cpp` file for the `int` case specifically. Compare (via your build system's object file sizes, or a compiler flag that reports instantiations) the total object-file footprint of the `int` instantiation across all TUs between the two approaches, and explain the difference you observe (or would expect) in terms of what work moved from "every TU" to "one TU."

**08-P22.** Implement a minimal ABI-boundary example: a shared library exporting `extern "C" { void* create_widget(); void destroy_widget(void* w); int widget_value(void* w); }` (a C-style opaque-handle API) wrapping an internal C++ class never exposed directly across the boundary, and a client program that calls these three functions without ever seeing the C++ class's definition. Explain why this specific pattern (C linkage, opaque `void*` handle, functions instead of methods) sidesteps both the name-mangling cross-compiler problem and the class-layout ABI-fragility problem simultaneously.

**08-P23.** Implement two versions of a shared-library export: version A returns a `struct Point { int x, y; };` by value from an exported function; version B instead takes an output-parameter `void get_point(int* out_x, int* out_y)` using only fundamental types across the boundary. Explain, referencing calling-convention and struct-layout ABI concerns from this chapter, which version is more robust to being called from a client built with a *different* compiler (not just a different version of the same compiler) than the one that built the shared library, and why.

### Level 4 — Debugging

**08-P24.** [DEBUG]
```cpp
// widget.h
class Widget {
public:
    Widget(int v) : value_(v) {}
    int value_;   // note: still public in this version
};

// used identically, header included unmodified, in both TUs below

// a.cpp
#include "widget.h"
int get_a() { Widget w(1); return w.value_; }

// b.cpp  (compiled with a *different* struct-packing pragma/flag than a.cpp)
#pragma pack(push, 1)
#include "widget.h"
#pragma pack(pop)
int get_b() { Widget w(2); return w.value_; }
```
Identify precisely what's wrong here: both TUs include the identical header text, yet `Widget`'s actual compiled layout differs between them because of the packing pragma wrapped around the `#include` in `b.cpp`. Name the specific rule this violates, and explain why the compiler does not (and generally cannot) diagnose this at compile time.

**08-P25.** [DEBUG] A developer ships a shared library whose header declares `void process(std::vector<int> data);` and later changes the *implementation* (not the header) to internally use a different container, then rebuilds and redistributes only the shared library binary — the header the client compiled against is untouched, and the client is not recompiled. At runtime, calling `process` from the old client crashes. Identify what's most likely wrong: is this a violation of source compatibility, binary compatibility, or both — and explain specifically what could make an unchanged public signature still break ABI when only the internal implementation changes.

**08-P26.** [DEBUG]
```cpp
// mathlib.h (shared by both the DLL and its client, included by both)
inline double square(double x) { return x * x; }
```
The DLL is built with `/fp:fast` (a floating-point optimization flag); the client executable is built without it. Both link successfully and run without crashing, but occasionally produce slightly different results for `square()` calls that appear identical at the source level, depending on which binary's copy of the inlined function actually executes for a given call site. Identify precisely why an `inline` function shared via a header, built with different compiler flags in different TUs, can violate ODR even though the *source text* is byte-for-byte identical in both places — and explain why this is a more insidious form of the same-name-different-definition problem than the macro-guarded case.

**08-P27.** [DEBUG] A team's shared library exports a class hierarchy where `Base` has three virtual functions. A later release adds a *fourth* virtual function to `Base`, appended at the end of the declaration, and ships a rebuilt shared library without requiring client recompilation (the header did change, but clients were told "just relink, no source changes needed, should be binary compatible since we only appended"). Some clients experience crashes calling what should be unrelated, unmodified virtual functions. Identify what's actually wrong with the "appending is always binary-compatible" assumption — specifically considering multiple inheritance and virtual-inheritance layouts (not just the simple single-inheritance case) as one place this assumption can fail, and explain what evidence (from the object file or crash) would confirm a vtable-layout mismatch is the actual cause.

**08-P28.** [DEBUG] A build system compiles the same `.cpp` file twice into two different static libraries — once as part of `libA.a`/`libA.lib` and once, unmodified, as part of `libB.a`/`libB.lib` — and a final executable links against both `libA` and `libB`, each of which defines the same non-`inline`, non-template, externally-linked function `int helper()`. Predict precisely what happens at link time (not at compile time), and explain why this differs from the "silent" ODR-violation cases discussed earlier in this chapter — i.e., why *this* particular duplicate-definition scenario is diagnosable while the inline/template/macro-driven ones are not.

**08-P29.** [DEBUG] A shared library is built with `-fvisibility=hidden` (GCC/Clang) but the developer forgot to mark the one function meant to be public, `void public_api();`, with the corresponding `__attribute__((visibility("default")))` (or, on the header, an appropriate export macro). The library builds without error, but a client program fails to link against it with an "undefined symbol: public_api" error, even though `public_api` is clearly defined in the shared library's source. Identify precisely why `-fvisibility=hidden` causes this, and explain the MSVC-equivalent mistake (using `__declspec(dllexport)`/`dllimport` incorrectly, or omitting a `.def` file entry) that would produce an analogous symptom.

**08-P30.** [DEBUG] A pimpl-based class `Widget` (with `struct Impl` fully hidden in the `.cpp` file) has its destructor `~Widget()` defaulted directly *in the header* (`~Widget() = default;` written inline in the class body), rather than declared in the header and defined (even if still just `= default;`) in the `.cpp` file where `Impl`'s definition is visible. Client code that only includes the header fails to compile with an error about `Impl` being an incomplete type. Identify precisely why `~Widget()`'s defaulted-in-header placement requires `Impl` to be complete at that point (because `unique_ptr<Impl>`'s destructor needs to know how to destroy `Impl`), and state the one-line fix (declare in the header, define — even as `= default` — in the `.cpp`).

**08-P31.** [DEBUG] Two shared libraries, `liba.so`/`a.dll` and `libb.so`/`b.dll`, both link against a common third shared library `libcommon.so`/`common.dll`, but were built at different times against two *incompatible* versions of `libcommon`'s header (a struct's layout changed between versions) — and the executable that loads both `liba` and `libb` at runtime ends up, through the OS's dynamic loader, resolving both to the *same* single loaded copy of `libcommon` at runtime (only one version can actually be present in the process). Identify precisely why this specific "diamond dependency" scenario produces a landmine that neither `liba`'s nor `libb`'s own tests would catch in isolation, and name what class of tool (from this chapter's dumpbin/nm-based tooling) could reveal the version/layout mismatch before it manifests as a runtime crash.

### Level 5 — Integration

**08-P32.** A widely-used internal library's public headers currently expose several classes' full internal representation directly (no pimpl), because pimpl was judged, when the library was first written, to add unacceptable indirection overhead in a performance-sensitive hot path where these classes are used. The library has since grown many more consumers, several of whom have been broken by ABI changes across releases specifically because of this exposed representation. Propose a design that gives the ABI-stability benefit of pimpl to the majority of (non-hot-path) call sites without imposing the indirection cost on the hot-path ones — e.g., pimpl for the general-purpose class, with a separate, explicitly-named, ABI-unstable "fast" type reserved for the specific hot-path consumers who have actually measured a need for it — and state what measurement (not intuition) should determine whether the original performance concern is still load-bearing today.

**08-P33.** Build a small three-file program — a header declaring a class template `template<typename T> class Cache { public: void put(T v); T get() const; private: std::vector<T> items_; };` with member bodies inline in the header, two `.cpp` files each instantiating `Cache<int>` and calling both methods, and a small `CMakeLists.txt`/build script that compiles both into one executable. Use your platform's tooling (`dumpbin /symbols` or `nm -C`) to confirm that both TUs' object files contain a symbol for `Cache<int>::put`, then explain — pointing at the actual mangled/demangled symbol names you observed — how the linker is able to merge these into a single copy in the final executable without a multiple-definition error, tying your answer explicitly to the implicit-`inline`-like status of template member functions.

**08-P34.** Design and build a small "plugin" system: a host executable defines an abstract interface `struct Plugin { virtual void run() = 0; virtual ~Plugin() = default; };` (in a header shared by the host and every plugin), and at least two separate shared libraries (`.dll`/`.so`) each implement a concrete `Plugin` subclass and export a C-linkage factory function `extern "C" Plugin* create_plugin();`. The host loads each shared library at runtime (`LoadLibrary`/`dlopen`), retrieves the factory function by name (`GetProcAddress`/`dlsym`), and calls `run()` polymorphically on the result. Explain why the factory function must be `extern "C"` even though the type it returns (`Plugin*`) is a full C++ polymorphic type, and what would break (and how) if the host and every plugin were not all built with the same compiler/ABI for the `Plugin` class's layout specifically.

**08-P35.** Take a real (or realistic, minimal) header-only library pattern — a class with several `inline` member functions plus one `inline` free function performing a genuinely nontrivial computation — and deliberately introduce, then detect and fix, a build-configuration-driven ODR violation: build one TU with an optimization flag that changes floating-point behavior (e.g., `/fp:fast` vs `/fp:precise`, or `-ffast-math` vs not) and another TU without it, both including the identical header. Using either observed runtime behavior (nondeterministic-seeming results) or a sanitizer/tool capable of detecting it if available on your toolchain, document what you observe, then propose and implement a concrete build-system fix (e.g., ensuring all TUs that see this header are compiled with consistent flags) that eliminates the violation, explaining why the fix works.

### Level 6 — Production

**08-P36.** Your team ships a shared library (`.dll`/`.so`) to external customers, versioned `1.x`, with a documented ABI-stability promise: "any `1.x` client binary must keep working against any newer `1.y` shared library, y ≥ x, without recompilation." Propose a concrete set of engineering practices and constraints (referencing this chapter's ABI/layout/pimpl material) that a library author must follow to actually keep that promise across releases — addressing at minimum: what kinds of changes to public classes are and are not safe under this promise, what role pimpl or a similar insulation technique plays, and what a pre-release CI check could concretely verify (e.g., comparing exported symbol lists or `dumpbin`/`nm` output between the previous and candidate release) to catch an accidental ABI break before it ships.

**08-P37.** A production build system currently compiles every `.cpp` file with the same aggressive template-heavy header included everywhere (a "God header" containing dozens of class templates used pervasively), and full rebuilds take 40 minutes — profiling shows a large fraction of that time is redundant template re-instantiation across TUs. Propose a concrete refactoring plan using `extern template` (and any necessary explicit instantiation `.cpp` files) to reduce this cost for the handful of template instantiations that are used in the overwhelming majority of TUs, explain what the plan does *not* solve (i.e., which categories of build-time cost remain even after this change, previewing Ch10's build-system territory referenced but not owned here), and identify one measurable way to confirm the change actually reduced total build time rather than just moving the cost around.

### Level 7 — Principal Reasoning

**08-P38.** Your organization is deciding, for a new internal C++ library used by a dozen different teams' services, whether to distribute it as (a) source only (every consumer compiles it themselves as part of their own build), (b) a static library built once per supported toolchain/configuration, or (c) a shared library with a documented, versioned ABI. Reason through the tradeoff across build-time cost, ABI-versioning risk, ease of patching a bug across all consumers without their involvement, and toolchain-fragmentation risk (consumers on different compilers/flags) — and propose a decision process (not just a verdict) for which to choose, addressing specifically what would have to be true about this library's rate of change, its consumers' toolchain diversity, and the organization's release/patching cadence for each of the three options to be the right call, since none of the three is unconditionally correct.

**08-P39.** A principal engineer is asked to review a proposal to add a single new virtual function to a heavily-used, ABI-stable public `Base` class in a shared library that dozens of external customers link against, where the new function would be inserted *between* two existing virtual functions in the class declaration (for "logical grouping" reasons) rather than appended at the end. Reason through why insertion (versus appending at the end) is categorically worse for ABI stability here, referencing vtable slot-order mechanics precisely, then propose at least two concrete alternative designs that would let the team add the needed functionality without breaking existing client binaries — and state what evidence or process (e.g., an automated ABI-diffing tool in CI, a documented review checklist) should exist to make this specific mistake structurally difficult to ship by accident in the future, not merely something reviewers are expected to remember to check.

## Integration Challenge — 08-IC1

You're handed two shared libraries, `libcore.so`/`core.dll` and `libplugin.so`/`plugin.dll`, and an executable that loads both and crashes shortly after starting. You are given no source code for either shared library — only the binaries themselves, plus the crash's rough symptom (a segmentation fault / access violation inside a call that appears, from the visible stack trace, to be a virtual function call into `libplugin` on an object that `libcore` constructed and handed to it).

1. Using `dumpbin /symbols /exports /dependents` (MSVC) or `nm -C` / `objdump -T` / `readelf -d` (GCC/Clang), extract and compare the exported symbol lists of both libraries, and identify which symbols `libplugin` actually imports from `libcore`.
2. Explain, from the symptom alone (a virtual call crashing, not a missing-symbol link error), why the most likely category of root cause is an ABI mismatch in a *class layout or vtable shape* that both libraries agree to reference by name but disagree about in shape — not a simple missing/renamed symbol (which would instead fail at load time with a clear "unresolved symbol" error).
3. Propose the specific further evidence you would gather to confirm this hypothesis (e.g., comparing the mangled/demangled signature and size information for the shared class between what each library's build was compiled against, or checking for a flag mismatch — different struct packing, different standard library ABI version, different exception-handling model — between the two libraries' build configurations).
4. State what change would fix this for the *current* pair of binaries (without source access, is a fix even possible from the binaries alone, or does this fundamentally require a rebuild of at least one library against a shared, consistent header/ABI?) and what process change would prevent this category of bug from being introduced again — tying your answer back to 08-P36's ABI-stability-promise practices and 08-P38's distribution-model tradeoff.

## Chapter Projects

This chapter feeds directly into:

- **[P-3.1](../PROJECT_ROADMAP.md) Binary Object File Inspector (ELF + PE)** — draws directly on 08-P19's symbol-table-dumping exercise, 08-IC1's dumpbin/nm/objdump/readelf diagnostic methodology, and this chapter's name-mangling and ABI material generally.
