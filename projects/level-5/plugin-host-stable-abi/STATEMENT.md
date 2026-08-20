# P-5.6 — Plugin Host with a Stable C ABI Boundary

**Level:** 5 (Production-style) · **Category:** Systems · **Requires:** Ch01–10 · **Est. effort:** L (16-24h)

## Objective

Build a plugin host that loads shared libraries (`.so`/`.dll`) at runtime, communicates with them across a deliberately C-only ABI boundary (no C++ types crossing it — no `std::string`, no virtual dispatch, no exceptions), supports safe unloading, versions the interface, and diagnoses symbol-mismatch failures with actionable errors rather than crashes.

This project is structured in phases. Each phase has its own exit bar; don't move on until the current one is met.

## Phase 1 — Load and Call

Get basic dynamic loading and symbol resolution working on one platform: load a plugin, resolve the entry point, call into it. Exit bar: a minimal example plugin is loaded, called, and produces the expected result.

## Phase 2 — Cross-Platform C ABI

Port to the other platform and lock down the interface to pure C types. Exit bar: the same example plugin's source builds as both a `.so` and a `.dll` from one shared interface header, with no C++ type crossing the boundary.

## Phase 3 — Versioning

Add the version field and the host-side compatibility check. Exit bar: a deliberately version-mismatched plugin is rejected with a clear error at load time, not used and left to crash or corrupt state later.

## Phase 4 — Safe Unload

Add unloading and verify no dangling function pointers remain callable afterward. Exit bar: a test loads, calls, unloads, and then confirms (e.g. via a guard/sentinel mechanism) that no host code path can still invoke the unloaded plugin's code.

## Phase 5 — Failure Diagnostics

Harden symbol-resolution and malformed-plugin failure paths into clear, actionable errors. Exit bar: a missing-entry-point plugin and a plugin returning a null interface pointer each produce a distinguishable, documented error at load time — verified by a test for each — with no null-pointer dereference in either case.

## Functional Requirements

1. Load a plugin (`.so` via `dlopen` on Linux, `.dll` via `LoadLibrary` on Windows) given a file path, resolve a well-defined entry-point symbol, and obtain a handle to the plugin's exposed functionality through that entry point.
2. The plugin interface crossing the ABI boundary must be pure C: a `struct` of function pointers (or equivalent) using only C-compatible types (fixed-width integers, `const char*`, raw pointers, C-layout structs) — explicitly no `std::string`, `std::vector`, virtual functions, exceptions, or any type whose layout/ABI is compiler-version-dependent.
3. Interface versioning: the plugin interface struct includes a version field, and the host explicitly checks it against the version(s) it supports before using any other part of the interface, rejecting an incompatible plugin with a clear error rather than proceeding and risking undefined behavior from a layout mismatch.
4. Safe unloading: a loaded plugin can be unloaded (`dlclose`/`FreeLibrary`) after the host has released all references into it, without leaving dangling function pointers callable from host code and without crashing if the plugin's own cleanup routine is well-behaved.
5. Symbol resolution failure (missing entry point, or the entry point returning a null/invalid interface pointer) is reported as a clear, distinguishable load-time error, not a null-pointer dereference at first use.
6. At least one working example plugin (built as a separate shared library within this project) demonstrating the full lifecycle: load, call into it, unload — plus a deliberately version-mismatched or malformed example plugin demonstrating the rejection path.

## Input

A plugin file path; calls into the plugin's exposed interface once loaded, using arguments restricted to the C-ABI-safe types the interface declares.

## Output

Success/failure of load, the plugin's interface (a function-pointer struct) on success, and the results of calling into that interface; a clear diagnostic message on any failure mode.

## Constraints

- C++20 for the host; the plugin interface header itself must be valid, unambiguous C (or a strict C-compatible subset expressible in a `extern "C"`-guarded C++ header) — since real-world plugin ABIs need to be usable from plugins written in C, Rust, or other languages, not only C++.
- No C++ exception may cross the plugin boundary in either direction — a plugin's internal exception must be caught inside the plugin before returning to the host, and the host's calls into the plugin must not allow a C++ exception to propagate through a C function-pointer call, which is undefined behavior.
- Must work correctly on both Linux (`dlopen`/`dlsym`/`dlclose`) and Windows (`LoadLibrary`/`GetProcAddress`/`FreeLibrary`), including their differing default symbol visibility (ELF exports by default vs. Windows requiring explicit `__declspec(dllexport)`/a `.def` file) and search-order behavior.

## Edge Cases

- Loading a plugin file that exists but isn't actually a valid shared library for this platform (e.g. wrong architecture, corrupted file) — a clear load-time error, not a crash.
- A plugin's entry-point symbol exists but returns a null interface pointer (a legitimate way for a plugin to signal "I refuse to load," e.g. due to an internal check failing) — handled as a clean rejection, not a null dereference.
- Calling into a plugin's interface function after the plugin has been unloaded (a use-after-unload bug in host code) — out of scope to fully defend against at the language level (the function pointer is simply invalid memory after unload, same as any dangling pointer), but the host's own API design should make this mistake hard to make by accident (e.g. an RAII handle whose function-pointer accessors become unusable after the handle scope ends).
- Two different plugins exporting symbols with the same name, loaded simultaneously — must not interfere with each other (each `dlopen`/`LoadLibrary` handle's symbols are resolved independently), verified explicitly since symbol visibility/interposition behavior differs meaningfully between ELF and PE.
- A plugin whose interface version is *newer* than what the host supports (forward-incompatible) versus *older* (the host might support backward compatibility for older versions) — the versioning policy must explicitly define which of these are accepted and which are rejected.

## Error Handling

- `dlopen`/`LoadLibrary` failure (file not found, load error) — the underlying platform error message (`dlerror()` on Linux, `GetLastError()`/`FormatMessage` on Windows) should be surfaced in the host's error, not swallowed, since it's often the only clue to a real-world plugin-loading failure.
- A version-mismatched plugin — rejected with an error message identifying both the plugin's reported version and the host's supported version(s), not a generic "failed to load."

## Acceptance Criteria

- The working example plugin's full lifecycle (load, call, unload) is demonstrated and tested.
- The version-mismatch rejection path is demonstrated against a deliberately incompatible example plugin, with a clear diagnostic message.
- A test confirms that no C++ exception thrown inside the example plugin escapes across the boundary uncaught (the plugin catches its own exceptions at the boundary and translates them to a C-compatible error signal).
- The host runs correctly on both Linux and Windows against platform-appropriate example plugin builds.
- The null-interface-pointer rejection path and the invalid-shared-library-file rejection path are both demonstrated with clear, distinguishable errors.

## Testing Requirements

- The full load/call/unload lifecycle test against the working example plugin.
- The version-mismatch rejection test.
- The null-interface-pointer rejection test.
- The invalid-file (not a real shared library) rejection test.
- The exception-does-not-cross-boundary test.
- A same-symbol-name-in-two-simultaneously-loaded-plugins non-interference test.

## Hints

### Hint 1 — Direction
Design the plugin interface as a plain C struct of function pointers, with a version field as its very first member (so even a wildly incompatible plugin's interface can still have its version checked safely, before touching anything else that might have a different layout) — write this struct's header as the one artifact that both the host and every plugin depend on, and treat any C++-specific type appearing in it as an immediate design error to fix, not a convenience to allow "just this once."

### Hint 2 — Technique
Every plugin-exposed function that might, internally, throw a C++ exception must have its *outermost* layer be a `try`/`catch(...)` block that translates any exception into a C-compatible error code or out-parameter before returning across the boundary — this translation happens inside the plugin's own code (which the plugin author, using this interface, is responsible for), while the host-side calling code should never assume it's safe to let an exception propagate through a function-pointer call it's making into unknown, possibly-non-C++ code.

### Hint 3 — Implementation
For safe unloading, wrap the raw `dlopen`/`LoadLibrary` handle and the resolved interface pointer together in one RAII type whose destructor calls `dlclose`/`FreeLibrary`, and whose interface-accessor method is the only way host code obtains the function-pointer struct — if that RAII type's destructor has run, the object providing access to the interface no longer exists, which is a natural way to make "call into a plugin after it's been unloaded" require actively working around your API rather than being an easy accident.

### Hint 4 — Debugging/Design
If your Linux build works but the Windows build's symbol resolution fails to find your plugin's entry point (or vice versa in spirit), check your export visibility — ELF shared libraries export all non-static symbols by default, which can mask a missing `extern "C"` or a subtly wrong function signature that "happens to work" on Linux, while Windows's default of exporting nothing unless explicitly marked (`__declspec(dllexport)` on the plugin side, matched by `__declspec(dllimport)` or nothing needed on the loading side since you're using `GetProcAddress` rather than static linking) will surface exactly this kind of mismatch immediately as a clear symbol-not-found error — treat the Windows build failing where Linux "succeeds" as valuable information about a real portability gap, not as a Windows-specific annoyance to work around.
