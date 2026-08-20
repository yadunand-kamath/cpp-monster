# P-2.5 — Compile-Time Reflection-Free Serializer

**Level:** 2 (Multi-concept component) · **Category:** Libraries · **Requires:** Ch01–05, Ch07 · **Est. effort:** M/L (10-18h)

## Objective

Build a binary serialization library that lets a user describe a struct's fields once (via an explicit, minimal per-type registration mechanism — this is "reflection-free" precisely because C++ has no compile-time reflection to lean on) and get correct, versioned, endian-correct serialization and deserialization generated at compile time, with no runtime type information and no per-field hand-written serialize/deserialize code duplicated for reading and writing.

## Functional Requirements

1. Provide a per-type registration mechanism (e.g. a `describe_fields()` free function or member returning a tuple of member-pointer-plus-name pairs) that a user writes once per struct, from which both serialization and deserialization are generated — the same registration must drive both directions, so a user cannot accidentally update one without the other.
2. Support at minimum: fixed-width integers, floating-point types, `bool`, `std::string`, and nested user-defined struct types (structs containing other registered structs) — recursively.
3. Serialize to a well-defined, documented binary format that is explicitly endian-correct (little-endian on the wire, regardless of host endianness, is a reasonable default — document your choice) — meaning multi-byte integers must be byte-order-normalized on write and reversed on read on a big-endian host, not just `memcpy`'d as raw host bytes.
4. Support a schema version number embedded in the serialized output, and support at least one realistic schema evolution scenario: deserializing older-version data into a struct that has since gained a new field (with a documented default-value policy) without corrupting the rest of the data.
5. Deserialization of malformed or truncated input must be detected and reported as a documented error (via this project's own `Result`-style approach from [P-2.4](../result-error-propagation/STATEMENT.md) if you've built it, or a simpler bool/exception scheme if not) — not undefined behavior from reading past a buffer's end.

## Input

In-memory structs to serialize; byte buffers to deserialize (from files or in-memory, your choice for the demo).

## Output

Binary-serialized byte buffers; deserialized struct instances; a demonstration program round-tripping several struct types including the versioned-schema-evolution case.

## Constraints

- C++20. The registration mechanism must not require the user to write separate "read" and "write" code paths per field — one field description drives both directions, which is the core anti-duplication requirement of this project.
- No `reinterpret_cast`-based direct-memory-layout serialization for anything beyond trivially-copyable, fixed-layout primitive types, and even then, only after byte-order normalization — structs are serialized field-by-field via the registration mechanism, not by dumping their raw memory layout (which would bake in padding, platform-specific layout, and endianness as accidental, undocumented parts of your "format").
- Must correctly round-trip on both a little-endian test host and (if you have access to one, e.g. via a big-endian QEMU target or an endianness-swap self-test) demonstrate the endian-correctness logic actually does something observable — if you cannot test on real big-endian hardware, a unit test that manually byte-swaps and confirms round-trip correctness against the swapped representation is an acceptable substitute, and should be documented as such.

## Edge Cases

- An empty `std::string` field, and a `std::string` field containing embedded null bytes (length-prefixed encoding, not null-terminated, is required specifically because of this case).
- A struct containing another struct containing another struct (at least 3 levels of nesting) — confirming the recursive registration mechanism actually recurses correctly rather than only working one level deep.
- Deserializing a buffer that is truncated mid-field (e.g. a length prefix claims more bytes than remain in the buffer) — must be detected, not read past the buffer.
- A schema-version mismatch where the version is newer than the reader understands — decide and document whether this is a hard failure or an attempted best-effort read.

## Error Handling

- Truncated/malformed input during deserialization — a documented, non-UB error result.
- A registered field type with no serialization support (e.g. a raw pointer, which cannot be meaningfully serialized) — ideally a compile-time error (via `static_assert` or a `concept` constraining what field types are acceptable) rather than a confusing runtime failure or, worse, silent misbehavior.

## Acceptance Criteria

- Passes a GoogleTest suite covering all required primitive types, nested structs (3+ levels), the empty-string and embedded-null-byte string cases, and the truncated-buffer detection case.
- The schema-evolution demonstration successfully deserializes an old-version buffer into a newer struct definition with a documented default-value policy for the new field.
- Endian-correctness demonstrated per the documented approach (real big-endian test or documented byte-swap self-test).
- Builds cleanly under `/W4 /permissive-`.

## Testing Requirements

- Round-trip tests (serialize then deserialize, compare equal) for every supported primitive type and for nested structs.
- The truncated-buffer and malformed-input detection tests.
- The schema-evolution round-trip test.
- The endian-correctness test (real or documented-substitute).

## Hints

### Hint 1 — Direction
Since C++ has no built-in compile-time reflection to enumerate a struct's fields for you, the core design problem is: what's the minimal, one-time piece of code a user must write per struct so that a template-based serializer can iterate "all of this struct's fields" generically? Think about what a list of (member pointer, field name or index) pairs gives you, and how a function template operating on such a list — rather than on the struct type directly — could implement both serialize and deserialize from the exact same list.

### Hint 2 — Technique
Consider representing a struct's field description as a `std::tuple` of pointer-to-member values (one per field), returned by a function you write once per struct. A generic `serialize`/`deserialize` function template can then use `std::apply` or a compile-time fold over that tuple to visit each field in turn, dispatching to a per-type "write this primitive" or "recurse into this nested struct" operation based on the field's actual type (deduced from the pointer-to-member's type) — this is what makes one registration drive both directions: the direction (read vs. write) is a parameter to the generic visit, not something baked into the registration itself.

### Hint 3 — Implementation
For endian correctness, you need a small, tested byte-swap primitive for each fixed-width integer size, applied consistently at exactly one layer (either always swap-on-write-and-swap-on-read symmetrically, or normalize to a canonical wire order and only swap when the host's order differs from it) — pick the approach and apply it uniformly rather than sprinkling ad hoc byte manipulation through your field-visiting code. For schema evolution, think about what information the serialized format needs to carry (a version number, most simply) so that your deserializer can decide, per field, "does this field exist in the version I'm reading, or do I need to supply a default" — this requires your registration mechanism to be able to express "this field was added in version N," not just "this struct has this field."

### Hint 4 — Debugging/Design
If your round-trip test for a nested struct (2+ levels deep) fails only at the deepest level while shallower cases pass, check whether your generic serialize/deserialize function is correctly recursing into nested registered types — a common mistake is writing the primitive-type dispatch case correctly but forgetting to add (or incorrectly conditionally excluding) the case where a field's type is itself a registered struct rather than a primitive, causing the visitor to try to serialize the nested struct as raw bytes instead of recursing into its own field list.
