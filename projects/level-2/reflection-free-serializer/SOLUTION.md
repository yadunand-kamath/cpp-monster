# P-2.5 — Solution

## Reference Architecture

A per-type `describe_fields` free function returning a tuple of member pointers, a generic writer/reader visiting that tuple via `std::apply`, and per-primitive-type overloads (resolved via overload resolution on the field's deduced type) that either write raw byte-swapped bytes or recurse into `describe_fields` again for nested struct types.

```cpp
// User-written registration (once per struct, drives both directions):
struct Point { int32_t x, y; };
inline auto describe_fields(Point*) { return std::make_tuple(&Point::x, &Point::y); }

// Generic byte-swap primitive:
template <typename T> T to_wire_order(T v) {
    if constexpr (sizeof(T) == 1) return v;
    else { T r; auto* src = reinterpret_cast<const unsigned char*>(&v);
           auto* dst = reinterpret_cast<unsigned char*>(&r);
           for (std::size_t i = 0; i < sizeof(T); ++i) dst[i] = src[sizeof(T) - 1 - i];
           return r; }
}

// Per-field write dispatch, resolved by overload resolution on the field's type:
template <typename T>
void write_field(std::vector<std::byte>& out, const T& field) {
    if constexpr (std::is_arithmetic_v<T>) {
        T wire = to_wire_order(field);
        auto* bytes = reinterpret_cast<const std::byte*>(&wire);
        out.insert(out.end(), bytes, bytes + sizeof(T));
    } else if constexpr (std::is_same_v<T, std::string>) {
        write_field(out, static_cast<uint32_t>(field.size())); // length-prefixed, not null-terminated
        auto* bytes = reinterpret_cast<const std::byte*>(field.data());
        out.insert(out.end(), bytes, bytes + field.size());
    } else {
        // nested registered struct: recurse using its own describe_fields
        std::apply([&](auto... member_ptrs) {
            (write_field(out, field.*member_ptrs), ...);
        }, describe_fields(static_cast<T*>(nullptr)));
    }
}

template <typename T>
void serialize_into(std::vector<std::byte>& out, const T& obj) {
    std::apply([&](auto... member_ptrs) {
        (write_field(out, obj.*member_ptrs), ...);
    }, describe_fields(static_cast<T*>(nullptr)));
}
```

Deserialization mirrors this exactly, replacing `write_field` with a `read_field` that advances a cursor through the buffer and returns a `Result<T, DeserializeError>` (per [P-2.4](../result-error-propagation/STATEMENT.md)), checking remaining-bytes-available before every multi-byte read.

## Design Rationale

**Why member pointers in a tuple, rather than, say, field offsets or a macro-generated switch statement?** A pointer-to-member (`&Point::x`) carries the field's exact type statically — `field.*member_ptr` gives you a correctly-typed reference to that specific member, letting overload resolution (the `if constexpr` chain in `write_field`) pick the right serialization behavior automatically, entirely at compile time. Offsets would require manual `reinterpret_cast`-based access (exactly the raw-memory-layout approach this project's constraints explicitly forbid), and a macro-generated switch would need to duplicate the read and write logic per field rather than genuinely sharing one generic path.

**Why byte-swap rather than `memcpy`ing raw host bytes onto the wire?** `memcpy`-ing raw bytes bakes the host's native endianness into the wire format as an accidental, undocumented property — data serialized on a little-endian host would silently corrupt if ever read on a big-endian host, with no way for the reader to even detect it happened. Normalizing to a documented wire order (little-endian, chosen here) on every write, and reversing that normalization on every read exactly when the host's native order differs from the wire order, makes the format's byte order an explicit, testable contract rather than an implicit assumption.

**Why does `write_field`'s nested-struct branch call `describe_fields` again rather than requiring the outer struct's registration to flatten inner fields?** Requiring flattening would defeat reuse — every struct that happened to contain a `Point` would need to duplicate `Point`'s own field list inline. Recursing through each nested type's own `describe_fields` means a struct's registration only ever needs to describe *its own direct* fields; nesting composes for free, which is exactly the "at least 3 levels deep" requirement's point — the recursion has no depth limit baked in, because it's genuine recursion through the type system, not a fixed number of hand-written levels.

## Reference Implementation

The above covers the write path's core structure; deserialization requires the mirror-image `read_field`, always checking `cursor + sizeof(T) <= buffer.size()` (or the length-prefix-implied size, for strings) before reading, returning an error `Result` immediately on any such check failing — propagated up through the same recursive structure via `and_then`-style short-circuiting. Remaining work for a complete submission:
1. `read_field` overloads mirroring each `write_field` branch, each returning `Result<T, DeserializeError>`.
2. A version byte/number prepended once at the start of `serialize_into`'s output, consulted by the top-level `deserialize<T>` entry point to decide per-field whether to read or supply a default (for the schema-evolution requirement) — this needs each field's registration to optionally carry an "added in version N" annotation, which is the one piece of the registration mechanism not shown above and left as the interesting remaining design work.
3. A `static_assert`/concept constraining `write_field`'s template parameter to arithmetic types, `std::string`, or types with a valid `describe_fields` overload — rejecting anything else (like a raw pointer field) at compile time with a clear message.

## Testing Strategy

Test the byte-swap primitive in isolation first (a known input value, a known expected wire-order byte sequence) before trusting any higher-level round-trip test — a round-trip test alone cannot distinguish "correct byte-swapping" from "no byte-swapping at all, but read and write happen to cancel out symmetrically on this host," which is exactly why the endian-correctness test needs to inspect the actual wire bytes directly (as in the `serialize_raw_int32_le` example test), not just round-trip and compare final values.

## Performance Analysis

Every serialize/deserialize call is fully resolved at compile time down to a fixed sequence of direct field accesses and byte-level operations — no runtime type dispatch, no heap allocation beyond the output buffer's own growth (amortized, same as any `std::vector<std::byte>` append). The recursive nested-struct handling costs nothing at runtime beyond what a hand-written, per-struct serializer would cost — the abstraction is compiled away, which is the entire motivation for building this at compile time rather than via a runtime reflection/visitor system.

## Failure Modes

- Reading past the end of a truncated buffer without a bounds check — undefined behavior, exactly what the Error Handling requirements exist to prevent; every multi-byte read must check remaining length first.
- Forgetting the length-prefix approach for strings and instead null-terminating — breaks immediately on the embedded-null-byte test case, and is a common instinct carried over from C-string habits that this project specifically exists to correct.
- A schema-evolution implementation that reads fields positionally without consulting the version number at all — works by accident when schemas never change, and silently misreads data the moment a real field is added or reordered.

## Extensions

- A schema-description-generation step producing a human-readable summary of a struct's wire format (field names, types, byte offsets) directly from `describe_fields`, useful for debugging and for external tooling that needs to read your format without your C++ code.
- JSON or another human-readable format as a second backend sharing the same `describe_fields` registration, demonstrating the registration mechanism's genericity across encodings, not just binary.
