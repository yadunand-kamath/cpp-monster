# P-3.3 — Reusable Library Template & Test Harness

**Level:** 3 (Realistic utility) · **Category:** Dev Tools · **Requires:** Ch08–10 · **Est. effort:** M (10-16h)

## Objective

Build the CMake-based project template and test-running infrastructure that every later Level-3+ project (and both capstones) will be submitted against: a library skeleton with a clean public/private split, GoogleTest wired through CTest, sanitizer-enabled build presets, and a CI-style script that runs the full verification matrix in one command.

## Functional Requirements

1. A CMake project structure separating a library target's public headers (installable/exportable) from its private implementation headers and sources, using `target_include_directories` with correct `PUBLIC`/`PRIVATE`/`INTERFACE` scoping — not a flat "everything is public" layout.
2. GoogleTest integrated via `FetchContent` (not a vendored copy, not a system-installed dependency assumption), with test targets registered in CTest so `ctest` alone runs the full suite.
3. At least three build presets (via `CMakePresets.json`): a default debug build, an MSVC `/W4 /permissive-` strict-warnings build, and a WSL Clang/GCC build with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
4. A documented, minimal ThreadSanitizer preset (WSL-only, per this workbook's toolchain notes) for projects that need it — this template project itself need not have concurrency to test, but it must demonstrate the preset is correctly configured (e.g. against a trivial intentionally-racy sample) so later concurrent projects can adopt it directly.
5. A single script (shell or CMake-driven) that builds and runs every preset's test suite in sequence and reports a clear pass/fail summary — the "one command runs everything" contract every later project's Acceptance Criteria will assume exists.
6. Warnings-as-errors enabled in at least the strict preset, and the template must itself be clean under it (demonstrating the preset isn't accidentally impossible to satisfy).

## Input

None at runtime — this is a project template plus a verification script, exercised by building and running it against itself.

## Output

A clean CMake project that configures, builds, and tests successfully across all documented presets, plus a script producing a pass/fail report.

## Constraints

- CMake ≥3.24 (for full presets support). C++20.
- No committed/vendored third-party source — GoogleTest arrives via `FetchContent` at configure time.
- The public/private header split must be enforced structurally (a consumer of the installed library cannot `#include` a private header even if they try), not just by a naming convention or comment.

## Edge Cases

- Building with zero tests present still succeeds cleanly (a fresh clone before any test is written should not fail to configure).
- A test that intentionally fails must produce a non-zero exit code from the aggregate script, and the script's summary must clearly identify which preset/test failed.
- The ASan/UBSan preset actually catches a deliberately introduced bug (a heap-buffer-overflow or a signed-integer-overflow sample) — the sanitizer wiring must be proven to work, not merely assumed configured correctly.
- The strict-warnings preset actually rejects a deliberately introduced warning-worthy line (unused variable, implicit narrowing) — again, proven, not assumed.

## Error Handling

- A missing/unreachable network connection during `FetchContent`'s GoogleTest download — should produce a clear CMake configure error, not a cryptic downstream build failure.
- An unsupported/missing sanitizer on a given compiler/platform combination — the preset should fail configuration with a clear message rather than silently building without sanitizer instrumentation.

## Acceptance Criteria

- `cmake --preset <name>` followed by `cmake --build` and `ctest` succeeds for the debug and strict-warnings presets on MSVC, and for the debug, ASan/UBSan, and TSan-demo presets under WSL.
- The aggregate script run once reports a correct pass/fail summary across all presets, including a deliberately-broken run (one test made to fail on purpose, one warning deliberately introduced) demonstrating the script correctly surfaces failures rather than only ever reporting success.
- A downstream "consumer" CMake project (a second, tiny throwaway project in the same repo) can `add_subdirectory` or `find_package` this template's library and successfully call its public API, while a deliberate attempt to `#include` one of its private headers fails to compile.

## Testing Requirements

- At least one real unit test (exercising some trivial library function) proving the GoogleTest+CTest wiring genuinely works end-to-end, not just configures.
- The sanitizer-catches-a-real-bug demonstration (ASan/UBSan) and the strict-warnings-rejects-a-real-warning demonstration, both with before/after evidence (the sample fails/warns when the safeguard is active, and the safeguard can be temporarily disabled to show the same sample passing without it — proving the safeguard is doing real work).
- The consumer-project public/private-header-boundary test described in Acceptance Criteria.

## Hints

### Hint 1 — Direction
This project is infrastructure, not a library with interesting runtime logic — its "correctness" is entirely about whether the build system enforces the boundaries and runs the checks it claims to. Approach it by first listing every guarantee a *later* project's Acceptance Criteria will assume exists ("tests run via one command," "private headers aren't reachable," "sanitizers are actually active") and then building the minimal CMake structure that makes each guarantee literally true, one at a time.

### Hint 2 — Technique
For the public/private header enforcement, `target_include_directories(mylib PUBLIC include PRIVATE src)` only controls what *this target's* consumers can see when they link against `mylib` — the key is making sure the install/export step (or, for an in-tree consumer via `add_subdirectory`, the target's usage requirements) only ever propagates the `PUBLIC` include path, never the `PRIVATE` one, so a consumer genuinely cannot resolve `#include "internal_detail.h"` even if they try.

### Hint 3 — Implementation
`CMakePresets.json` lets you define named configurations with their own generator, build type, and cache variables (like `CMAKE_CXX_FLAGS` for `/W4 /permissive-` or `-fsanitize=address,undefined`) without needing separate build scripts per configuration — each preset is just a different set of cache variables layered onto the same `CMakeLists.txt`. For the "one command runs everything" script, the simplest robust approach is a loop (shell script, or a CMake script run via `cmake -P`) that configures-builds-tests each preset in its own separate build directory (so presets never contaminate each other's cached CMake state) and aggregates each step's exit code.

### Hint 4 — Debugging/Design
If a later project's tests pass locally but the aggregate script reports failure (or vice versa), the most common cause is a stale or shared build directory between presets — each preset should configure into its own distinct binary directory (`CMakePresets.json`'s `binaryDir` field, parameterized by preset name) so switching between, say, the debug and ASan presets doesn't silently reuse cached configuration from the other.
