# P-2.2 — SBO Variant Storage: `inplace_any<N>`

**Level:** 2 (Multi-concept component) · **Category:** Libraries · **Requires:** Ch01–04, Ch07 · **Est. effort:** M (10-16h)

## Objective

Build a type-erased value container, similar in spirit to `std::any`, that stores any object fitting within a fixed-size inline buffer without heap allocation, correctly handles types that don't fit (via a documented policy), and provides a fast path for trivially-copyable types that avoids virtual dispatch entirely.

## Functional Requirements

1. `inplace_any<N>` can hold a value of any type `T` (copyable, and move-constructible at minimum) whose `sizeof(T) <= N` (and satisfies your chosen alignment constraint) entirely inline, with no heap allocation.
2. Provide a documented, explicit policy for `T` that does not fit inline: either a compile-time rejection (`static_assert` on construction) or a fallback heap allocation for oversized types — pick one and justify it; "undefined behavior for oversized types" is not an acceptable policy.
3. Support querying the currently held type (`type()` returning `std::type_info` or similar) and retrieving the value via a type-checked accessor (returns/throws or returns an optional/pointer on mismatch — your choice, documented).
4. Correctly copy-construct, move-construct, copy-assign, and move-assign `inplace_any` instances holding arbitrary contained types, correctly invoking the contained type's own copy/move semantics rather than a shallow byte copy for non-trivial types.
5. Provide a fast path — detectable via `std::is_trivially_copyable_v<T>` — where holding a trivially-copyable type avoids any per-operation virtual call/function-pointer indirection, falling back to a small vtable-like dispatch mechanism (function pointers or a hand-rolled vtable struct) only for non-trivial types.
6. Destruction of the held value must correctly invoke the contained type's actual destructor (not just "clear the bytes") for non-trivial types.

## Input

None — library component exercised by a GoogleTest suite and a small benchmark comparing the trivially-copyable fast path against the non-trivial dispatch path.

## Output

None beyond test/benchmark results.

## Constraints

- C++20. Alignment of the inline buffer must be correct for any `T` you accept — an inline buffer aligned only to 1 or to `alignof(std::max_align_t)` regardless of what's actually needed is not acceptable if it either wastes space unnecessarily or (worse) doesn't actually satisfy a stricter-than-assumed alignment requirement.
- No RTTI dependency required for the type-check mechanism if you don't want one, but if you do use `std::type_info`/`typeid`, document that choice and its cost (a small amount of RTTI overhead per type, not per instance).
- The trivially-copyable fast path must be verified (not just claimed) to avoid the non-trivial dispatch machinery — a benchmark or a static check showing zero indirect calls for that path is required.

## Edge Cases

- Storing a type whose size exactly equals `N` (boundary case, not off-by-one).
- Storing a type with an alignment requirement stricter than the buffer's natural alignment (e.g. a SIMD type requiring 16 or 32-byte alignment) — does your buffer's `alignas` correctly handle this, or does it silently produce a misaligned object?
- Re-assigning an `inplace_any` currently holding type `A` with a new value of a different type `B` — the old `A` must be destroyed correctly before `B` is constructed in its place.
- Copying an `inplace_any` that is empty (holds no value).

## Error Handling

- Accessing the held value via the type-checked accessor with the wrong type — document whether this throws (`std::bad_any_cast`-style), returns a null/empty result, or is a checked precondition violation, and be consistent.
- Constructing with an oversized type — per your chosen policy (compile-time rejection preferred; document if you instead chose heap fallback and how that's signaled).

## Acceptance Criteria

- Passes a GoogleTest suite covering trivial types, non-trivial types (with observable constructor/destructor/copy/move behavior via an instrumented type), over-aligned types, empty state, copy/move/reassignment across different held types.
- A benchmark demonstrating the trivially-copyable fast path measurably avoids the dispatch overhead paid by the non-trivial path (even a modest, honestly-reported difference is fine — the point is measuring, not necessarily achieving a huge number).
- Builds cleanly under `/W4 /permissive-`.
- No heap allocation for any type satisfying the inline-fit constraint, verified via an allocation-counting test.

## Testing Requirements

- Type-correctness tests using an instrumented type (reuse or extend the `Tracked` type from P-1.4 if convenient) to verify exactly one construction and exactly one destruction occur per logical value held, across copy/move/reassignment operations.
- An over-aligned type test (e.g. a type with `alignas(32)`).
- An allocation-counting test proving zero heap allocation for in-bounds types.
- The oversized-type policy test (either a `static_assert`-failure documented case, matching P-1.1's compile-failure documentation pattern, or a heap-fallback-triggered allocation-count test).

## Hints

### Hint 1 — Direction
You need one piece of storage — sized and aligned to accommodate whatever type is currently held — plus some way to remember, at runtime, what operations (copy, move, destroy) are appropriate for whatever's currently in that storage, since the storage itself is just bytes and doesn't know what's in it. Think about how a single `inplace_any<N>` instance, at different points in its life, holds completely unrelated types — what does it need to carry around, alongside the raw bytes, to still behave correctly?

### Hint 2 — Technique
Consider a small struct of function pointers (or a hand-rolled vtable) generated once per contained type via a template function, storing pointers to "copy this specific type out of this buffer into that buffer," "move it," and "destroy it" — each instantiated for the specific `T` currently held. For the trivially-copyable fast path, think about what those three operations actually reduce to when `T` is trivially copyable (a `memcpy` and nothing else) and how you might detect that case at compile time to skip generating or calling through the vtable machinery entirely.

### Hint 3 — Implementation
For alignment correctness, your inline buffer's declared alignment needs to be at least the maximum alignment you intend to support — think about whether that should be a fixed value you pick once, or a second template parameter alongside `N` that callers can adjust for stricter needs. For "detect the fast path at compile time," `std::is_trivially_copyable_v<T>` combined with `if constexpr` inside your construction/copy/move/destroy logic lets you generate genuinely different code paths for trivial versus non-trivial `T`, rather than a runtime branch that still pays for the possibility of both paths.

### Hint 4 — Debugging/Design
If your instrumented-type test shows more constructions or destructions than expected during a reassignment (holding type `A`, then assigned a new value of type `B`), check that your assignment logic actually destroys the old value using *its* correct destructor (via the vtable/function-pointer captured for type `A`) before constructing the new value in the same storage — reusing the buffer without destroying what's there first is undefined behavior even if it happens not to crash. If an over-aligned type test fails or misbehaves subtly (works in a small test but corrupts data in a larger program), suspect that your buffer's `alignas` value is not actually being honored for the alignment your test type requires — check what alignment you declared versus what the type actually needs.
