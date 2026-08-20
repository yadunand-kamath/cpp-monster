# P-3.3 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch10's build-systems material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

This project has almost no interesting runtime logic — its entire value is in whether the build configuration *actually* enforces what it claims to. Before writing any CMake, write down, in plain language, every guarantee a future project will lean on ("running one command runs all the tests," "a consumer can't reach private headers," "the sanitizer preset actually catches bugs") and treat each one as a requirement to be *proven*, not assumed, exactly as the Testing Requirements section demands.

## Hint 2 — Technique

CMake's `target_include_directories` has three visibility keywords for a reason: `PRIVATE` paths are used to build the target itself but never handed to anything that links against it; `PUBLIC` paths are used internally *and* propagated to consumers; `INTERFACE` paths are propagated to consumers but not used by the target itself (relevant for header-only pieces). Getting the public/private header split right is entirely about which keyword each `target_include_directories` call uses — there is no additional enforcement mechanism needed beyond using these correctly and consistently through to your `install(TARGETS ... EXPORT ...)` / `add_subdirectory` usage.

## Hint 3 — Implementation

`CMakePresets.json` supports a `binaryDir` field that can be templated per-preset (e.g. using the `${presetName}` macro) — use this to guarantee every preset gets an isolated build directory, which sidesteps an entire category of "works when built alone, breaks when built after another preset" bugs. For sanitizer flags, both AddressSanitizer/UndefinedBehaviorSanitizer (`-fsanitize=address,undefined`) and ThreadSanitizer (`-fsanitize=thread`) are compiler+linker flags you can inject via a preset's `cacheVariables` (`CMAKE_CXX_FLAGS`, `CMAKE_EXE_LINKER_FLAGS`) — no separate CMake modules are required for the basics.

## Hint 4 — Debugging/Design

If a test passes when you build a preset in isolation but the aggregate script reports it failing (or the reverse), suspect stale CMake cache state from a previously-configured preset bleeding into the current one — this almost always traces back to presets sharing a build directory, either because `binaryDir` wasn't set per-preset or because an old, manually-created build directory from before you adopted presets is still lying around and being reused. Deleting all build directories and reconfiguring from scratch is the fastest way to confirm or rule this out.
