# Chapter 10 — Build Systems, Testing Infrastructure, and CI

## Crash Course

### CMake Targets and Visibility

Modern CMake is target-centric, not variable-centric: you declare a target (`add_library`, `add_executable`) and attach properties to it (`target_include_directories`, `target_link_libraries`, `target_compile_definitions`, `target_compile_features`) rather than mutating global directory-wide variables. Every attachment takes a visibility keyword:

- `PRIVATE` — needed to build this target itself, not needed by anything that links against it (an internal header directory, an implementation-only dependency).
- `INTERFACE` — not needed to build this target itself, but required by anything that links against it (a header-only dependency this target merely forwards, or requirements imposed only on consumers).
- `PUBLIC` — both: needed here *and* required by consumers. `PUBLIC` = `PRIVATE` + `INTERFACE`.

This distinction exists because linking is transitive: if `liba` privately links `libc`, and `libb` links `liba`, `libb` should not automatically gain `libc`'s include paths, compile definitions, or link requirement — `libc` is an implementation detail of `liba`. Getting this wrong is the single most common real-world CMake defect: marking something `PUBLIC` when it should be `PRIVATE` leaks internal dependencies into every downstream consumer's build, silently expanding their include paths, their link line, and their exposure to that dependency's version churn.

### Generator Expressions and Toolchains

Generator expressions (`$<CONFIG:Debug>`, `$<COMPILE_LANG_AND_ID:CXX,MSVC>`, `$<BUILD_INTERFACE:...>` vs `$<INSTALL_INTERFACE:...>`) let a target property vary by configuration, compiler, or build-vs-install context, evaluated at generate time rather than configure time — necessary because a single `CMakeLists.txt` must produce correct results across Debug/Release, MSVC/Clang/GCC, and build-tree/install-tree consumption, and an `if()` at configure time cannot see which configuration will eventually be built (for multi-config generators like Visual Studio's, all configurations are configured once and selected at build time). A toolchain file (or presets' `toolchainFile` field) pins compiler, sysroot, and target-platform settings so the same source tree cross-compiles or targets a specific SDK reproducibly, independent of what happens to be on a given machine's `PATH`.

### Dependency Management and Presets

`FetchContent` downloads and configures a dependency's source as part of the current build's configure step (used here for GoogleTest); `find_package` locates an already-installed dependency via its exported CMake config files. `CMakePresets.json` captures a named, versioned, shareable set of configure/build/test arguments (generator, build directory, cache variables, toolchain file) so `cmake --preset msvc-debug` reproduces exactly what a teammate or CI runner gets — replacing fragile, undocumented, ad hoc command lines with a file that lives in version control.

### Testing: GoogleTest and CTest

GoogleTest supplies the assertion/fixture/parameterized-test machinery; CTest is CMake's test *runner*, driving whatever test executables `gtest_discover_tests()` registers, aggregating pass/fail, timing, and (with `--output-on-failure`) surfacing failing assertions without manually re-running each binary. The two compose: CTest doesn't know what GoogleTest asserted, only whether the process it ran exited zero — `gtest_discover_tests()` bridges this by querying the compiled test binary for its individual test case names at build time and registering each as its own CTest test, so a single failing `TEST(Foo, Bar)` is reported by name rather than as "the whole binary failed."

### Installation and Packaging

`install(TARGETS ...)` and `install(EXPORT ...)` produce not just copied files but an exported CMake package config that downstream projects consume via `find_package(YourLib)` — this is what makes a library actually *installable* rather than only buildable-from-source-as-a-subdirectory. Packaging (CPack, or hand-rolled archives) bundles the installed tree for distribution; the `BUILD_INTERFACE`/`INSTALL_INTERFACE` generator-expression split exists specifically because include paths that are correct inside the build tree (`${CMAKE_SOURCE_DIR}/include`) are wrong after installation (the installed layout differs), and vice versa.

### CI, Testing Practice, Fuzzing, and Sanitizers

A CI pipeline exists to run the same build-and-test sequence a human would run locally, automatically, on every change, on infrastructure nobody can quietly skip a step on. Good testing practice separates unit tests (fast, isolated, run every commit) from integration/stress tests (slower, run less often or on a schedule). Fuzzing (e.g. libFuzzer) generates adversarial inputs to find crashes and UB that example-based tests miss by construction — a fuzzer explores input space a human author never thought to write a test for. Sanitizers instrument the binary itself to catch categories of bugs at runtime that compile cleanly and often "work by luck": AddressSanitizer (ASan) catches out-of-bounds/use-after-free, UndefinedBehaviorSanitizer (UBSan) catches signed overflow/misaligned access/etc., ThreadSanitizer (TSan) catches data races, MemorySanitizer (MSan) catches reads of uninitialized memory.

**Toolchain reality this chapter must confront directly:** MSVC (this workbook's primary toolchain) supports ASan but has **no TSan and no MSan**. Both are Clang/GCC-only. This is not a minor gap — Chapter 11's data-race-free acceptance criteria and this chapter's own uninitialized-read problems are ungradable on MSVC alone. The fix is not "wait for Microsoft" — it is a dedicated, exercised CMake preset, `wsl-clang-tsan` (and `wsl-clang-msan`), that runs the identical source tree under WSL's Clang. A build system that only targets one toolchain's sanitizer availability has a permanent, structural blind spot; a build system with presets for both toolchains does not.

## Common Misconceptions

1. **"`PUBLIC` is the safe default — it can't hurt to over-share."** It is the opposite of safe: `PUBLIC` is a *promise to every future consumer* about what they will also depend on. Marking something `PUBLIC` that should be `PRIVATE` means every consumer's include paths and link line silently grow, and upgrading that dependency's version becomes a breaking change for everyone downstream, not just for this target.

2. **"CTest and GoogleTest are the same thing / CTest runs my assertions."** CTest never sees an individual `EXPECT_EQ` — it only sees a process's exit code. `gtest_discover_tests()` is what turns "one exit code" into "one CTest entry per `TEST()`"; without it, an entire test binary is one pass/fail unit, and a single failing assertion buried among fifty passing ones is indistinguishable, from CTest's perspective, from all fifty failing.

3. **"Sanitizers are for finding memory bugs; if my code passes -Wall and compiles, it's probably fine."** A program with a data race, an out-of-bounds read that happens to land in already-allocated memory, or an uninitialized read that happens to be zero can compile warning-free, pass every unit test, and still be wrong — sanitizers exist precisely because these classes of bug are invisible to both the compiler's static checks and to tests that only check *observed* output, not *well-definedness*.

4. **"MSVC and Clang/GCC support the same sanitizers, just with different flags."** False specifically for TSan and MSan — MSVC has neither, full stop, not "has them under a different flag." Any acceptance criterion phrased as "TSan-clean" or "run under MSan" is a hard requirement for the WSL Clang/GCC toolchain and cannot be satisfied, even in principle, on MSVC alone.

5. **`FetchContent` and `find_package` are interchangeable ways to "get a dependency."** `FetchContent` builds the dependency from source as part of *this* configure/build, controlling its exact version and options; `find_package` locates something already built and installed elsewhere, whose version and build options this project has no control over. Silently swapping one for the other changes what "the same build" even means across two machines.

6. **"A CMake preset is just a saved command line — I could just write a shell script instead."** A preset is versioned, composable (presets can `inherit` from other presets), and understood natively by IDEs and CI systems that can enumerate and select presets without parsing a script; a hand-rolled shell script is opaque to that tooling and tends to drift from what's actually documented as "the way to build this."

## Quick Checks

**10-QC1.** `liba` privately links `libc` (an implementation-only dependency). `libb` links `liba`. Does `libb`'s build see `libc`'s include directories? Why or why not?

**10-QC2.** What is the difference between what CTest reports when a test binary segfaults versus when a `gtest_discover_tests()`-registered `TEST()` inside that binary fails an `EXPECT_EQ`?

**10-QC3.** Why can't a plain `if(CMAKE_BUILD_TYPE STREQUAL "Debug")` check, evaluated at configure time, correctly gate a compile option for a Visual Studio (multi-config) generator?

**10-QC4.** A header path is correct as `${CMAKE_SOURCE_DIR}/include` while building from the source tree, but wrong once the library is installed to `/usr/local/include`. What CMake mechanism exists specifically to give two different answers for these two contexts, and why can't a single hardcoded path serve both?

**10-QC5.** Name one class of bug ASan catches that UBSan does not, and one class UBSan catches that ASan does not.

**10-QC6.** Why is "TSan-clean" categorically ungradable on an MSVC-only build, regardless of warning level or `/analyze` settings?

**10-QC7.** What is the practical difference between an example-based unit test and a fuzz target for the same function, in terms of what kind of bug each is likely to find?

**10-QC8.** Why does `PUBLIC` on a `target_link_libraries` call count as *two* separate promises rather than one?

## Problems

### Level 1 — Recognition

**10-P01.** Given a `CMakeLists.txt` snippet with `target_link_libraries(mylib PUBLIC fmt::fmt)` versus one with `target_link_libraries(mylib PRIVATE fmt::fmt)`, state which one causes any executable linking `mylib` to also automatically gain `fmt`'s include directories and link requirement, and which one keeps `fmt` fully internal to `mylib`.

---

**10-P02.** Given a `CMakeLists.txt` that calls `enable_testing()` and `gtest_discover_tests(my_tests)`, but never calls `add_test()` directly anywhere, explain how CTest ends up with test entries at all.

---

**10-P03.** A `CMakePresets.json` defines `"generator": "Ninja"` for preset `linux-debug` and `"generator": "Visual Studio 17 2022"` for preset `msvc-debug`. State which preset supports selecting the build configuration (Debug/Release) at the `cmake --build` step rather than at the `cmake --preset` (configure) step, and why.

---

**10-P04.** Given the sanitizer availability matrix (ASan: MSVC+Clang/GCC; UBSan: Clang/GCC only; TSan: Clang/GCC only; MSan: Clang-only), identify which two sanitizers are simply unavailable on MSVC no matter what flags are passed.

---

**10-P05.** Given a target property attached via `target_include_directories(mylib PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)`, state which of the two paths is used by a consumer building against `mylib` as an in-tree subdirectory, and which is used by a consumer that ran `find_package(mylib)` against an installed copy.

### Level 2 — Prediction

**10-P06.** `liba` is built with `target_compile_definitions(liba PRIVATE USE_FAST_PATH)`. `libb` links `liba` and includes one of `liba`'s public headers, which has `#ifdef USE_FAST_PATH` branches affecting an inline function's ABI-visible layout. Predict whether `libb`'s translation units see `USE_FAST_PATH` defined, and explain the consequence if `liba`'s public header actually needs that macro's value to be consistent with however `liba` itself was compiled.

---

**10-P07.** A project's top-level `CMakeLists.txt` sets `set(CMAKE_CXX_STANDARD 20)` as a plain variable, with no `target_compile_features` call anywhere. A second, independently-built library is later added as a subdirectory and itself calls `target_compile_features(otherlib PUBLIC cxx_std_17)`. Predict which C++ standard `otherlib` actually compiles under, and explain why a directory-scoped variable and a target-scoped feature requirement don't automatically reconcile.

---

**10-P08.** A `gtest_discover_tests(my_tests)` call is present, but the test executable `my_tests` was built without linking `GTest::gtest_main` (only `GTest::gtest`). Predict what happens when `ctest` is run, and explain in terms of what `gtest_main` actually supplies that a bare `gtest` link does not.

---

**10-P09.** A CI pipeline runs `ctest --preset wsl-clang-tsan` on a codebase that has never been built under that preset before, and the preset was defined but never previously exercised. Predict the most likely first-run outcome and why "the preset exists in the file" is not the same guarantee as "the preset has ever actually succeeded."

---

**10-P10.** Two developers each run `cmake --preset msvc-debug` on their own machines, but one has an older Ninja on `PATH` that the preset's `"generator"` field doesn't actually pin (it was left as `"generator": "Ninja"` with no explicit version constraint), while the preset does pin an exact `"toolchainFile"`. Predict which of the two configurations (generator or toolchain) is more likely to silently produce a different build than intended, and why a toolchain file pins compiler/sysroot but a bare generator name doesn't pin a version.

---

**10-P11.** A fuzz target is written for a function that parses a length-prefixed binary record, and the fuzzer is run for 10 minutes before the developer concludes "no crash found, function is correct." Predict what conclusion is actually justified by 10 minutes of fuzzing with no crash, versus what conclusion ("the function is correct") is not justified, and why the two are different claims.

### Level 3 — Implementation

**10-P12.**
Write a two-target `CMakeLists.txt` for a library `mathutils` (a `.cpp`/`.h` pair implementing a `gcd` function) and an executable `mathutils_demo` that calls it, such that: `mathutils`'s include directory is exposed to consumers via `target_include_directories(... PUBLIC ...)`, the library is built as a static library, and `mathutils_demo` links it with correctly scoped visibility (no unnecessary `PUBLIC` on the executable's own link line, since nothing links against an executable). Build and run it to confirm `mathutils_demo` compiles and executes.

---

**10-P13.**
Extend a `CMakeLists.txt` to fetch GoogleTest via `FetchContent`, declare a test executable `mathutils_tests` linking `mathutils` and `GTest::gtest_main`, call `gtest_discover_tests(mathutils_tests)`, and add at least two `TEST()` cases for the `gcd` function from 10-P12 (one typical case, one edge case such as `gcd(0, n)`). Run `ctest --output-on-failure` and confirm both tests are discovered and reported by name, not as one aggregate result.

---

**10-P14.**
Write a `CMakePresets.json` defining two presets, `msvc-debug` (Visual Studio generator, Debug configuration, `/W4 /permissive- /Zc:preprocessor`) and `wsl-clang-debug` (Ninja generator, a Clang toolchain file, `-Wall -Wextra`), such that `cmake --preset msvc-debug` and `cmake --preset wsl-clang-debug` each configure successfully from the same source tree. Run both and confirm each produces a working build directory.

---

**10-P15.**
Add a `wsl-clang-asan` preset (inheriting from `wsl-clang-debug`) that adds `-fsanitize=address -fno-omit-frame-pointer` to both compile and link flags. Deliberately introduce a one-line out-of-bounds heap write in a test file, build under this preset, run the resulting binary, and confirm ASan reports the exact bug with a stack trace — then remove the bug and confirm a clean run.

---

**10-P16.**
Add a `wsl-clang-tsan` preset (inheriting from `wsl-clang-debug`) that adds `-fsanitize=thread`. Write a small two-thread program with a deliberate unsynchronized read/write race on a shared `int` (no mutex, no atomic), build it under this preset, and confirm TSan reports the race with both threads' stack traces. Then fix the race with a `std::mutex` and confirm a clean run under the same preset.

---

**10-P17.**
Given a library target `netcore` that privately depends on OpenSSL (`target_link_libraries(netcore PRIVATE OpenSSL::SSL)`) purely as an internal implementation detail (no OpenSSL types appear in any public header), write the `install(TARGETS netcore EXPORT netcoreTargets ...)` and `install(EXPORT netcoreTargets ...)` calls needed to make `netcore` installable and consumable via `find_package(netcore)` from a separate downstream project, such that the downstream project's build does **not** need to know OpenSSL exists. Verify by configuring a small downstream `CMakeLists.txt` against the installed package and confirming it builds without an `OpenSSL::SSL` link line of its own.

---

**10-P18.**
Write a minimal GitHub Actions (or equivalent CI YAML) workflow that, on every push, runs `cmake --preset msvc-debug`, builds, and runs `ctest --preset msvc-debug --output-on-failure` on a Windows runner, and separately runs the `wsl-clang-tsan` preset's configure/build/test sequence on a Linux runner. Confirm (by inspection of the YAML, or by running an equivalent sequence locally) that a failing test in either job would fail the pipeline, not just log a warning.

---

**10-P19.**
Write a single libFuzzer target (`LLVMFuzzerTestOneInput`) for the length-prefixed binary-record parser referenced in 10-P11, wire it into a dedicated `wsl-clang-fuzz` CMake preset using `-fsanitize=fuzzer,address`, and run it for at least 60 seconds against a small seed corpus. If it finds a crash, minimize the crashing input (`-minimize_crash=1`) and explain what bug the minimized input reveals; if it finds nothing in that window, state exactly what that non-result does and does not establish about correctness.

---

**10-P20.**
Given a library `serializer` whose public header requires C++20 (`concepts`, `<span>`), write the `target_compile_features(serializer PUBLIC cxx_std_20)` call and demonstrate — by attempting to consume it from a downstream target compiled under `-std=c++17` — that CMake either raises the downstream target's effective standard to C++20 or fails the configure step outright, rather than silently producing a downstream translation unit that fails to compile with a confusing raw compiler error only.

**10-P21.**
Write a small header-only library target `geoutils` using `add_library(geoutils INTERFACE)` and `target_include_directories(geoutils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)`, containing a templated `distance(Point, Point)` function. Consume it from two separate executable targets, each including the header and calling `distance`, and confirm both build correctly despite `geoutils` never producing a compiled `.lib`/`.a` of its own — demonstrating that `INTERFACE` targets carry only propagated properties, not compiled code.

### Level 4 — Debugging

**10-P22.** [DEBUG] A library `parsers` is declared with `target_link_libraries(parsers PUBLIC nlohmann_json::nlohmann_json)`, purely because one internal `.cpp` file happens to use it for a debug-only diagnostic dump — no public header of `parsers` mentions JSON at all. Six months later, a downstream consumer's build breaks when they upgrade their own unrelated JSON library version, and the error is confusingly inside `parsers`'s transitively-exposed JSON dependency. Diagnose why a purely-internal, `.cpp`-only dependency ended up affecting a downstream consumer's build, and state the one-line fix.

---

**10-P23.** [DEBUG] A test suite calls `gtest_discover_tests(my_tests)`, and `ctest` reports **zero tests found**, even though the `my_tests` binary, when run directly, executes and prints several `[ PASSED ]` lines. Diagnose the likely cause (hint: consider what `gtest_discover_tests()` needs to do at build time versus what simply running the binary does) and state the fix.

---

**10-P24.** [DEBUG] A `CMakeLists.txt` has `if(CMAKE_BUILD_TYPE STREQUAL "Debug") target_compile_definitions(mylib PRIVATE DEBUG_LOGGING) endif()`, and under the `msvc-debug` preset (Visual Studio generator), `DEBUG_LOGGING` is *not* defined even when building the Debug configuration in Visual Studio. Diagnose why this configure-time check fails specifically under a multi-config generator, and state the generator-expression-based fix that actually works regardless of generator.

---

**10-P25.** [DEBUG] A project's `wsl-clang-tsan` preset is defined in `CMakePresets.json` but has never actually been run in CI — only `msvc-debug` and `wsl-clang-debug` run automatically. A concurrency bug (a genuine data race) ships to production and is only caught weeks later when a developer happens to run the TSan preset manually. Diagnose the process failure (not a code bug) that allowed this, and state what needs to change about the CI configuration, not the code, to prevent recurrence.

---

**10-P26.** [DEBUG] A fuzz target for a binary-record parser is run for a full week with zero crashes found, and the team concludes the parser is "fuzz-verified safe" and removes an existing bounds check as "redundant, the fuzzer would have found it." Diagnose the reasoning error, referencing what a fuzzer's absence-of-crashes result does and does not prove, and predict the likely consequence of removing the bounds check on this basis alone.

---

**10-P27.** [DEBUG] A downstream project calls `find_package(netcore REQUIRED)` against an installed copy of the `netcore` library from 10-P17, and the configure step fails with an error that `OpenSSL` cannot be found — even though `netcore`'s own public headers never mention OpenSSL. Diagnose which of `netcore`'s own CMake install/export calls is responsible for this leak, and state the fix in terms of visibility keyword, not in terms of "install OpenSSL on the downstream machine."

### Level 5 — Integration

**10-P28.**
Take a two-target library (`geolib`, built both as `geolib_static` and `geolib_shared` from the same sources) where a `PUBLIC` dependency on an internal-only header-generation tool has leaked into consumers' include paths (consumers can `#include <geolib/internal/codegen_helpers.h>`, which was never meant to be part of the public API). Restructure the target graph — introducing a `PRIVATE` or `INTERFACE` split, or a separate internal target, as appropriate — so consumers linking either `geolib_static` or `geolib_shared` see only the intended public headers, while both library variants still build correctly from the same source set. Verify with a downstream consumer target that a `#include` of the internal header now fails to resolve.

---

**10-P29.**
Build a complete CMake project for a small library with: a `msvc-debug`/`msvc-release` preset pair, a `wsl-clang-debug`/`wsl-gcc-debug` preset pair, a `wsl-clang-asan` preset, and a `wsl-clang-tsan` preset — all via preset inheritance from one or two base presets rather than five independently-written blocks. Add a GoogleTest suite exercising a small thread-safe counter class, run it clean under every preset, then deliberately introduce a race in the counter and confirm only the TSan preset's run reports it (the others build and "pass" despite the bug).

---

**10-P30.**
Wire a fuzz target for a URL-parsing function into the project from 10-P28 via a `wsl-clang-fuzz` preset, run it against a seed corpus until it finds a genuine crash (a real, previously-unknown parsing bug, not a planted one), minimize the crash, add a regression `TEST()` reproducing the minimized input to the GoogleTest suite so it's covered going forward, and confirm the regression test fails on the pre-fix code and passes after a fix.

---

**10-P31.**
Package the library from 10-P28 for distribution: add `install(TARGETS ...)`/`install(EXPORT ...)` calls producing a `find_package`-consumable export, add a CPack configuration producing at least one distributable archive, and write a from-scratch downstream `CMakeLists.txt` that consumes the installed package via `find_package` and links against it — confirming the downstream project needs zero knowledge of the library's own internal dependencies (per the `PUBLIC`/`PRIVATE` boundary enforced in 10-P27).

### Level 6 — Production

**10-P32.** A mid-sized C++ codebase (40+ internal libraries) has grown its `PUBLIC` link-library lists organically over three years, with no enforcement discipline — some genuinely need `PUBLIC`, many should be `PRIVATE` but nobody ever revisited them. Propose a concrete, incrementally-adoptable process (not a one-time big-bang rewrite) for auditing and correcting visibility keywords across the target graph without breaking any currently-working consumer mid-migration, including how you'd verify each individual change is safe before merging it.

---

**10-P33.** Your organization's CI currently only runs `msvc-debug` and `msvc-release` — no WSL job exists at all, so ASan, UBSan, TSan, and MSan have never run against this codebase. Design the rollout plan for introducing the WSL sanitizer presets into CI, addressing: which sanitizer to introduce first and why, how to handle the likely flood of pre-existing violations a first TSan/ASan run will surface (mass-disable vs. mass-fix vs. staged enforcement), and how to prevent the new CI job from being treated as "optional/ignorable" the way an unenforced preset was in 10-P25's incident.

---

**10-P34.** A production incident postmortem reveals that a library's fuzz target existed, ran in CI nightly, and had been silently failing to actually execute (a build misconfiguration meant the fuzzer binary was never rebuilt after a refactor, so CI was fuzzing a six-month-stale binary) for months before a real bug reached production that the *current* code's fuzz target would likely have caught immediately. Design a concrete safeguard — a specific CI check or build-system assertion — that would have caught "the fuzz target silently stopped being rebuilt" as its own failure, distinct from "the fuzz target found no bugs."

### Level 7 — Principal Reasoning

**10-P35.** Your organization ships a C++ library to external customers who build it themselves, on their own choice of MSVC or Clang/GCC versions, across a multi-year support window. Design the build-system-level policy (CMake minimum version pinning, presets exposed to customers vs. internal-only, which sanitizer/testing infrastructure is customer-facing vs. internal-only, how C++ standard requirements are communicated and enforced via `target_compile_features` rather than prose documentation alone) that maximizes the chance a customer's own build succeeds on their first attempt, while still letting your team evolve the internal build system freely. Justify each policy choice against a concrete failure mode it prevents, and identify at least one tradeoff you are deliberately accepting (a capability you give up) in exchange for that reliability.

## Integration Challenge — 10-IC1

You are given a two-target library, `iolib` (built as both `iolib_static` and `iolib_shared` from the same source set), where a `PUBLIC` dependency on a compression library (`zstd`) has leaked: `iolib`'s public headers never mention `zstd` at all — it is used only inside a handful of `.cpp` files as an internal implementation detail — yet `target_link_libraries(iolib_static PUBLIC zstd::libzstd)` and the equivalent on `iolib_shared` currently expose it to every consumer's link line and (via a transitively-included header brought in by an overly broad `target_include_directories(... PUBLIC ...)`) their include path as well.

1. **Diagnose the leak completely.** Identify both mechanisms causing the leak (the `PUBLIC` link visibility and the `PUBLIC` include-directory visibility) and explain, for each, concretely what a downstream consumer currently sees that they should not.
2. **Fix the target graph.** Restructure `iolib_static` and `iolib_shared`'s CMake declarations so `zstd` becomes a `PRIVATE` dependency of both, while both variants still build correctly from the shared source set (no duplicated source-list maintenance between the two targets).
3. **Prove it with a downstream consumer.** Write a small downstream target that links `iolib_shared` and confirm, by inspecting its actual compile/link command lines (not just "it built"), that no `zstd` include path or link library appears anywhere in the downstream target's own build — while `iolib_shared` itself still correctly links and uses `zstd` internally.
4. **Add a regression guard.** Propose a concrete, automatable check (a CI step, a CMake assertion, or a lint rule) that would have caught this specific leak at the moment it was introduced, rather than relying on a downstream consumer eventually noticing an unexpected transitive dependency.

## Chapter Project

This chapter feeds directly into:
- **P-3.3 Reusable Library Template & Test Harness** — its deliverable *is* the target-graph, preset, and CTest wiring discipline this chapter teaches (10-P12–10-P14, 10-P28–10-P31), producing the reusable harness every later project in this workbook is submitted against.
