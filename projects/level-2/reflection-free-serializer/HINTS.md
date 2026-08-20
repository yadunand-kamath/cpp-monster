# P-2.5 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch05's compile-time/generic-programming material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Since C++ has no built-in compile-time reflection to enumerate a struct's fields for you, the core design problem is: what's the minimal, one-time piece of code a user must write per struct so that a template-based serializer can generically iterate "all of this struct's fields"? Think about what a list of (member pointer, field identity) pairs gives you, and how a function template operating on such a list — rather than on the struct type directly — could implement both serialize and deserialize from that exact same list.

## Hint 2 — Technique

Consider representing a struct's field description as a `std::tuple` of pointer-to-member values, returned by a small function written once per struct. A generic `serialize`/`deserialize` function template can then use `std::apply` or a compile-time fold over that tuple to visit each field in turn, dispatching to a per-type "write this primitive" or "recurse into this nested struct" operation based on the field's actual type, deduced from the pointer-to-member's type. This is exactly what makes one registration drive both directions: the *direction* (reading vs. writing) becomes a parameter to the generic visiting logic, never something baked into the registration itself.

## Hint 3 — Implementation

For endian correctness, build one small, tested byte-swap primitive per fixed-width integer size, and apply it at exactly one consistent layer — either always swap-on-write and swap-on-read symmetrically, or normalize to a single canonical wire byte order and only swap when the host's native order differs from it. Pick one approach and apply it uniformly everywhere a multi-byte value crosses the wire boundary, rather than handling endianness ad hoc per call site. For schema evolution, think about what information the serialized format itself needs to carry — most simply, a version number — so your deserializer can decide, per field, whether that field exists in the version being read or whether a documented default must be supplied instead; this requires your registration mechanism to be able to express "this field was added in version N," not merely "this struct has this field."

## Hint 4 — Debugging/Design

If your round-trip test for a nested struct fails only at the deepest level of nesting while shallower cases pass, check whether your generic serialize/deserialize function actually recurses into nested registered types at every level — a common mistake is writing the primitive-type dispatch case correctly but forgetting to handle (or incorrectly excluding) the case where a field's type is itself a registered struct, causing the visitor to try to serialize that nested struct as raw bytes instead of recursing into its own field list. Check this specifically at the boundary between "one level of nesting" (which often works by accident even with a subtly wrong recursion check) and "two or more levels" (which exposes the bug clearly).
