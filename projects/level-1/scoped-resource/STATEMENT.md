# P-1.2 — Scoped Resource Handle Family

**Level:** 1 (Focused component) · **Category:** Systems · **Requires:** Ch01–02 · **Est. effort:** S (4-8h)

## Objective

Build a small family of move-only RAII wrapper types — a scoped file handle, a scoped timer, and a fully generic `scope_exit` utility — that guarantee resource cleanup exactly once, on every exit path (normal return, early return, and exception unwinding), with no possibility of a double-close or a leaked resource under correct use.

## Functional Requirements

1. **`scoped_file`** — wraps a platform file handle (`FILE*` via the C stdio API is acceptable and portable across this workbook's MSVC/WSL toolchains; a raw OS handle is also acceptable if you want the extra realism) and closes it exactly once on destruction, unless the handle has been released or moved from.
2. **`scoped_timer`** — on construction, records a start time; on destruction, computes the elapsed duration and reports it via a caller-supplied callback (e.g. `std::function<void(std::chrono::nanoseconds)>`, or a template parameter if you prefer to avoid the type-erasure overhead — your choice, but justify it).
3. **`scope_exit`** — a fully generic utility taking any invocable with no arguments and calling it exactly once on destruction, unless dismissed. Must work with lambdas, function pointers, and `std::function`.
4. All three types must be **move-only**: copy construction and copy assignment must be deleted or otherwise disabled, and move construction/move assignment must correctly transfer ownership such that the moved-from object no longer performs the cleanup action on its own destruction.
5. Each type must support an explicit **release/dismiss** operation that hands back or cancels ownership of the underlying resource/action without it being cleaned up by this object — e.g. `scoped_file::release()` returns the raw handle and disarms the wrapper; `scope_exit::release()` (or `dismiss()`) cancels the pending call entirely.
6. `scope_exit` specifically must correctly run its action during **stack unwinding from an exception**, not just on normal scope exit — this must be demonstrated, not merely assumed.

## Input

No runtime input format in the traditional sense — deliver the header-only (or small header+source) library plus a demonstration/test program. If your `scoped_file` demo opens a real file, use a temporary file path constructed at test time (do not hardcode a path that assumes a specific filesystem layout).

## Output

Correctness is demonstrated via GoogleTest assertions on: whether the underlying resource was actually released (e.g. a counter incremented by a fake "close" callback, so you don't need to inspect real OS-level file-descriptor tables to prove closure happened), whether it was released *exactly once*, and whether move semantics transferred ownership correctly.

## Constraints

- C++20, `/std:c++20` MSVC primary; portable to WSL Clang/GCC (avoid anything Windows-`HANDLE`-specific in the *generic* `scope_exit`; `scoped_file` may be platform-specific if you choose the raw-OS-handle route, but then must have a working implementation on both).
- No heap allocation for `scope_exit` unless a captured lambda's state doesn't fit inline (if you use `std::function`, document the allocation this can introduce — this is intentionally revisited in Ch12 as a *measured* cost, not something to hand-wave now).
- Must be exception-safe: if the wrapped resource's cleanup call itself can throw (e.g. `fclose` failing), your destructor must not let that exception escape (destructors that throw during stack unwinding cause `std::terminate` — decide and document how you handle this).

## Edge Cases

- Destructing a default-constructed / already-released wrapper must not attempt to clean up anything (no double-close, no null-pointer dereference).
- Move-assigning a `scoped_file` (or `scope_exit`) that is currently holding a live resource into another that is *also* currently holding a live resource — what happens to the *target's* previously-held resource? (This is a real, easy-to-get-wrong case — think it through explicitly.)
- Self-move-assignment (`a = std::move(a);`) — while unusual, your type should not crash or double-free in this case.

## Error Handling

- If the underlying resource's cleanup operation can fail (e.g. `fclose` returning a nonzero error code), specify and document what your destructor does with that failure — since a destructor generally must not throw, at minimum log or otherwise surface the failure through a non-throwing channel (an error callback, a global/thread-local last-error slot, or simply documented as "cleanup failures are not observable from the destructor by design" if you make that explicit trade-off).
- Constructing a `scoped_file` from a failed `fopen` (null `FILE*`) should be a well-defined, documented state — decide whether this throws, or whether the wrapper is simply constructed in an "empty" state the caller must check.

## Acceptance Criteria

- All three types pass a GoogleTest suite covering construction, move, release/dismiss, and (for `scope_exit`) exception-path execution.
- AddressSanitizer-clean (`/fsanitize=address` on MSVC or `-fsanitize=address` on WSL Clang) run over the full test suite — no double-free, no use-after-free.
- No copy constructor/assignment callable — enforced via `static_assert(!std::is_copy_constructible_v<...>)`.
- Builds cleanly under `/W4 /permissive-` with zero warnings.

## Testing Requirements

- Unit tests for construction, destruction-triggers-cleanup, move-transfers-ownership, release-disarms, and self-move-assignment safety.
- At least one test that deliberately throws an exception from inside a scope guarded by `scope_exit` and asserts the cleanup action still ran exactly once.
- An ASan run as part of your verification (documented in your submission notes, even if not wired into an automated CI step for this project specifically).

## Hints

### Hint 1 — Direction
All three of these are variations on the same underlying pattern: acquire a resource or commit to an action at construction time, and guarantee it happens exactly once by the time the object's lifetime ends — including lifetimes ended early by an exception unwinding through the stack. Think about what language mechanism guarantees a specific function runs when an object's storage duration ends, regardless of *how* control left that scope.

### Hint 2 — Technique
For "move-only, correctly disarms the moved-from object," think about what state a moved-from object needs to be left in so that its own destructor becomes a no-op — and where exactly that disarming needs to happen (in the move constructor/assignment operator itself, not just hoped for).

### Hint 3 — Implementation
For move assignment specifically, consider what must happen to the *target's* currently-held resource before you overwrite it with the source's — forgetting this step is one of the most common ways this kind of wrapper leaks a resource silently. For `scope_exit`'s exception-safety requirement, consider whether your destructor's cleanup-invocation logic needs to look any different from a "normal path" cleanup at all — what actually differs about a destructor call during unwinding versus during ordinary scope exit, from the object's own point of view?

### Hint 4 — Debugging/Design
If your ASan run reports a double-free, check every code path that could leave two live objects believing they both own the same underlying resource simultaneously — move construction/assignment and self-move-assignment are the two most likely culprits. If a destructor-that-could-throw is causing a `std::terminate` you didn't expect during your exception-path test, remember that a destructor invoked during stack unwinding from another exception, if it itself throws, terminates the program immediately — your cleanup logic inside the destructor needs a plan for its own failure that does not involve throwing.
