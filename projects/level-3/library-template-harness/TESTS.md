# P-3.3 — Tests

## Visible Tests (GoogleTest)

```cpp
// A trivial library function, just enough to prove the wiring works end-to-end.
TEST(TemplateLib, PublicApiFunctionWorks) {
    EXPECT_EQ(mylib::add(2, 3), 5);
}

TEST(TemplateLib, PublicApiHandlesEdgeInput) {
    EXPECT_EQ(mylib::add(-1, 1), 0);
}
```

## Compile-Failure Case

```cpp
// consumer_project/main.cpp — deliberately attempts to reach a private header.
#include "internal_detail.h" // should fail: not on any propagated include path
int main() { return 0; }
```
Document the actual compiler error observed (a "file not found" style error, since the private include directory is never propagated to the consumer target).

## Manual/Script-Driven Verification

- Run the aggregate script with the codebase in its clean, intended state: all presets configure, build, and pass.
- Temporarily introduce a failing assertion in one test, rerun the script, and capture that the summary correctly identifies that specific preset/test as failed while others still report success.
- Temporarily introduce an unused-variable (or similarly warning-worthy) line, rerun the strict-warnings preset, and capture the build failing due to warnings-as-errors; remove `/WX` (or `-Werror`) temporarily to confirm the same line only warns, not errors, proving the safeguard is doing real work rather than the line being silently ignored either way.
- Temporarily introduce a heap-buffer-overflow (or signed-integer-overflow) sample function, rerun the ASan/UBSan preset, and capture the sanitizer's crash report; rebuild the same sample under the plain debug preset (no sanitizer) to confirm it does *not* get caught there, proving the sanitizer preset is the thing actually catching it.
- Build a trivial intentionally-racy sample (two threads incrementing a non-atomic shared counter) under the TSan preset and capture the data-race report.

## Hidden Tests

- configuring the project from a completely clean clone with zero tests written yet, confirming configure+build still succeeds (no assumption that at least one test already exists)
- the consumer project's positive case: successfully calling `mylib::add` after `add_subdirectory`/`find_package`, confirming the public path *does* propagate correctly (the compile-failure case alone doesn't prove the public path works)
- deliberately breaking network access during a from-scratch configure, checking that the `FetchContent` failure produces a clear configure-time error rather than an obscure later build failure
- verifying each preset configures into its own separate build directory, by configuring two presets back-to-back and confirming neither's cached state leaks into the other's build output
