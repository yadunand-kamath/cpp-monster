# Chapter 10 — Solutions

## Quick Check Answers

**10-QC1.** No — `libc` is `liba`'s `PRIVATE` dependency, meaning it is needed to build `liba` itself but is explicitly not part of what `liba` requires its consumers to also have. `PRIVATE` properties are not transitively propagated to anything linking `liba`, so `libb`'s build never sees `libc`'s include directories at all.

**10-QC2.** A segfaulting test binary is reported by CTest as a single failed test — the one CTest entry that `add_test`/`gtest_discover_tests` registered for that binary crashed, and CTest has no visibility into which of the (possibly many) `TEST()` cases inside it was running at the time. A `gtest_discover_tests()`-registered `TEST()` that fails an `EXPECT_EQ` (without crashing the process) is reported as its own, individually-named failing CTest entry, because discovery already split the one binary into one CTest test per `TEST()` case at build time.

**10-QC3.** For a multi-config generator (like Visual Studio's), `CMAKE_BUILD_TYPE` is not meaningfully set at configure time at all — all configurations (Debug, Release, etc.) are configured once, and the actual configuration is chosen later, at the `cmake --build --config <Config>` step. An `if()` check against `CMAKE_BUILD_TYPE` evaluates once, at configure time, against whatever (usually empty or irrelevant) value that variable happens to hold — it cannot "know" which configuration will eventually be built, so the branch it guards is baked in incorrectly (or not at all) regardless of which configuration is later selected.

**10-QC4.** The `$<BUILD_INTERFACE:...>` / `$<INSTALL_INTERFACE:...>` generator-expression pair exists specifically to give two different answers for these two contexts: `$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>` is substituted only when a consumer builds against this target from within the same build tree (a subdirectory or same-project consumer), while `$<INSTALL_INTERFACE:include>` is substituted only when a consumer finds this target via an installed package (`find_package`). A single hardcoded path can't serve both because the two contexts have genuinely different, non-overlapping correct answers — the source tree's absolute path is meaningless (and often nonexistent) on a machine that only received the installed tree, and the installed relative path is meaningless while still building in-tree.

**10-QC5.** ASan catches out-of-bounds/use-after-free access — a spatial or temporal memory-safety violation on the address itself — which UBSan does not check for (UBSan is not tracking memory regions at all). UBSan catches signed integer overflow, misaligned access, and other language-level undefined-behavior violations that don't necessarily touch any invalid address at all — a signed overflow can happen entirely within a validly-allocated `int`, which ASan has no reason to flag since no memory-safety rule was violated.

**10-QC6.** TSan is a Clang/GCC-only sanitizer with no MSVC implementation whatsoever — it is not that MSVC's version is weaker or requires different flags, it simply does not exist under MSVC at any warning level, `/analyze` setting, or flag combination. "TSan-clean" is therefore not a claim that can even be evaluated on an MSVC-only build; the claim requires actually running the code under a Clang/GCC build with `-fsanitize=thread`, which means a WSL (or equivalent) toolchain is mandatory infrastructure, not an optional nice-to-have.

**10-QC7.** An example-based unit test finds exactly the bugs its author anticipated and wrote an assertion for — it is bounded by the author's own imagination about which inputs are worth checking. A fuzz target, by generating adversarial/random/mutated inputs at scale, tends to find bugs triggered by inputs the author never thought to write a test for at all (unusual byte sequences, boundary lengths, malformed structure) — the two are complementary: unit tests verify known expected behavior, fuzzing searches for unknown, unanticipated failure inputs.

**10-QC8.** `PUBLIC` = `PRIVATE` + `INTERFACE` — it simultaneously declares "this target itself needs this dependency to build" (the `PRIVATE` half) and "anything linking this target also requires this dependency" (the `INTERFACE` half). These are logically separate claims — a target can need something only for itself (`PRIVATE`), require something only of its consumers without needing it itself (`INTERFACE`, e.g. a header-only forwarding requirement), or both (`PUBLIC`) — so `PUBLIC` is never a single promise, it is exactly these two bundled together.

## Problem Solutions

### Level 1 — Recognition

**10-P01.** `target_link_libraries(mylib PUBLIC fmt::fmt)` is the one that causes any executable linking `mylib` to automatically gain `fmt`'s include directories and link requirement — `PUBLIC` propagates both the build requirement and the consumer-facing requirement. `target_link_libraries(mylib PRIVATE fmt::fmt)` keeps `fmt` entirely internal to `mylib`: consumers linking `mylib` never see `fmt`'s include paths or gain a transitive link requirement on it.

---

**10-P02.** `gtest_discover_tests()` inspects the compiled `my_tests` binary at build time (by running it with a special discovery flag) to enumerate every individual `TEST()`/`TEST_F()` case it contains, and registers each one as its own `add_test()` call automatically, behind the scenes — the developer never needs to call `add_test()` by hand for each case because this discovery step does it for them, generating the equivalent CTest registration from the binary's own self-reported test list.

---

**10-P03.** The `msvc-debug` preset, using the Visual Studio generator, supports selecting Debug/Release at the `cmake --build` step — Visual Studio is a multi-config generator, so all configurations are configured once and only chosen later, at build time (`cmake --build --config Debug`). The `linux-debug` preset, using Ninja (a single-config generator), must have its configuration (Debug vs. Release) fixed at the `cmake --preset` (configure) step instead, since Ninja only ever builds the one configuration it was configured for.

---

**10-P04.** TSan and MSan are simply unavailable on MSVC no matter what flags are passed — both require Clang or GCC. ASan is available on both toolchains; UBSan is Clang/GCC-only (also unavailable on MSVC), but the problem specifically asks for the two sanitizers unavailable "no matter what flags" — TSan and MSan, since MSan additionally isn't even available on GCC (Clang-only), making it the most restricted of the four.

---

**10-P05.** The `$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>` path is used by a consumer building against `mylib` as an in-tree subdirectory (still inside the same build tree, where the source-tree path is valid). The `$<INSTALL_INTERFACE:include>` path is used by a consumer that ran `find_package(mylib)` against an installed copy (where only the relative, installed-layout path is meaningful).

### Level 2 — Prediction

**10-P06.** `libb`'s translation units do **not** see `USE_FAST_PATH` defined — it was attached with `PRIVATE` visibility, which means it applies only while compiling `liba` itself and is never propagated to anything linking `liba`. The consequence, if `liba`'s public header genuinely needs that macro's value to be consistent with how `liba` was compiled (e.g. it changes an inline function's layout or an exposed struct's size), is a real ODR/ABI mismatch: `liba`'s own object code was compiled with one branch of the `#ifdef`, but `libb`'s translation units compiling the same header see the other branch — two different definitions of what should be the same entity, undefined behavior at link/runtime, exactly the kind of bug this chapter's `PRIVATE`-macro-affecting-a-public-header misconceptions section warns about. The macro should have been `PUBLIC` (or, better, expressed via a generated config header) precisely because it affects consumer-visible ABI.

---

**10-P07.** `otherlib` compiles under C++17, not C++20 — `target_compile_features(otherlib PUBLIC cxx_std_17)` is a target-scoped, explicit requirement attached directly to `otherlib`, while `set(CMAKE_CXX_STANDARD 20)` is a directory-scoped variable that only supplies a *default* for targets that don't otherwise specify their own standard requirement. A target-scoped `target_compile_features` call always wins over an inherited directory default because CMake treats the explicit target property as the actual requirement, not merely a suggestion the variable could override — the two mechanisms operate at different levels of authority and don't "reconcile," the more specific one simply takes precedence.

---

**10-P08.** `ctest` reports that the `my_tests` binary's discovered tests fail to run at all, typically with each discovered test erroring out at the CTest level (a runtime failure trying to execute the test binary's discovery/list step, or every individual test failing immediately when actually run) — because `gtest_main` supplies the `main()` function that parses command-line flags and invokes `RUN_ALL_TESTS()`; linking only bare `GTest::gtest` gives you the assertion/fixture machinery but no entry point at all, so the binary has no `main()` to run unless the developer supplied one themselves.

---

**10-P09.** The most likely first-run outcome is that the TSan preset's build fails to configure/build cleanly, or, if it does build, its first-ever run surfaces existing violations that were never caught before (since nothing exercised this compiler/sanitizer combination previously) — a preset merely being present in `CMakePresets.json` says nothing about whether that exact configure/build/toolchain combination has ever actually succeeded; it is untested infrastructure until it has actually been run at least once and observed to work.

---

**10-P10.** The generator (Ninja, unpinned to a specific version) is more likely to silently produce a different build than intended, because a `toolchainFile` pins the actual compiler binary, its version, and sysroot/target settings explicitly and deterministically, while a bare `"generator": "Ninja"` string only says "use whatever Ninja is discovered on `PATH`" — two developers with different Ninja versions on `PATH` get the same toolchain (compiler) but potentially different generator behavior (e.g. differing support for newer Ninja features, different default parallelism handling, or subtly different dependency-file parsing), a difference the preset never pinned down at all.

---

**10-P11.** The only conclusion actually justified by 10 minutes of fuzzing with no crash is "the fuzzer did not find a crash-triggering input within the input space it happened to explore in 10 minutes" — a narrow, time-and-corpus-bounded negative result. "The function is correct" is not justified, because absence of a crash within a bounded exploration window says nothing about the vastly larger space of inputs the fuzzer didn't reach in that time, nor does it rule out bugs that don't manifest as a crash (e.g. a silently wrong but non-crashing result) — these are different claims: one is a bounded empirical observation, the other is an unbounded correctness claim that no finite fuzzing run can establish on its own.

### Level 3 — Implementation

**10-P12.**
```cmake
cmake_minimum_required(VERSION 3.24)
project(mathutils_demo_project CXX)

add_library(mathutils STATIC src/mathutils.cpp)
target_include_directories(mathutils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mathutils PUBLIC cxx_std_20)

add_executable(mathutils_demo src/main.cpp)
target_link_libraries(mathutils_demo PRIVATE mathutils)
```
```cpp
// include/mathutils.h
int gcd(int a, int b);
```
```cpp
// src/mathutils.cpp
#include "mathutils.h"
int gcd(int a, int b) { while (b) { int t = b; b = a % b; a = t; } return a; }
```
```cpp
// src/main.cpp
#include "mathutils.h"
#include <cstdio>
int main() { printf("%d\n", gcd(48, 18)); }
```
Building and running (`cmake -B build && cmake --build build && ./build/mathutils_demo`) prints `6`, confirming the library builds, links, and executes correctly. `mathutils_demo`'s own link line uses `PRIVATE` since nothing ever links against an executable — `PUBLIC`/`INTERFACE` on an executable's `target_link_libraries` would be meaningless.

---

**10-P13.**
```cmake
include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(mathutils_tests tests/test_mathutils.cpp)
target_link_libraries(mathutils_tests PRIVATE mathutils GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(mathutils_tests)
```
```cpp
// tests/test_mathutils.cpp
#include <gtest/gtest.h>
#include "mathutils.h"
TEST(GcdTest, TypicalCase) { EXPECT_EQ(gcd(48, 18), 6); }
TEST(GcdTest, ZeroFirstArgument) { EXPECT_EQ(gcd(0, 18), 18); }
```
Running `ctest --output-on-failure` after building reports `GcdTest.TypicalCase` and `GcdTest.ZeroFirstArgument` as two separately-named passing tests — confirming discovery split the one binary into per-`TEST()` CTest entries rather than one aggregate pass/fail.

---

**10-P14.**
```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "msvc-debug",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build/msvc-debug",
      "cacheVariables": {
        "CMAKE_CXX_FLAGS": "/W4 /permissive- /Zc:preprocessor"
      }
    },
    {
      "name": "wsl-clang-debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/wsl-clang-debug",
      "toolchainFile": "${sourceDir}/cmake/clang-toolchain.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_FLAGS": "-Wall -Wextra"
      }
    }
  ]
}
```
Running `cmake --preset msvc-debug` and (from WSL) `cmake --preset wsl-clang-debug` each configure a separate build directory successfully from the same source tree, confirmed by each producing its own working `build/<preset-name>/` directory with no cross-contamination of flags or generator.

---

**10-P15.**
```json
{
  "name": "wsl-clang-asan",
  "inherits": "wsl-clang-debug",
  "binaryDir": "${sourceDir}/build/wsl-clang-asan",
  "cacheVariables": {
    "CMAKE_CXX_FLAGS": "-Wall -Wextra -fsanitize=address -fno-omit-frame-pointer",
    "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address"
  }
}
```
With a deliberately introduced one-line bug (e.g. `buf[10] = 'x';` on a 10-element `char buf[10]`), building and running under this preset produces an ASan report identifying a heap-buffer-overflow with the exact write's stack trace pointing at the offending line. Removing the bug and rebuilding under the same preset produces a clean run with no ASan output — confirming the sanitizer is actually active and specific to the introduced defect, not a false positive from the build configuration itself.

---

**10-P16.**
```json
{
  "name": "wsl-clang-tsan",
  "inherits": "wsl-clang-debug",
  "binaryDir": "${sourceDir}/build/wsl-clang-tsan",
  "cacheVariables": {
    "CMAKE_CXX_FLAGS": "-Wall -Wextra -fsanitize=thread",
    "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=thread"
  }
}
```
```cpp
int shared = 0;
void writer() { shared = 42; }
void reader() { if (shared == 42) { /* ... */ } }
// std::thread t1(writer), t2(reader); t1.join(); t2.join();
```
Building and running this program under `wsl-clang-tsan` produces a TSan data-race report naming both the write in `writer()` and the read in `reader()`, each with its own thread ID and stack trace. Wrapping both accesses in a shared `std::mutex` (lock before each access, unlock after) and rebuilding under the same preset produces a clean run with no race reported.

---

**10-P17.**
```cmake
add_library(netcore src/netcore.cpp)
target_include_directories(netcore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(netcore PRIVATE OpenSSL::SSL)

include(GNUInstallDirs)
install(TARGETS netcore EXPORT netcoreTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT netcoreTargets FILE netcoreTargets.cmake
    NAMESPACE netcore:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/netcore)
install(FILES netcoreConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/netcore)
```
Because `OpenSSL::SSL` was linked `PRIVATE`, it is never written into `netcoreTargets.cmake`'s exported interface requirements. A downstream `CMakeLists.txt` calling `find_package(netcore REQUIRED)` and `target_link_libraries(app PRIVATE netcore::netcore)` configures and builds successfully with no `OpenSSL::SSL` appearing anywhere in `app`'s own link line — confirmed by inspecting the generated build files (e.g. `ninja -t commands` or the MSBuild link command) for the absence of any OpenSSL reference.

---

**10-P18.**
```yaml
name: CI
on: [push]
jobs:
  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake --preset msvc-debug
      - run: cmake --build --preset msvc-debug
      - run: ctest --preset msvc-debug --output-on-failure
  linux-tsan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake --preset wsl-clang-tsan
      - run: cmake --build --preset wsl-clang-tsan
      - run: ctest --preset wsl-clang-tsan --output-on-failure
```
Each `run:` step is a separate shell invocation whose non-zero exit code (which `ctest` produces on any test failure) fails that step, which fails the job, which fails the whole workflow run — confirmed by deliberately breaking a test locally and running the equivalent sequence, observing `ctest`'s non-zero exit and that GitHub Actions marks a job red on a non-zero step exit code, not merely logging output.

---

**10-P19.**
```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    parse_record(data, size);
    return 0;
}
```
```json
{
  "name": "wsl-clang-fuzz",
  "inherits": "wsl-clang-debug",
  "cacheVariables": { "CMAKE_CXX_FLAGS": "-fsanitize=fuzzer,address" }
}
```
Running the built fuzzer binary against a small seed corpus for 60+ seconds (`./record_fuzzer corpus/`) either finds a crash — in which case `-minimize_crash=1 crash-<hash>` reduces it to a minimal reproducing input, typically revealing an off-by-one in the length-prefix bounds check (e.g. a record claiming a length one byte longer than the buffer actually holds) — or finds nothing in that window, in which case the only justified statement is "no crash was found in this specific corpus and time budget," not "the parser is correct" (per 10-P11's distinction).

---

**10-P20.**
```cmake
add_library(serializer INTERFACE)
target_include_directories(serializer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(serializer INTERFACE cxx_std_20)
```
A downstream target explicitly compiled under `-std=c++17` (e.g. via its own `target_compile_features(app PRIVATE cxx_std_17)`) that links `serializer` triggers CMake to raise `app`'s effective compiled standard to C++20 to satisfy `serializer`'s requirement — or, if the downstream target's standard was pinned in a way CMake cannot reconcile (an explicit, non-negotiable flag conflict), the configure step fails outright with a clear diagnostic naming the conflicting requirement, rather than silently compiling `app` under C++17 and producing a confusing raw compiler error deep inside `serializer`'s header about `concepts`/`<span>` not being recognized.

---

**10-P21.**
```cmake
add_library(geoutils INTERFACE)
target_include_directories(geoutils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(geoutils INTERFACE cxx_std_20)

add_executable(app_a src/app_a.cpp)
target_link_libraries(app_a PRIVATE geoutils)
add_executable(app_b src/app_b.cpp)
target_link_libraries(app_b PRIVATE geoutils)
```
```cpp
// include/geoutils.h
template <typename Point>
auto distance(Point a, Point b) { /* ... */ }
```
Both `app_a` and `app_b` build correctly despite `geoutils` never producing a `.lib`/`.a`/`.so` of its own — `add_library(... INTERFACE)` creates a target that exists purely to propagate its attached properties (include directories, compile features) to whatever links it, with zero compiled object code of its own; `geoutils` never appears as a build artifact on disk, only as a set of properties applied to `app_a` and `app_b`'s own compile commands.

### Level 4 — Debugging

**10-P22.** [DEBUG] `nlohmann_json::nlohmann_json` was attached with `PUBLIC` visibility despite being used only inside one internal `.cpp` file — `PUBLIC` propagates the link requirement (and, since `nlohmann_json` is header-only, effectively its include path/version) to every consumer of `parsers`, so any consumer's own build ends up implicitly depending on whatever JSON library version `parsers` was built against, even though no `parsers` public header ever exposes JSON types. The one-line fix: change `target_link_libraries(parsers PUBLIC nlohmann_json::nlohmann_json)` to `target_link_libraries(parsers PRIVATE nlohmann_json::nlohmann_json)`.

---

**10-P23.** [DEBUG] `ctest` reporting zero tests found while the binary itself runs and passes when invoked directly indicates the discovery step itself never ran or failed silently — the most likely cause is that `gtest_discover_tests(my_tests)` was called, but the project never called `enable_testing()` (required for CTest test registration to take effect at all), or the discovery step's build-time invocation of the test binary failed (e.g. because the binary requires a runtime dependency, like a shared library, not present in the discovery environment's `PATH`/`LD_LIBRARY_PATH`, even though it's present when the developer runs it manually from their own shell). The fix is to confirm `enable_testing()` is called before `gtest_discover_tests()`, and that the discovery-time environment (working directory, library search path) matches what's needed to actually execute the binary, not just build it.

---

**10-P24.** [DEBUG] This configure-time `if(CMAKE_BUILD_TYPE STREQUAL "Debug")` check fails specifically under Visual Studio's multi-config generator because `CMAKE_BUILD_TYPE` is essentially meaningless for multi-config generators — all configurations are configured once, with the actual Debug/Release choice deferred to the later `cmake --build --config <Config>` step, so at configure time there is no single "the build type" for the check to compare against (it's typically empty). The fix is to replace the configure-time `if()` with a generator-expression-based compile definition that's evaluated per-configuration at generate/build time: `target_compile_definitions(mylib PRIVATE $<$<CONFIG:Debug>:DEBUG_LOGGING>)`, which correctly activates only when the Debug configuration is actually the one being built, regardless of generator.

---

**10-P25.** [DEBUG] The process failure is that the `wsl-clang-tsan` preset existed in `CMakePresets.json` but was never wired into the CI pipeline's actual job list — a preset's mere existence in a file guarantees nothing about whether it runs automatically; only a CI job that explicitly invokes it does. The fix is not a code change but a CI configuration change: add a dedicated CI job that runs the `wsl-clang-tsan` preset's configure/build/test sequence on every push (or at minimum every merge to the main branch), exactly as automatically and non-optionally as the `msvc-debug`/`wsl-clang-debug` jobs already run, so a data race is caught by CI before merge rather than discovered by a developer manually running the preset weeks later.

---

**10-P26.** [DEBUG] The reasoning error is treating a week of fuzzing's absence of crashes as proof of correctness for the entire input space, when it only establishes "no crash was found within the specific inputs explored in that time budget and seed corpus" — a fuzzer's coverage is necessarily bounded by its actual exploration, not by the full space of possible malformed inputs, and a bounds check that happens to never be exercised by the fuzzer's mutation strategy (e.g. because the seed corpus never produced inputs near that exact boundary) provides zero evidence that boundary is safe to remove. The likely consequence: removing the bounds check reintroduces exactly the out-of-bounds read/write it was guarding against, now with no defense at all, waiting for a real-world input (not necessarily an adversarial one) to hit the case the fuzzer's corpus happened never to explore.

---

**10-P27.** [DEBUG] The leak is caused by `netcore`'s own `target_link_libraries(netcore PRIVATE OpenSSL::SSL)` being correct — the actual bug must instead be in how `netcore`'s CMake package config (`netcoreConfig.cmake`, or the generated `netcoreTargets.cmake`) was written: if the config file itself calls `find_dependency(OpenSSL)` unconditionally, or if OpenSSL was actually linked `PUBLIC`/`INTERFACE` rather than `PRIVATE` in the real target declaration, `find_package(netcore REQUIRED)` will transitively require OpenSSL to also be found on the downstream machine even though no `netcore` public header needs it. The fix, in terms of visibility keyword: ensure `OpenSSL::SSL` is genuinely linked `PRIVATE` (not `PUBLIC`/`INTERFACE`) on the real `netcore` target, and ensure the generated/hand-written package config does not add an unconditional `find_dependency(OpenSSL)` call that isn't actually needed by the exported interface.

### Level 5 — Integration

**10-P28.**
```cmake
add_library(geolib_internal_codegen STATIC src/codegen_helpers.cpp)
target_include_directories(geolib_internal_codegen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal)

add_library(geolib_obj OBJECT ${GEOLIB_SOURCES})
target_include_directories(geolib_obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(geolib_obj PRIVATE geolib_internal_codegen)

add_library(geolib_static STATIC $<TARGET_OBJECTS:geolib_obj>)
target_include_directories(geolib_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(geolib_shared SHARED $<TARGET_OBJECTS:geolib_obj>)
target_include_directories(geolib_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
```
The internal header-generation tool's directory (`internal/`) is now attached only `PRIVATE` to `geolib_internal_codegen`/`geolib_obj`, never `PUBLIC` on either `geolib_static` or `geolib_shared` — only the intended `include/` directory is `PUBLIC`. Both variants build from the same object-library source set (`$<TARGET_OBJECTS:geolib_obj>`) without duplicating the source list. A downstream consumer target attempting `#include <geolib/internal/codegen_helpers.h>` now fails to resolve, since that directory was never added to its include path.

---

**10-P29.**
```json
{
  "configurePresets": [
    { "name": "base", "hidden": true, "binaryDir": "${sourceDir}/build/${presetName}" },
    { "name": "msvc-debug", "inherits": "base", "generator": "Visual Studio 17 2022" },
    { "name": "msvc-release", "inherits": "base", "generator": "Visual Studio 17 2022", "cacheVariables": {"CMAKE_BUILD_TYPE": "Release"} },
    { "name": "wsl-clang-debug", "inherits": "base", "generator": "Ninja", "toolchainFile": "${sourceDir}/cmake/clang-toolchain.cmake" },
    { "name": "wsl-gcc-debug", "inherits": "base", "generator": "Ninja", "toolchainFile": "${sourceDir}/cmake/gcc-toolchain.cmake" },
    { "name": "wsl-clang-asan", "inherits": "wsl-clang-debug", "cacheVariables": {"CMAKE_CXX_FLAGS": "-fsanitize=address"} },
    { "name": "wsl-clang-tsan", "inherits": "wsl-clang-debug", "cacheVariables": {"CMAKE_CXX_FLAGS": "-fsanitize=thread"} }
  ]
}
```
A GoogleTest suite for a `ThreadSafeCounter` (mutex-guarded `increment()`/`value()`) runs clean under every preset. Deliberately removing the mutex guard from `increment()` and rebuilding: `msvc-debug`, `msvc-release`, `wsl-clang-debug`, and `wsl-gcc-debug` all still build and "pass" (the race doesn't reliably manifest as a test failure), while only `wsl-clang-tsan`'s run reports the data race explicitly, with both racing accesses' stack traces — demonstrating that ordinary test passing is not evidence of race-freedom, only a TSan run is.

---

**10-P30.**
```json
{ "name": "wsl-clang-fuzz", "inherits": "wsl-clang-debug", "cacheVariables": {"CMAKE_CXX_FLAGS": "-fsanitize=fuzzer,address"} }
```
Running the URL-parser fuzz target from 10-P28's project against a seed corpus surfaces a genuine crash — e.g. a URL with an unterminated percent-escape sequence (`%` as the last character) causing an out-of-bounds read two bytes past the input when decoding the escape. `-minimize_crash=1` reduces it to a two-or-three-byte minimal input. A regression test, `TEST(UrlParser, TrailingPercentDoesNotOverread) { EXPECT_NO_FATAL_FAILURE(parse_url("%")); }`, is added to the GoogleTest suite; it fails (ASan-detected overread) against the pre-fix parser and passes once the percent-decoding loop bounds-checks for a trailing incomplete escape.

---

**10-P31.**
```cmake
include(GNUInstallDirs)
install(TARGETS geolib_shared EXPORT geolibTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT geolibTargets FILE geolibTargets.cmake NAMESPACE geolib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geolib)
install(FILES geolibConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geolib)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "geolib")
include(CPack)
```
`cpack` produces a `geolib-<version>-<platform>.zip` distributable archive. A from-scratch downstream project's `CMakeLists.txt` (`find_package(geolib REQUIRED)`, `target_link_libraries(app PRIVATE geolib::geolib_shared)`) configures and builds successfully against the installed package, needing zero knowledge of `geolib`'s own internal codegen dependency (per 10-P27's `PRIVATE` boundary), confirmed by inspecting `app`'s generated build commands for the absence of any reference to the internal codegen library.

### Level 6 — Production

**10-P32.** A concrete, incremental process: (1) write a small script (or a CMake-file-API-based tool) that enumerates every `target_link_libraries(... PUBLIC ...)` call across the codebase and cross-references it against each target's actual public headers (via a simple grep for the dependency's types/includes in headers under that target's public include directory) to flag candidates where `PUBLIC` is very likely wrong — no public header references the dependency at all; (2) for each flagged candidate, change it to `PRIVATE` in isolation, in its own small PR, and rebuild every consumer of that target (not just the target itself) in CI to confirm nothing downstream was actually relying on the leaked transitive dependency (a green CI run across all consumers is the actual safety proof, not just "this one target still builds"); (3) merge one flagged fix at a time rather than batching many together, so a break is immediately attributable to a single, small, revertible change. This avoids a big-bang rewrite by treating each visibility correction as its own independently-verified, independently-revertible unit of work.

---

**10-P33.** Rollout plan: (1) introduce ASan first, not TSan/UBSan/MSan — ASan is the most broadly applicable (catches memory-safety bugs present in almost any nontrivial C++ codebase), runs on both toolchains (so it's not purely a WSL-only addition, lowering the bar to adopt), and its violations are usually easier to triage individually (a specific bad access at a specific line) than a data race's often-nonlocal cause; (2) for the likely flood of pre-existing ASan violations on first run, avoid both extremes — mass-disable (which defeats the purpose and risks becoming permanent) and mass-fix-before-merging-the-job (which blocks adoption for months) — instead land the CI job in a non-blocking ("informational," report-only) mode immediately, triage and fix the existing violations over a bounded, tracked timeframe (e.g. a few weeks, one ticket per violation), then flip the job to blocking once the backlog reaches zero; (3) to prevent the new job from being treated as optional the way 10-P25's unenforced `wsl-clang-tsan` preset was, make the transition to "blocking" an explicit, calendared, tracked commitment from the outset (not an indefinitely-deferred someday), and once blocking, treat any attempt to skip/ignore a red sanitizer job exactly as seriously as a red compile — no merges land on top of it. TSan follows the same non-blocking-then-blocking pattern after ASan's backlog is clear, since TSan violations are typically more architecturally invasive to fix (synchronization redesign, not a local bounds fix).

---

**10-P34.** A concrete safeguard: add a CI step, run as part of every scheduled fuzz job, that independently checks the fuzz-target binary's build timestamp (or a build-content hash, e.g. of the compiled binary or its build manifest) against the current commit's source tree, and fails the job explicitly if the fuzz binary is older than the last commit that touched the code it's supposed to be fuzzing — distinguishing "the fuzz target ran and found nothing" from "the fuzz target's binary is stale and wasn't actually testing current code" as two different, separately-alertable failure conditions. Concretely, this can be implemented as a small script comparing `git log -1 --format=%ct -- src/parser/` against the fuzzer binary's mtime (or, more robustly, embedding a build-time source-hash constant into the fuzzer binary itself via a generated header, and asserting at fuzz-run start that it matches the current tree's hash) — either approach converts "silently stale" into a loud, immediate CI failure rather than a silent, months-long false sense of coverage.

### Level 7 — Principal Reasoning

**10-P35.** Policy, with justification per choice: (1) **Pin a conservative CMake minimum version** (e.g. the oldest version still receiving security patches at release time, not the newest available) via `cmake_minimum_required(VERSION X)`, because a customer's own toolchain/CMake install lags the library maintainer's by default, and pinning too aggressively new a minimum is a first-attempt failure mode entirely within the library's control to avoid. (2) **Expose only `msvc-release`/`msvc-debug` and a single documented `clang-gcc-release`-style preset to customers**, keeping `wsl-clang-tsan`/`wsl-clang-asan`/fuzz presets internal-only — customers building their own product don't need (and shouldn't be required to have) a WSL-based sanitizer toolchain just to consume a released library; sanitizer/testing infrastructure is the library maintainer's *internal* quality gate, not a customer-facing build requirement, and exposing it as if it were required would be a needless first-attempt failure surface. (3) **Enforce the C++ standard requirement via `target_compile_features(mylib PUBLIC cxx_std_20)` on the exported target itself, not prose documentation alone**, because a customer whose own build silently compiles under an insufficient standard, discovering the mismatch only via a confusing header-level compiler error deep in the library's internals, is a strictly worse first-attempt experience than CMake itself raising their effective standard or failing configure with a clear, top-level diagnostic (per 10-P20's mechanism) — documentation can be skipped or missed; a `target_compile_features` requirement cannot silently fail to apply. The deliberate tradeoff being accepted: the internal team gives up the freedom to change build-system internals (adding new required internal presets, restructuring the internal target graph, adopting new internal tooling) without careful attention to what remains customer-facing, since anything exposed to customers becomes something the multi-year support window implicitly promises not to break without notice — internal agility is traded for external build-success reliability.

## Integration Challenge Solution — 10-IC1

1. **Diagnose the leak completely.** Two independent mechanisms are leaking: (a) `target_link_libraries(iolib_static PUBLIC zstd::libzstd)` (and the `iolib_shared` equivalent) propagates `zstd`'s link requirement to every consumer — any executable linking `iolib_static`/`iolib_shared` is forced to also link `zstd::libzstd` on its own link line, even though it never calls a `zstd` function directly; (b) a `target_include_directories(iolib_static PUBLIC ${ZSTD_INCLUDE_DIR})` (or an overly broad `PUBLIC` include directory that happens to also contain a header transitively including `zstd.h`) exposes `zstd`'s headers on every consumer's include path, meaning a consumer's own translation units could accidentally `#include <zstd.h>` and start depending on `iolib`'s internal compression library version, entirely by accident. Concretely, a downstream consumer currently sees: `zstd`'s public headers on its own include path (even though it never asked for them), and a forced link dependency on `zstd::libzstd` that it did not choose and would break if `iolib` ever swapped compression libraries.

2. **Fix the target graph.**
```cmake
add_library(iolib_obj OBJECT ${IOLIB_SOURCES})
target_include_directories(iolib_obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(iolib_obj PRIVATE zstd::libzstd)

add_library(iolib_static STATIC $<TARGET_OBJECTS:iolib_obj>)
target_include_directories(iolib_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(iolib_static PRIVATE zstd::libzstd)

add_library(iolib_shared SHARED $<TARGET_OBJECTS:iolib_obj>)
target_include_directories(iolib_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(iolib_shared PRIVATE zstd::libzstd)
```
Both variants build from the shared `iolib_obj` object library's compiled objects (no duplicated source-list maintenance), and `zstd::libzstd` is now `PRIVATE` on every target that actually needs it, while only the genuine public `include/` directory remains `PUBLIC`.

3. **Prove it with a downstream consumer.** A downstream target `app` linking `iolib_shared` (`target_link_libraries(app PRIVATE iolib::iolib_shared)`) is confirmed, by inspecting the actual generated build commands (`ninja -t commands app` or the MSBuild link invocation), to have no `zstd` include path (`-I.../zstd/include` absent from its compile command) and no `zstd` link library (`-lzstd`/`zstd.lib` absent from its link command) — while `iolib_shared`'s own compile/link commands still correctly reference `zstd`'s headers and library, and `app` still correctly benefits from `iolib`'s internal compression at runtime (compression still works when exercised through `iolib`'s own public API).

4. **Add a regression guard.** A CI step that, after configuring the project, greps the generated build system (`compile_commands.json`, produced via `CMAKE_EXPORT_COMPILE_COMMANDS=ON`) for any downstream consumer target's compile command containing a `zstd` include path, and fails the build if found — since after the fix, `zstd` should never appear in any target's compile command except `iolib_obj`/`iolib_static`/`iolib_shared` themselves. This directly generalizes 10-P32's audit technique (cross-referencing `PUBLIC` dependencies against what a target's own public headers actually reference) into an automated, continuously-enforced check rather than a one-time manual audit, and would have caught this exact leak the moment `zstd::libzstd` was first marked `PUBLIC`, rather than relying on a downstream consumer eventually noticing an unexpected dependency months later.
