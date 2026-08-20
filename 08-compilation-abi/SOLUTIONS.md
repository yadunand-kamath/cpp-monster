# Chapter 08 — Solutions

## Quick Check Answers

**08-QC1.** A declaration introduces a name and its type (e.g. `extern int x; void f();`) without allocating storage or providing a body; a definition actually allocates storage or supplies the body. ODR cares because its whole premise — "one definition per program, but declare anywhere" — only makes sense if the two are distinguishable: you may repeat a declaration in every TU that needs the name, but the definition (the thing that actually reserves the storage or gives the code a body) must be singular (or, for inline/template entities, singular-in-content across all its copies).

**08-QC2.** The non-inline/non-template case (two definitions of the same non-inline external-linkage function) is caught by the linker as a hard "multiple definition" error — the build fails loudly. The inline/template/macro-guarded case produces *different* definitions that are each individually legal to appear in multiple TUs, so the linker just picks one silently (no diagnostic required by the standard); the program links and runs, but with two halves of the codebase holding inconsistent assumptions about what that name means — a bug that surfaces only as unexplained runtime behavior, if it surfaces at all.

**08-QC3.** `inline` guarantees "this definition may legally appear, identically (token-for-token), in multiple translation units without violating ODR" — it is a linkage/ODR relaxation, not a performance directive. It does not guarantee, or even meaningfully influence in most modern compilers, whether calls to the function are actually inlined at the call site; that decision is made by the optimizer's own cost heuristics independently of the keyword.

**08-QC4.** Because name mangling — the scheme that encodes a function's namespace, class, parameter types, and template arguments into a linker symbol — is not standardized by the C++ standard. GCC/Clang use the Itanium C++ ABI's mangling scheme; MSVC uses its own, incompatible scheme. Even if every other convention (calling convention, struct layout) happened to match, the two toolchains would produce different symbol names for the identical declaration, so the linker literally cannot find a matching symbol across the boundary.

**08-QC5.** `extern "C"` disables name mangling for a declaration, producing a plain, unmangled symbol name that both a C compiler and any other C++ compiler's C-linkage convention agree on — solving the cross-compiler/cross-language linking problem. It doesn't need to solve an equivalent problem for overloaded functions because C linkage has no overloading: a `extern "C"` symbol name is just the bare function name, so two `extern "C"` functions with the same name (even with different parameter types) collide, which is precisely why `extern "C"` and overloading are mutually exclusive in practice (see 08-P15).

**08-QC6.** Adding a new data member to a class (or reordering/adding virtual functions) is source-compatible — existing client `.cpp` files that only call the class's existing public interface still compile without modification — but not binary-compatible: it changes `sizeof(T)`, member offsets, or vtable slot layout, so an already-compiled client binary that embeds the type by value, or that was compiled against the old vtable order, will read/write the wrong offsets or call through the wrong vtable slot once linked against the new binary, without ever being recompiled.

**08-QC7.** Pimpl guarantees that the *public* class's size and layout, as seen by any client TU, never changes — the client only ever sees a stable-size opaque pointer (`std::unique_ptr<Impl>`) — no matter how much the hidden `Impl` struct's members change, grow, or reorder. A plain public struct with only *access-control*-private members still exposes its actual data layout (size, member offsets) to every client TU that includes the header, because `private:` is a compile-time access restriction, not a layout-hiding mechanism — the compiler still needs and emits the full layout for every TU that sees the class definition, so changing a private member still changes `sizeof` and breaks ABI for existing client binaries.

**08-QC8.** A shared library's ABI is checked only at load/run time, against whatever code binary is actually installed at that moment — so an ABI-incompatible upgrade can silently break every already-compiled client the instant the new shared library file is dropped in, with no recompilation step to catch the mismatch. A statically-linked dependency's code is copied into the client binary at link time, so once a client is built, its dependency's exact ABI is frozen inside it — later ABI-incompatible changes to the library's source simply don't affect that already-linked client at all (it would need to be relinked). In exchange for avoiding this versioning risk, static linking gives up the ability to patch or upgrade the dependency without recompiling/relinking every dependent, and gives up sharing one physical copy of the library's code across multiple running processes.

## Problem Solutions

### Level 1 — Recognition

**08-P01.** No, this is not an ODR violation as described. `void f(int); void f(double);` are two distinct overloads, each a distinct entity with its own single definition across the program (matching, identically, wherever it's defined) — ODR is satisfied per-entity, and overloading doesn't change that; each mangled name is unique. It *would* become a violation if, e.g., one TU defined `f(int)` differently than another TU that also provides a definition of `f(int)` (not merely a declaration) — i.e., if the "exactly one definition, or identical-if-inline" rule were broken for one of the two individual overloads, not because two different overloads coexist.

---

**08-P02.** No. `inline` is precisely the mechanism the standard provides for this exact situation: a function defined identically in every TU that includes the header is legal under ODR as long as it's marked `inline`, because `inline`'s guarantee is "this definition may appear, token-for-token identical, in multiple TUs without being treated as a duplicate-definition error." The linker deduplicates the multiple identical copies into one at link time; there is no violation because the definitions are textually identical in all five TUs.

---

**08-P03.** No. Compiling and linking successfully on GCC says nothing about interoperability with MSVC-compiled object files, because the two toolchains use different, mutually incompatible name-mangling schemes (Itanium ABI vs. MSVC's own scheme) for the same C++ signatures. Even a trivially simple function will get a different mangled symbol name from each compiler, so MSVC's linker will report the GCC-produced symbol as undefined (and vice versa) — cross-compiler C++ object-file linking is not generally supported except across an `extern "C"` boundary, which has no mangling to disagree about.

---

**08-P04.** Yes. The client `.cpp` file only needs `Impl` to be an *incomplete* type to declare a `std::unique_ptr<Impl>` member and to call `Widget`'s already-declared member functions — none of that requires `Impl`'s definition to be visible. `Widget`'s constructor and destructor bodies (which do need `Impl` complete, to construct/destroy it) live in `Widget`'s own `.cpp` file, where `Impl`'s definition is visible; the client never needs to see it because it never directly constructs, destroys, or accesses `Impl`.

---

**08-P05.** Load/run time (or, for explicit dynamic loading, exactly when `dlopen`/`LoadLibrary` is called). The executable only records *which* symbols it needs from *which* shared library at link time; the OS loader actually locates the library file and binds the addresses of its symbols when the program starts (or when the explicit load call executes). One practical consequence: a missing or renamed symbol in a shared library is not caught at compile time or even at the initial (implicit) link time in the same way a static-library mismatch might be surfaced early — it can surface only when the program is actually run (or, worse, only when a specific rarely-executed code path first calls into that symbol), long after the build reported success.

### Level 2 — Prediction

**08-P06.** Each TU gets its own independent copy of `static int n` — there is no single shared counter across `a.cpp` and `b.cpp`. `inline`'s ODR guarantee only says the *function's* multiple identical definitions are legal and get deduplicated as one function; it says nothing about un-hoisting local `static` variables into a single shared instance across TUs. In fact, a function-local `static` inside an `inline` function is required by the standard to refer to the *same* object in every TU where the inline definition appears — so this is actually the opposite of what the problem statement suggests: there **is** one shared `static int n`, because the ODR-mandated "identical definition" for `n` inside the identically-defined inline function collapses to a single object, just as the function itself collapses to a single instantiation. Calling `counter()` from either TU increments the same shared counter.

---

**08-P07.** "Compiler/linker-dependent, but never more than one by the time linking finishes" — more precisely, in the normal case each of the three TUs implicitly instantiates its own copy of `max_of<int>`'s machine code (three initial copies), but because `max_of<int>` is implicitly `inline`-like, the linker treats these as ODR-identical duplicate definitions and deduplicates them into exactly one copy in the final linked executable. The compilation step produces per-TU redundancy (contributing to code bloat and slower builds); the *linked artifact* ends up with one copy.

---

**08-P08.** The client executable can simply load the new shared library file without recompilation, as long as `compute`'s exported symbol name (unmangled, or identically mangled), its calling convention, and its observable input/output contract at the ABI level are unchanged — none of which requires the *internal implementation* to stay the same. Since the signature `double compute(double)` and its behavior for valid inputs are unchanged, and no data-layout (struct/class ABI) is involved for a plain function taking/returning fundamental types, this is a safe binary-only replacement — the classic case dynamic linking is designed to support (patch the implementation, no recompile needed).

---

**08-P09.** This causes a silent ODR violation specifically — the second, undiagnosed form: both TUs `#include` the same header text, but the macro makes TU-A's actual compiled definition of `Point` different (different size/layout) from TU-B's. This is not a link error (both TUs may well link successfully, since `Point` isn't necessarily itself defining an external-linkage function the linker would flag) and not a compile error (each TU individually compiles its own, internally-consistent view just fine) — it's undefined behavior with no diagnostic required, because the class is defined with genuinely different token sequences in the two TUs and yet is treated by the linker/runtime as "the same type" if a `Point` object crosses the TU boundary (e.g. passed by value to a function defined in the other TU), corrupting layout assumptions on one side.

---

**08-P10.** A link error. `extern template void process<int>(int);` promises the compiler "don't instantiate this here — trust that a definition exists somewhere in the final link," which suppresses the second `.cpp` file's own implicit instantiation. Since that promise is never fulfilled for this particular link (the object file containing the one actual explicit instantiation is never linked in), the linker has a call to `process<int>` with no corresponding definition anywhere in the set of object files being linked — a classic unresolved-external-symbol link error, not a silent failure and not a compile-time error (the compiler has no way to know, from that one TU alone, whether the instantiation exists elsewhere).

---

**08-P11.** No, `app2` does not break. Static linking copies the needed object code directly into the executable *at link time* — once `app2` was built and linked against the old version of `mylib`, that object code is now physically embedded inside `app2`'s own binary, completely decoupled from whatever happens to the static library file afterward. Rebuilding the static library has zero effect on `app2` unless `app2` is itself recompiled/relinked. This differs fundamentally from the shared-library case, where the *same* rebuild-without-recompiling-the-client scenario would immediately break `app2` at its next load, because shared-library symbol resolution happens at load/run time against whatever binary is currently installed, not at the time the client was originally built.

---

**08-P12.** The wrong function is called (or, formally, this is undefined behavior manifesting as calling the wrong function) — not "the right function" and not merely abstractly "unspecified" in a way that's actually safe. The already-compiled client binary was built assuming `f()` occupies vtable slot 0 and `g()` occupies slot 1; the new `Base` reorders the *declarations*, which reorders the *actual* vtable slot assignment to `g()` at slot 0, `f()` at slot 1. When the old client calls `derived_ptr->f()`, it looks up slot 0 (as it was compiled to do) — which the new binary has populated with `g`'s address — so it ends up calling `g()`'s implementation instead of `f()`'s. This is the concrete mechanism behind "reordering virtual functions breaks ABI."

---

**08-P13.** A link error — specifically, a multiple-definition error, once both `.cpp` files are compiled into object files and linked together. The function is external-linkage, non-`inline`, non-template, so ODR's first (diagnosable) form applies: exactly one definition is allowed across the program, but here the header causes it to be *defined* (not just declared) in both `a.cpp`'s and `b.cpp`'s translation units, giving the linker two definitions of the same external symbol — the rule being violated is the plain "one definition per program" clause of the ODR, and unlike the inline/template/macro cases elsewhere in this chapter, this one is guaranteed to be diagnosed.

---

**08-P14.** Generally safe under the stated conditions, but it depends on more than "same compiler" — it depends on the DLL and its client being built with the exact same compiler *version and configuration*, most importantly the same C++ standard library implementation and ABI version (e.g., the same version of MSVC's STL / `libstdc++` / `libc++`, and matching runtime library linkage — static vs. dynamic CRT, Debug vs. Release iterator/allocator layout). `std::vector<Widget>`'s own internal layout (and its allocator, and how it manages the heap it allocates from) is itself part of the standard library's ABI, and different builds/versions/configurations of "the same compiler" can and do change that layout or allocator behavior, which is exactly why passing standard-library container types across a shared-library ABI boundary is considered fragile in practice even under same-compiler assumptions.

---

**08-P15.** Something goes wrong — this fails to compile (a redeclaration/conflicting-linkage error, or in practice a duplicate-symbol situation the compiler rejects outright), because `extern "C"` linkage has no mangling scheme to distinguish overloads by parameter types; both declarations would map to the identical, unmangled symbol name `log`. The compiler detects the conflict (two functions wanting the same C-linkage name) and rejects the program. This is the concrete illustration of why `extern "C"` and overloading are fundamentally in tension: mangling is exactly the mechanism overloading relies on to produce distinct linker symbols, and `extern "C"` exists specifically to turn that mechanism off.

### Level 3 — Implementation

**08-P16.**
```cpp
// bad.h
struct Widget {
#ifdef FEATURE_X
    int a, b;
    int compute() const { return a + b; }
#else
    int a;
    int compute() const { return a; }
#endif
};

// tu_with_feature.cpp
#define FEATURE_X
#include "bad.h"
int get_with_feature() {
    Widget w{1, 2};   // OK only under FEATURE_X's layout
    return w.compute();
}

// tu_without_feature.cpp
#include "bad.h"      // FEATURE_X NOT defined here
extern int get_with_feature();  // declared elsewhere with the *other* Widget in mind
int get_without_feature() {
    Widget w{1};
    return w.compute();
}

int main() {
    return get_with_feature() + get_without_feature();
}
```
Build (MSVC): `cl /std:c++20 /EHsc /c tu_with_feature.cpp` then `cl /std:c++20 /EHsc /c tu_without_feature.cpp` then `cl tu_with_feature.obj tu_without_feature.obj /Fe:prog.exe`. Build (GCC/Clang): `g++ -std=c++20 -c tu_with_feature.cpp -o a.o && g++ -std=c++20 -c tu_without_feature.cpp -o b.o && g++ a.o b.o -o prog`. Both compile and link *without any diagnostic*, because each TU is individually well-formed and the linker never compares `Widget`'s two conflicting definitions token-for-token — it isn't required to. At runtime, `get_with_feature()`'s `Widget{1, 2}` is built and read back using the "with FEATURE_X" `sizeof`/layout, while `get_without_feature()`'s `Widget{1}` uses the "without" layout; if any code in one TU ever treats a `Widget` object as though it had the other TU's layout (e.g. by aggregating them across a shared function boundary compiled once with one macro state), reads/writes land at the wrong offsets — this is exactly the "no diagnostic required" silent-UB category the ODR's second form describes: the program built and ran with no error at any stage, yet different parts of it silently disagree about what `Widget` is.

---

**08-P17.**
```cpp
// box.h
template<typename T> class Box {
public:
    explicit Box(T v);
    T get() const;
private:
    T value_;
};

// box.cpp
#include "box.h"
#include <string>
template<typename T> Box<T>::Box(T v) : value_(v) {}
template<typename T> T Box<T>::get() const { return value_; }
template class Box<int>;
template class Box<std::string>;

// client.cpp
#include "box.h"
#include <string>
#include <iostream>
int main() {
    Box<int> bi(42);
    Box<std::string> bs("hello");
    std::cout << bi.get() << " " << bs.get() << "\n";
}
```
`client.cpp` never sees `Box`'s member bodies — only the declarations in the header — yet compiles and links successfully against `box.cpp`'s object file, because the explicit instantiations `template class Box<int>;` and `template class Box<std::string>;` force the compiler, while compiling `box.cpp`, to emit full concrete definitions for those two specializations' member functions into `box.obj`, which the linker then resolves `client.obj`'s calls against. `Box<double>` would compile fine in `client.cpp` (the compiler only needs the declaration to type-check the call), but fail to *link*, because no explicit instantiation of `Box<double>` exists anywhere, and the compiler never implicitly instantiates it from `client.cpp` (it can't — it never sees the member bodies) — so the linker reports an unresolved symbol for `Box<double>::Box` and `Box<double>::get`.

---

**08-P18.**
```cpp
// logger.h
#include <memory>
#include <string>
class Logger {
public:
    Logger();
    ~Logger();
    void log(const std::string& msg);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// logger.cpp (version 1)
#include "logger.h"
#include <vector>
struct Logger::Impl { std::vector<std::string> history; };
Logger::Logger() : impl_(std::make_unique<Impl>()) {}
Logger::~Logger() = default;
void Logger::log(const std::string& msg) { impl_->history.push_back(msg); }

// logger.cpp (version 2 — added a field, rebuilt logger.cpp ONLY)
struct Logger::Impl { std::vector<std::string> history; bool verbose = false; };
// rest unchanged
```
Only `logger.cpp` is recompiled; `client.cpp` (which only includes `logger.h` and calls `Logger`'s public members) is untouched and still links and runs correctly, because the client's compiled code never depended on `Impl`'s size or layout — it only ever holds a `std::unique_ptr<Impl>`, a fixed-size pointer, and calls through `Logger`'s stable public interface. Had `Impl`'s fields (`history`, then the added `verbose`) instead been exposed directly as `Logger`'s own private data members, the client's compiled code (which embeds `Logger` by value, or takes `sizeof(Logger)` implicitly via any local variable, member, or `new Logger`) would have baked in the *old* `sizeof(Logger)` and member offsets; adding a field would change `Logger`'s actual size/layout, and the unmodified client binary would then be silently operating on a stale size/offset assumption the moment it's linked against the rebuilt code — exactly the ABI break pimpl is designed to prevent.

---

**08-P19.**
```cpp
// process.cpp
void process(int) {}
void process(double) {}
namespace ns { void process(int) {} }
```
Build: `cl /std:c++20 /c process.cpp` then `dumpbin /symbols process.obj` (MSVC), or via WSL `g++ -std=c++20 -c process.cpp -o process.o && nm -C process.o`. The three mangled names (Itanium/GCC form) are `_Z7processi` (`process(int)`), `_Z7processd` (`process(double)`), and `_ZN2ns7processEi` (`ns::process(int)`) — `nm -C` demangles these back to `process(int)`, `process(double)`, `ns::process(int)`. The linker distinguishes all three because the mangled encoding embeds the parameter type (`i` for `int` vs. `d` for `double`, disambiguating the two top-level overloads) and the enclosing namespace (`N2ns...E` wrapping the third), so despite sharing the identifier `process`, each produces a textually distinct symbol.

---

**08-P20.** Static lib: `cl /std:c++20 /c square.cpp` then `lib square.obj /OUT:mathstatic.lib` (MSVC), or `g++ -c square.cpp -o square.o && ar rcs libmathstatic.a square.o` (GCC). Shared lib: on MSVC, `cube.cpp` declares `__declspec(dllexport) int cube(int);`, built via `cl /std:c++20 /LD cube.cpp /Fe:mathshared.dll` (producing `mathshared.dll` + `mathshared.lib` import lib); on GCC/Clang, default visibility suffices: `g++ -shared -fPIC cube.cpp -o libmathshared.so`. Final link: `cl main.cpp mathstatic.lib mathshared.lib` (MSVC) or `g++ main.cpp -L. -lmathstatic -lmathshared -o main` (GCC). Inspecting the build log/output executable size shows `square`'s object code physically copied into `main.exe`/`main`'s own binary (it came from the static archive, resolved entirely at link time), while `cube`'s code stays in the separate `mathshared.dll`/`libmathshared.so` file, loaded into the process only when `main` runs — `main`'s binary only records an import-table entry (and, on MSVC, links against the small `.lib` import stub) referencing `cube`, not `cube`'s actual machine code.

---

**08-P21.** Implicit-instantiation build: each of the three `.cpp` files independently instantiates `log_value<int>` and `log_value<std::string>` in its own object file, so `dumpbin /symbols` / `nm -C` on all three `.obj`/`.o` files shows three separate (individually complete) copies of `log_value<int>`'s compiled code — three times the compile-time cost for that one instantiation, deduplicated only at final link time. `extern template`-based build: with `extern template void log_value(const int&);` in the header and one explicit instantiation definition in a single `.cpp` file, only that one `.cpp` file's object file contains `log_value<int>`'s actual compiled code; the other two TUs' object files contain no `log_value<int>` definition at all (just a reference to the external one), so their object files are smaller and compile faster for that instantiation. The observed difference: total object-file footprint (and total compile work) for the `int` instantiation drops from "duplicated in every using TU" to "compiled exactly once," at the cost of needing to remember to keep that one explicit-instantiation TU in sync with actual usage — while `log_value<std::string>` (not covered by `extern template` in this scenario) still gets implicitly instantiated redundantly in all three TUs.

---

**08-P22.**
```cpp
// widget_api.h
extern "C" {
    void* create_widget();
    void destroy_widget(void* w);
    int widget_value(void* w);
}

// widget_api.cpp
#include "widget_api.h"
class WidgetImpl { public: int value = 42; };
extern "C" void* create_widget() { return new WidgetImpl(); }
extern "C" void destroy_widget(void* w) { delete static_cast<WidgetImpl*>(w); }
extern "C" int widget_value(void* w) { return static_cast<WidgetImpl*>(w)->value; }
```
The client only calls `create_widget`/`widget_value`/`destroy_widget` through `void*` handles, never seeing `WidgetImpl`'s definition. This sidesteps the name-mangling problem because `extern "C"` gives each function a plain, unmangled symbol name any compiler/language can call by name, regardless of mangling-scheme differences. It sidesteps the layout-ABI problem because the *only* thing crossing the boundary is an opaque `void*` (whose size and meaning is fixed and trivial — a pointer) — the client never embeds `WidgetImpl` by value, never computes its `sizeof`, and never depends on its member layout or vtable shape, so `WidgetImpl` can change arbitrarily (add members, add virtuals, change its own internal class hierarchy) without breaking any already-compiled client, and the boundary works identically even if client and library were built with two entirely different C++ compilers.

---

**08-P23.** Version B (the output-parameter, fundamental-types-only form) is more robust to a different-compiler client. Version A returns a `struct Point` by value, which depends on both compilers agreeing on the struct's layout (generally fine for a simple aggregate of two `int`s, per the common-subset guarantees most platform ABIs make for POD-like structs) *and*, more fragile-in-practice, on both compilers using the *same calling convention* for how a small struct is returned by value (register-based vs. memory-based return, which register(s)) — a convention detail the C++ standard doesn't fix and that can differ between compiler/platform ABI documents. Version B passes only `int*` — pointers to fundamental types — whose representation and calling-convention treatment is part of the platform's C ABI, which essentially every C++ compiler on a given platform agrees to honor (it's the lowest common denominator every compiler must interoperate with for C interop). By avoiding by-value struct-return convention entirely, version B removes one whole axis of potential compiler disagreement.

### Level 4 — Debugging

**08-P24.** [DEBUG] The `#pragma pack(1)` wrapped around `b.cpp`'s `#include` changes how the compiler lays out `Widget` *in that TU only* — even though the header's token sequence is byte-for-byte identical in both TUs — because the packing pragma is a compiler directive that changes the *effective* class definition (its actual size/alignment/offsets) without changing a single character of the included text. This violates the ODR's requirement that a class defined in multiple TUs have *identical* definitions — "identical" here means identical in the compiler's actual interpretation (including in-effect pragmas and macros), not merely identical source text as blindly diffed. The compiler cannot diagnose this because ODR compliance is a whole-program property spanning translation units compiled in *separate, independent* compiler invocations — no single compilation ever sees both `a.cpp`'s and `b.cpp`'s pragma state at once to notice the discrepancy, and the standard does not require (or make practically possible) a cross-TU consistency check for this.

---

**08-P25.** [DEBUG] This is a binary-compatibility violation, not (necessarily) a source-compatibility one — the header (and thus the client's source-level view) is literally untouched, so source compatibility is fully preserved; the client wouldn't even need to recompile to keep *compiling*. What breaks ABI despite an unchanged public signature is most likely the function's parameter itself: `std::vector<int>` (or any standard-library container) has its own internal representation as part of the C++ standard library's ABI, and if the "different container" change altered assumptions the caller and callee make about how the argument is passed/constructed at the ABI boundary (e.g. if the change involved a different allocator, a different internal layout assumption leaking through an inlined/templated call path, or — more subtly — if the change altered stack frame expectations, exception-handling tables, or calling convention in some compiler-specific way) the two sides disagree about the binary contract despite agreeing about the source-level signature. In general: any implementation change that alters what the function actually *does* at the binary level with its arguments/stack/registers — even under an unchanged declared signature — can break already-compiled callers, which is exactly why "only changed the .cpp, not the header" is not a safe binary-compatibility argument on its own.

---

**08-P26.** [DEBUG] `inline`'s ODR contract requires the definition to be *identical* in every TU where it's defined — and building the same source text with `/fp:fast` in one TU and default (`/fp:precise`) floating-point semantics in another produces two definitions that are only identical at the *source* level, not at the level the standard actually cares about: the flag changes what floating-point operations the compiler is permitted to assume/reorder/approximate, so the compiled code (and, potentially, the observable numerical result) for `square()` genuinely differs between the two TUs even though not one character of source differs. This is more insidious than the macro-guarded case because there's no textual difference anywhere for a code reviewer (or even a diff tool) to spot — the divergence lives entirely in build configuration invisible from the source files themselves — and it can produce results that are merely *sometimes* different (only when the whichever-TU's-copy-the-linker-happened-to-keep is the one whose fast-math assumptions actually affect that particular input), making it far harder to reproduce or root-cause than an `#ifdef`-visible difference.

---

**08-P27.** [DEBUG] The "appending is always binary-compatible" assumption fails because binary compatibility for virtual functions depends on vtable *slot* stability, and while appending to a single, simple (single-inheritance) vtable does preserve existing slots' indices, that guarantee does not automatically extend to more complex inheritance layouts: under multiple inheritance, a derived class typically carries multiple vtable pointers (one per polymorphic base subobject, in implementation-defined but consistent-per-compiler arrangements), and under virtual inheritance, the shared virtual base's vtable-related bookkeeping (virtual base table / offset adjustments) can be laid out in ways where "appending" to `Base` shifts where subsequent bases' vtables or vtable-pointer-adjustment thunks are expected to sit relative to the object, depending on the specific compiler's ABI. A crash calling seemingly unrelated, unmodified virtual functions is consistent with a vtable-pointer or thunk-offset miscalculation stemming from this kind of layout shift, not from the appended function itself being called incorrectly. Confirming evidence: dumping both the old client's and the new library's actual vtable contents/offsets (via a debugger or ABI-inspection tool) for the affected class hierarchy and showing that a given virtual function's slot index or vtable-pointer offset differs between what the client binary was compiled to expect and what the new library's object layout actually provides — a concrete slot/offset mismatch, not merely "it crashes," is the confirming evidence.

---

**08-P28.** [DEBUG] The link fails with a multiple-definition error for `helper` — both `libA` and `libB` are just archives containing the same object file with the same external-linkage, non-inline, non-template definition of `int helper()`, and the final link step pulls in both archives' object code, presenting the linker with two definitions of the same symbol, which is exactly the diagnosable, first form of ODR violation. This differs from the inline/template/macro-driven cases because there, the entities involved (`inline` functions, template instantiations, or macro-varied definitions across headers) are legally *allowed* to have multiple, textually-identical-or-ODR-exempt copies across TUs, so the linker's normal job is to silently deduplicate them, not flag them — the standard requires no diagnostic precisely because those categories are expected to appear more than once by design. Here, `helper` carries none of those exemptions; it's an ordinary externally-linked function that the ODR flatly requires to have exactly one definition in the entire program, so the linker's ordinary duplicate-symbol detection catches it immediately and loudly.

---

**08-P29.** [DEBUG] `-fvisibility=hidden` changes the *default* export visibility of every symbol in the shared library to hidden (i.e., not exported in the library's dynamic symbol table) unless a symbol is explicitly marked otherwise. Since `public_api` was never annotated with `__attribute__((visibility("default")))` (or an equivalent export macro), it inherits the hidden default just like every other symbol, so it is defined and present inside the shared library's own code, but it never appears in the library's exported-symbol table that the dynamic linker consults when resolving a client's references — hence "undefined symbol" at the client's link/load step, despite `public_api` unquestionably existing in the library's source and object code. The MSVC-equivalent mistake is forgetting `__declspec(dllexport)` on the function's declaration when building the DLL (or omitting the corresponding entry in a `.def` file) — MSVC's default is also "don't export" unless told to, so the same symptom (function compiles fine into the DLL, but the client's linker reports it as an unresolved external symbol) results from the same underlying category of mistake: default-hidden visibility with a missed explicit "please export this one" annotation.

---

**08-P30.** [DEBUG] `std::unique_ptr<Impl>`'s destructor, when instantiated, must be able to call `delete` on the pointed-to `Impl*`, which requires `Impl` to be a *complete* type at the point that destructor is instantiated — because `delete` on an incomplete type is itself ill-formed (the compiler needs to know `Impl`'s size and whether it has a non-trivial destructor to generate correct deletion code). Writing `~Widget() = default;` directly in the header causes `Widget`'s destructor (and therefore, implicitly, `unique_ptr<Impl>`'s member destructor call) to be *instantiated right there, in the header*, at a point where `Impl` is still only forward-declared (incomplete) — hence the "incomplete type" compile error in any client TU that only includes the header. The one-line fix: declare `~Widget();` in the header (no `= default` there) and define it — even trivially, as `Widget::~Widget() = default;` — in the `.cpp` file, at the point where `Impl`'s full definition is visible, so the destructor's instantiation (and its implicit `unique_ptr<Impl>` destruction) happens only where `Impl` is complete.

---

**08-P31.** [DEBUG] This diamond-dependency scenario is invisible to either `liba`'s or `libb`'s own tests in isolation because each one, tested alone (or tested together with only the *version* of `libcommon` it was itself built against), behaves perfectly correctly — the incompatibility only exists in the specific combination where the *other* library's incompatible expectation of `libcommon`'s layout is also loaded into the same process and the dynamic loader has unified both onto one single physical copy of `libcommon`. Neither library's test suite exercises "loaded simultaneously with a sibling library built against a different, layout-incompatible version of our shared dependency," so the landmine only detonates in the specific executable that actually loads both `liba` and `libb` together at runtime. The class of tool that could reveal this before it manifests as a crash is symbol/ABI-inspection tooling from this chapter — `dumpbin /dependents` / `nm -C` / `objdump -T` / `readelf -d`, used to compare the exported symbol lists, mangled type/size signatures, or dependency versions each of `liba` and `libb` actually links against for `libcommon` — surfacing the version/layout mismatch as a static, pre-runtime comparison rather than waiting for the loader to unify them and crash.

### Level 5 — Integration

**08-P32.** Propose two coexisting types: keep the existing class as a stable, pimpl'd, ABI-insulated general-purpose type for the majority of call sites (the ones that have never actually measured a performance problem), and introduce a separate, explicitly-named type (e.g. `WidgetFast`, or a namespace like `hot_path::Widget`) that exposes its representation directly and is documented as ABI-unstable, reserved for the specific consumers who have measured an actual, load-bearing need for the indirection-free access. The two types can share underlying logic via a common non-virtual implementation helper or a template, so behavior doesn't diverge — only the ABI-exposure surface differs. The measurement that should decide whether the performance concern is still load-bearing is a profiled, representative before/after benchmark of the actual hot-path call sites with pimpl's extra indirection introduced (not a microbenchmark of the indirection in isolation, and not intuition) — if that measured difference is within noise or below whatever latency/throughput budget the hot path is actually held to, the "unacceptable overhead" premise from when the library was first written may simply no longer hold, and the fast-path escape type may be unnecessary for most or all of today's consumers.

---

**08-P33.**
```cpp
// cache.h
#include <vector>
template<typename T> class Cache {
public:
    void put(T v) { items_.push_back(v); }
    T get() const { return items_.empty() ? T{} : items_.back(); }
private:
    std::vector<T> items_;
};

// tu1.cpp / tu2.cpp — each does:
#include "cache.h"
void use_cache() { Cache<int> c; c.put(1); c.put(2); volatile int v = c.get(); }
```
Compiling both TUs and running `dumpbin /symbols tu1.obj` / `nm -C tu1.o` (and the same for `tu2`) shows each object file containing its own symbol for `Cache<int>::put` (mangled, e.g. Itanium form `_ZN5CacheIiE3putEi`, demangled by `nm -C` to `Cache<int>::put(int)`) — present, independently, in both object files. The linker merges these into a single copy in the final executable without a multiple-definition error because class template member functions are implicitly treated like `inline` functions: ODR permits an inline-equivalent entity to have identical definitions in multiple TUs specifically so the linker can deduplicate them, and since both TUs instantiated `Cache<int>::put` from the same template definition (and the standard requires such instantiations to be equivalent), the linker is within its rights — and expected — to keep exactly one copy, exactly as it would for an ordinary `inline` function.

---

**08-P34.** The factory function must be `extern "C"` because the host looks it up *by name* via `GetProcAddress`/`dlsym`, which require an exact, predictable, unmangled symbol string to search for — if the factory were an ordinary C++ function, its actual exported symbol would be a compiler- and signature-dependent mangled name the host would have no portable way to construct or guess (and which could change if the factory's signature changed at all). `extern "C"` only disables *name mangling for the lookup mechanism* — it does not, and does not need to, strip C++-ness from what the function *returns*; `Plugin*` is still a full, polymorphic C++ pointer once past the lookup step, and ordinary virtual dispatch on it works completely normally after the call returns. What would break if host and every plugin weren't built with the same compiler/ABI for `Plugin`'s layout specifically: any mismatch in `Plugin`'s vtable layout, the exact bytes of its vtable pointer's position in the object, or its size — e.g. if one plugin's compiler add(ed) a different set of hidden fields (RTTI info, virtual base pointers) or ordered virtual functions differently than the host expects — would cause `run()`'s virtual dispatch (a vtable-pointer-relative lookup entirely below the C-linkage boundary) to jump to the wrong function or crash, exactly the vtable-slot-order/layout fragility discussed earlier in this chapter, since the C-linkage boundary only protects the *lookup* of the factory symbol, not the ABI of the C++ type it hands back.

---

**08-P35.** Build `header.h` with `inline double compute(double x) { return (x + 1e-20) - x; }` (a computation whose result is sensitive to fast-math reassociation) included, unmodified, by `tu_fast.cpp` (compiled with `/fp:fast` or `-ffast-math`) and `tu_precise.cpp` (compiled without). Running the resulting program shows `compute()` returning different values depending on which TU's linked-in copy actually executes for a given call site — a build-configuration-driven ODR violation of the second (silent) form, analogous to 08-P26, because the source text is identical but the *effective*, compiler-flag-influenced definition differs. Concrete fix: change the build system (e.g. the CMake target's compile options) so that *every* TU which includes this header is compiled with the same floating-point flag setting — either apply `/fp:fast`/`-ffast-math` uniformly to the whole target (or, more conservatively, to no TU that includes shared fast-math-sensitive inline headers, isolating fast-math to files that don't share such headers) — eliminating the divergence by making the "identical token sequence" property also hold at the effective-compiled-definition level, which is what ODR actually requires.

### Level 6 — Production

**08-P36.** Concrete practices: (1) adopt pimpl (or an equivalent opaque-handle/interface-only pattern) for every class whose layout the library doesn't want to freeze forever, so internal representation changes never touch client-visible size/layout; (2) treat "append-only" as the only safe modification to any still-exposed virtual function table, and never reorder, remove, or insert virtual functions among existing ones in a released `Base` class; (3) never change a public struct/class's data-member layout, size, or alignment once released, except via a new, separately-named type; (4) document and enforce a fixed calling convention and exception-handling model across the boundary, and pin the C++ standard-library ABI version so client and library agree; (5) any function signature change is itself a new symbol (new name or new overload the linker can distinguish), never a silent reinterpretation of an existing one. A concrete pre-release CI check: automatically dump and diff the exported symbol list (and, ideally, `sizeof`/offset metadata for exposed types) between the previous released version and the release candidate, via `dumpbin /exports` or `nm -C -D`/`readelf --dyn-syms`, failing the build if any previously-exported symbol disappears, changes its mangled signature, or (for types with available debug info) changes its size — turning "did we accidentally break ABI" from a manual-review judgment call into an automated, mechanical gate.

---

**08-P37.** Refactoring plan: profile which specific template instantiations (by concrete type arguments) actually dominate the redundant-instantiation cost across the codebase, then for each such instantiation, add an `extern template` declaration for it in the God header (or a narrower header specifically for that instantiation) plus exactly one explicit-instantiation-definition `.cpp` file that provides the real instantiation once; every other TU that only *uses* that instantiation now sees the `extern template` declaration and skips its own redundant copy, moving that portion of the compile cost from "every TU" to "one TU, plus every TU's now-cheaper reference to it." What this plan does *not* solve: it addresses only the redundant re-instantiation cost of the handful of hot instantiations specifically targeted — it does nothing for the God header's own parsing/preprocessing cost (every TU still has to lex and parse the entire header's declarations), for build-graph-level costs like unnecessary rebuild cascades when the God header changes (a build-system/dependency-structuring problem, previewing Ch10's territory), or for instantiations that remain numerous/varied enough that no small fixed set of `extern template`s covers them. A measurable way to confirm the change actually reduced total build time (rather than moving cost around): compare wall-clock full-rebuild time (and, more precisely, aggregate per-TU compile time via the build system's own timing/`-ftime-trace`-style reports) before and after, on the same machine/load conditions, specifically checking that the *total* time dropped rather than merely shifting time into the one new explicit-instantiation TU by an amount that offsets the savings elsewhere.

### Level 7 — Principal Reasoning

**08-P38.** None of the three is unconditionally correct; the right choice is driven by three questions asked of this specific library. First: how fast does it change, and how tolerant are consumers of taking new versions? A library that changes rarely and whose consumers upgrade in lockstep tolerates (a) or (b) fine; a library that changes often, or whose consumers can't coordinate synchronized upgrades, benefits from (c)'s ability to patch centrally — but only if (c)'s ABI-versioning discipline (per 08-P36) is actually staffed and enforced, since a poorly-maintained shared-library ABI promise is worse than either alternative. Second: how diverse are consumers' toolchains/build flags? If consumers span multiple compilers, standard-library versions, or incompatible build configurations (debug/release CRT, different `/fp` settings, etc.), (a) source-only sidesteps ABI entirely (each consumer just compiles it themselves, under their own toolchain) at the cost of everyone re-paying the build-time cost and losing any single-point-of-patching ability; (b) requires building and shipping one static-library artifact per supported toolchain/configuration combination, which is a real, ongoing packaging cost that scales with toolchain diversity; (c) requires the strictest, hardest-to-maintain promise (a stable ABI across that same diversity) and is the worst fit if toolchain diversity is high and uncontrolled. Third: what's the organization's actual patching/release cadence and appetite for coordinating rebuilds versus deploying centrally? A security-sensitive, frequently-patched library used by many independent teams' *already-running* services favors (c) specifically because it's the only option that lets one central rebuild reach all consumers without each team's own rebuild-and-redeploy cycle. The decision process: measure (or estimate) the library's actual rate of breaking/patch-worthy change, survey consumers' toolchain diversity, and identify who bears the cost of a slow patch reaching production — then pick source-only if toolchain diversity is high and coordinated ABI maintenance isn't realistically staffed, static if toolchain diversity is manageable and consumers upgrade in a controlled, infrequent cadence, and shared/ABI-versioned only if the organization is willing and able to actually run the ABI-stability practices from 08-P36 in perpetuity.

---

**08-P39.** Inserting a new virtual function *between* two existing ones shifts every subsequent virtual function's vtable slot index down by one relative to what already-compiled client binaries were built expecting — every client call through a slot index at or after the insertion point now lands on the wrong function (or on the newly inserted one instead of the one the client meant to call), a far larger blast radius than appending, which by construction only adds a new slot at the end and leaves every existing slot's index untouched (setting aside the multiple/virtual-inheritance caveats from 08-P27, appending is comparatively safe precisely because it never renumbers anything). This is categorically worse, not just "also somewhat risky," because insertion guarantees breakage for every existing virtual call at or past that slot, in every already-compiled client, the moment the client relinks against the new binary — there is no configuration in which mid-list insertion is ABI-safe, whereas appending has a real, well-understood safe path. Two concrete alternatives: (1) append the new virtual function at the end of `Base`'s declaration regardless of "logical grouping" concerns, and address the grouping/readability goal purely at the *source* level (e.g. through documentation, or through a derived interface/mixin that groups related declarations without altering `Base`'s existing slot order); (2) introduce a new, separate interface (e.g. `BaseExtended : public Base`) that adds the new virtual function, and have `Base`-returning factories/APIs optionally return a `BaseExtended*` (obtainable via a `dynamic_cast` or a dedicated query method) for clients that want the new capability, leaving `Base`'s own vtable completely untouched for every existing client. Process/evidence to make this mistake structurally hard to ship by accident: an automated ABI-diffing tool in CI (as proposed in 08-P36) that specifically flags any change in an existing virtual function's slot index between the previous release and the candidate — not just "was a symbol removed," but "did any existing symbol's *position* in its class's vtable change" — turning this exact mistake into a hard CI failure rather than something that depends on a reviewer noticing "inserted, not appended" during code review.

## Integration Challenge Solution — 08-IC1

1. Running `dumpbin /exports core.dll` / `dumpbin /dependents plugin.dll` (or `nm -C -D libcore.so`, `objdump -T libplugin.so`, `readelf -d libplugin.so` on the Itanium-ABI side) lists every symbol `libcore` exports and every external symbol `libplugin` imports; intersecting the two lists identifies exactly which `libcore` symbols — most importantly, any constructor, method, or vtable-related symbol for the shared class(es) that cross the boundary — `libplugin` actually resolves against at load time.

2. A missing or renamed symbol produces a load-time "unresolved external symbol" error the moment the dynamic loader tries to resolve `libplugin`'s imports against `libcore`'s exports — the process would fail to even start running the code that eventually crashes. Since the program instead starts, runs, and only crashes *inside a virtual call* on an object `libcore` constructed and handed to `libplugin`, the failure mode is downstream of successful symbol resolution: both libraries agree the symbol (e.g. a class's vtable-associated symbols, or the constructor) exists and is present, but disagree about the *shape* behind that symbol — i.e. the actual vtable slot layout, object size, or member offsets for the shared class — so the virtual dispatch computed by one library's compiled expectations lands on the wrong slot or offset once fed an object built to the other library's (different) layout.

3. Further evidence to gather: dump and compare the demangled signature and, where debug info is available, the reported size/offset metadata for the shared class as seen from each library's own build (via `dumpbin /symbols` with type info, or `nm -C`/a debugger's type-info query against each binary independently); check for a flag mismatch between the two libraries' build configurations — different struct-packing pragmas, different C++ standard-library ABI version, different exception-handling model (e.g. one built with `/EHsc` assumptions inconsistent with the other), or one library built against a different version of a shared header that changed the class's layout (the exact scenario from 08-P31's diamond-dependency case, here manifesting directly between the two libraries themselves rather than through a third one).

4. Without source access, no fix is possible from the binaries alone — an ABI/layout mismatch between two already-compiled binaries cannot be patched without recompiling at least one of them against a shared, consistent definition of the class (and its layout-affecting build flags); binary patching a vtable or offset mismatch reliably, in general, isn't a real option. The actual fix requires rebuilding at least one (ideally both) of `libcore`/`libplugin` against a single, shared, version-pinned header and consistent build configuration for the class that crosses the boundary. The process change to prevent recurrence ties directly to 08-P36 and 08-P38: adopt (and enforce via CI, per 08-P36's ABI-diffing gate) a documented ABI-stability promise and automated symbol/layout comparison between releases of any type crossing this boundary, and revisit the distribution-model decision from 08-P38 for how `libcore` and `libplugin` are versioned and shipped relative to each other — e.g., pinning both to build against the exact same versioned shared-header artifact rather than allowing them to drift independently, which is precisely the condition that let this mismatch occur unnoticed until runtime.
