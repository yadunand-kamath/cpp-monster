# P-3.3 — Solution

## Reference Architecture

```
template-project/
├── CMakeLists.txt
├── CMakePresets.json
├── scripts/run_all_presets.sh (or .ps1)
├── include/mylib/mylib.h        # PUBLIC
├── src/internal_detail.h        # PRIVATE
├── src/mylib.cpp
├── tests/test_mylib.cpp
└── consumer_project/            # throwaway downstream-consumer proof
    ├── CMakeLists.txt
    └── main.cpp
```

Top-level `CMakeLists.txt`, showing the visibility split and GoogleTest wiring:

```cmake
add_library(mylib src/mylib.cpp)
target_include_directories(mylib
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(mylib PUBLIC cxx_std_20)

include(FetchContent)
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(mylib_tests tests/test_mylib.cpp)
target_link_libraries(mylib_tests PRIVATE mylib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(mylib_tests)
```

`CMakePresets.json`, showing per-preset isolated build directories and sanitizer injection:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "debug",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "msvc-strict",
      "binaryDir": "${sourceDir}/build/msvc-strict",
      "cacheVariables": { "CMAKE_CXX_FLAGS": "/W4 /permissive- /WX" }
    },
    {
      "name": "wsl-asan-ubsan",
      "binaryDir": "${sourceDir}/build/wsl-asan-ubsan",
      "cacheVariables": {
        "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer",
        "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined"
      }
    },
    {
      "name": "wsl-clang-tsan",
      "binaryDir": "${sourceDir}/build/wsl-clang-tsan",
      "cacheVariables": {
        "CMAKE_CXX_FLAGS": "-fsanitize=thread",
        "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=thread"
      }
    }
  ]
}
```

The aggregate script's core loop:

```bash
#!/usr/bin/env bash
set -uo pipefail
overall=0
for preset in debug msvc-strict wsl-asan-ubsan wsl-clang-tsan; do
    echo "=== preset: $preset ==="
    cmake --preset "$preset" && cmake --build --preset "$preset" && ctest --preset "$preset"
    status=$?
    [ $status -ne 0 ] && { echo "FAILED: $preset"; overall=1; }
done
exit $overall
```

## Design Rationale

**Why `PUBLIC`/`PRIVATE` on `target_include_directories` instead of a naming convention like an `internal/` folder with a comment?** A naming convention is advisory — nothing stops a consumer from `#include`-ing whatever they can find a path to. CMake's usage-requirements propagation is structural: a `PRIVATE` include path is genuinely never exported into the target's usage requirements, so a consumer linking against `mylib` has no compiler include-path entry that would let `#include "internal_detail.h"` resolve, regardless of what the consumer tries. This is the difference between "please don't" and "cannot," which is exactly what the Constraints section calls for.

**Why isolate each preset into its own `binaryDir` rather than reusing one build directory and reconfiguring in place?** CMake caches configuration decisions (compiler flags, detected sanitizer support, dependency locations) in `CMakeCache.txt`. Reconfiguring the same directory with different cache variables can leave stale entries that were never explicitly overridden, silently corrupting a later preset's build with an earlier preset's settings. Giving every preset its own directory makes each one a fully independent, reproducible build from a clean slate — the failure mode this avoids is exactly the one called out in Hint 4.

**Why prove the sanitizer and warnings-as-errors presets against deliberately-introduced bugs rather than trusting the configuration once it "looks right"?** A sanitizer or warning flag that's misspelled, placed in the wrong CMake variable, or silently ignored by the compiler (e.g. due to flag ordering or a missing linker flag) will often still let a normal build succeed — the absence of a caught bug is not evidence the safeguard works, only that nothing happened to trigger a false sense of security. Deliberately introducing a known bug and confirming it's caught (and *not* caught when the safeguard is disabled) is the only way to actually validate the configuration is live.

## Reference Implementation

The above covers the full core shape. Remaining work for the learner: the `consumer_project/`'s `CMakeLists.txt` (via `add_subdirectory(..)` pointing at the template project, then `target_link_libraries(consumer PRIVATE mylib)`), the compile-failure demonstration file and captured error text, and the PowerShell equivalent of the aggregate script for the MSVC presets (since the shell script above assumes a POSIX shell, appropriate for the WSL presets but not for a native MSVC preset run from PowerShell).

## Testing Strategy

Each safeguard (sanitizers, warnings-as-errors) needs a before/after demonstration specifically because a build system claim that "isn't tested" is exactly as unreliable as application code that isn't tested — the fact that this is infrastructure rather than business logic doesn't exempt it from the same discipline.

## Performance Analysis

Not applicable in the traditional sense — the relevant "performance" concern here is configure/build time across four separate preset directories, which is a real but acceptable cost (each preset is a from-scratch build) traded for the reliability of never sharing cache state.

## Failure Modes

- A `PUBLIC` include path accidentally used where `PRIVATE` was intended, silently exposing internal headers to consumers.
- Reusing one build directory across presets, causing intermittent, hard-to-reproduce cross-contamination between configurations.
- A sanitizer flag applied to compilation but not to linking (or vice versa), which can produce confusing link errors or, worse, silently non-functional instrumentation.
- A strict-warnings preset that happens to already be warning-free on day one, giving false confidence it would actually catch a new warning later — hence the deliberate-warning demonstration requirement.

## Extensions

- A GitHub Actions (or equivalent) CI workflow file invoking the same aggregate script, turning local verification into an actual CI gate.
- A code-coverage preset (`--coverage`/`gcov`/`llvm-cov`) added to the same pattern.
