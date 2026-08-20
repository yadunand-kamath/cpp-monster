# P-5.6 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch08's ABI material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Write the shared interface header first, in isolation, before any host or plugin code — and hold it to a strict standard: every member of every struct that crosses the boundary must be a type whose in-memory layout is fixed by the C ABI (not the C++ ABI, which varies across compilers and even compiler versions for certain types). If you're unsure whether a type qualifies, the safe rule is: if it would compile as plain C with `extern "C"`, it qualifies; if it needs C++-only syntax to express, it doesn't.

## Hint 2 — Technique

Every plugin-side function reachable from the host must be structured as: `extern "C" ReturnType function_name(...) { try { /* real C++ logic here */ } catch (...) { /* translate to an error code/out-parameter */ return error_sentinel; } }` — the `catch (...)` at the outermost layer is non-negotiable for any function that could throw, since letting any exception reach the `extern "C"` function boundary and propagate through it is undefined behavior, not merely bad practice.

## Hint 3 — Implementation

A minimal RAII plugin handle: constructor calls `dlopen`/`LoadLibrary`, resolves the entry-point symbol via `dlsym`/`GetProcAddress`, calls it to obtain the interface struct pointer, checks its version field, and either stores the validated pointer or throws/returns an error (cleaning up the partially-opened library handle either way); destructor calls `dlclose`/`FreeLibrary` if a handle is held. The accessor method returning the interface pointer should be the only way to reach it — no public raw pointer member that could be copied out and outlive the handle.

## Hint 4 — Debugging/Design

If a call into a plugin's function crashes with what looks like stack or memory corruption rather than a clean failure, suspect a struct-layout mismatch before suspecting a logic bug — this happens when the host and plugin were compiled against slightly different versions of the shared interface header (a stale build, a forgotten rebuild after an interface change) and end up disagreeing about a struct's field offsets, which is exactly the class of bug the version-check requirement in Functional Requirements exists to catch *if the version field itself is bumped whenever the interface changes* — a version check only protects against a mismatch if the version number was actually incremented, which is a discipline the host can enforce for its own supported versions but not something it can force on every plugin author.
