# P-2.3 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch05's generic-programming material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

`function_ref` and `unique_function` solve related but distinct problems: one needs to reference an *existing* callable someone else owns, cheaply and temporarily; the other needs to *own* a callable, potentially outliving the scope it was created in, without necessarily requiring that callable to be copyable. Think about why a type designed for the first job should be forbidden from doing the second (owning), and why a type designed for the second job needs fundamentally different storage than "just two pointers."

## Hint 2 — Technique

For `function_ref`, think about what two pieces of information are sufficient to both remember *which* callable to invoke and know *how* to invoke it generically, for any callable type matching a given signature: a pointer to the callable's storage, and a pointer to a small non-capturing function that knows how to cast that storage back to the right type and call it with the given arguments. For `unique_function`'s SBO, this is structurally very close to the type-erasure mechanism from [P-2.2](../inplace-any/STATEMENT.md) — inline storage plus a small per-type dispatch table generated once per captured-callable type — except here the operations you need are specifically "invoke with these arguments," "move," and "destroy," not "copy," since `unique_function` doesn't need to be copyable at all.

## Hint 3 — Implementation

For `function_ref`'s invoke function pointer, think about how a free function template, instantiated once per concrete callable type, can capture that type information at the point of construction (via being instantiated with the actual callable's type) while the resulting function pointer's own type erases it away — this is the same "type-specific behavior behind a type-erased pointer" pattern used throughout this workbook's library projects, applied here in its simplest form (a single function pointer, no vtable struct needed). For `unique_function`'s move-only support specifically, make sure your invoke/move/destroy operation functions are generated in a way that never requires the contained callable to be copy-constructible anywhere — a template that happens to also generate a copy operation, even one that's never called for this instantiation, can fail to compile for a genuinely move-only capture.

## Hint 4 — Debugging/Design

If your `unique_function`-wrapping-a-move-only-capture test fails to compile with an error pointing deep into your own template code (rather than into the user's lambda), check whether any part of your implementation — even a code path that's never actually executed for this specific instantiation — still requires `T` to be copyable, such as a helper guarded incorrectly by `if constexpr`, or a fallback path that unconditionally references a copy-based operation. Templates instantiate based on what's *referenced*, not what's *executed*, so an unused-but-still-instantiated copy path is a common, easy-to-miss way this requirement silently breaks.
