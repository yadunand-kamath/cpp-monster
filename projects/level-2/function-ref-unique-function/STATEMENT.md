# P-2.3 — `function_ref` and `unique_function`

**Level:** 2 (Multi-concept component) · **Category:** Libraries · **Requires:** Ch01–05 · **Est. effort:** M (10-16h)

## Objective

Build two complementary callable-wrapper types: `function_ref<Sig>`, a non-owning, zero-allocation view over any callable matching a signature (for use as a function parameter, never stored), and `unique_function<Sig>`, an owning, move-only callable wrapper with small-buffer optimization for the common case of small captures, contrasted explicitly against `std::function`'s copyable-and-therefore-more-constrained design.

## Functional Requirements

1. `function_ref<R(Args...)>` can be constructed from any lvalue callable (a lambda, function object, or function pointer) matching the signature, storing only a type-erased pointer to the callable plus a function pointer to invoke it — never taking ownership, never copying the callable, never allocating.
2. `function_ref` must be trivially copyable itself (copying a `function_ref` just copies the two pointers) and must have a lifetime the caller is responsible for — document explicitly that a `function_ref` must not outlive the callable it refers to, matching the semantics of `std::string_view`'s own "you manage the lifetime" contract.
3. `unique_function<R(Args...)>` owns its callable, is move-only (no copy), and stores small callables (a documented inline-capacity threshold) without heap allocation, falling back to heap allocation only for callables exceeding that threshold.
4. `unique_function` must correctly support move-only captured state (e.g. a lambda capturing a `std::unique_ptr` by move) — something `std::function` (which requires its target to be copy-constructible) cannot do, and this project must demonstrate that difference concretely.
5. Both types must support calling through `operator()`, forwarding arguments and the return value correctly, including for `void`-returning signatures and signatures with reference parameters.
6. Provide a comparative note/benchmark: constructing and invoking a `unique_function` wrapping a small capture vs. `std::function` wrapping the same capture, specifically measuring whether `std::function`'s implementation (which is allowed, but not required, to have its own SBO) allocates in your specific standard library implementation for this case — report the actual observed behavior of your toolchain's `std::function`, not an assumption about it.

## Input

None — library components exercised by a GoogleTest suite and a short comparison program/benchmark.

## Output

None beyond test/benchmark results.

## Constraints

- C++20.
- `function_ref` must add no lifetime management overhead — it should be exactly two pointers in size (a `void*`-equivalent context pointer and a function pointer), no vtable, no reference counting.
- `unique_function`'s inline-capacity threshold must be a documented, deliberate choice (not an arbitrary leftover number), and heap fallback (for oversized captures) must work correctly, not just inline-fitting cases.

## Edge Cases

- A `function_ref` constructed from a lambda that is then immediately used within the same full expression (safe) versus stored past the lambda's lifetime (the documented misuse case — not required to be preventable, but must be documented clearly, mirroring `std::string_view`'s dangling-reference risk).
- `unique_function` wrapping a capture-less lambda (should this even need the SBO machinery, or can it degenerate to something closer to a bare function pointer? — worth considering, not required to special-case).
- Moving a `unique_function` that currently holds a heap-allocated (oversized) callable — must transfer the heap pointer, not copy the captured data.
- Calling an empty/moved-from `unique_function` — decide and document whether this throws, is UB (matching raw function-pointer-call-on-null semantics), or has some other well-defined empty-call behavior.

## Error Handling

- Calling an empty `unique_function` (default-constructed or moved-from) — must be a documented, deliberate behavior, not accidental UB you didn't think about.
- `function_ref` has no "empty" state by design (it's always constructed from a valid callable) — if you allow a default-constructed empty `function_ref`, document that as an explicit extension beyond the minimal contract, and document its call-when-empty behavior too.

## Acceptance Criteria

- Passes a GoogleTest suite covering `function_ref` over lambdas/function pointers/function objects, `unique_function`'s SBO fast path and heap fallback, move-only captured state, and correct forwarding for various signatures including `void` returns and reference parameters.
- `static_assert(sizeof(function_ref<void()>) == 2 * sizeof(void*))` (or your documented equivalent minimal size) as a submitted proof, not just an assumption.
- A demonstrated case where `unique_function` successfully wraps a `std::unique_ptr`-capturing lambda and `std::function` provably cannot (a compile-failure case, documented per this workbook's compile-failure-case convention).
- Builds cleanly under `/W4 /permissive-`.

## Testing Requirements

- Correctness tests for both types across the signature/capture variations listed above.
- An allocation-counting test for `unique_function`'s SBO threshold (below threshold: zero allocations; above: exactly one).
- The `std::function`-cannot-hold-move-only-capture compile-failure documentation.
- A move-transfers-heap-pointer-not-data test for oversized `unique_function` captures (verifiable via observing the underlying storage address before and after the move, or via an instrumented type's construction count staying at exactly one across the move).

## Hints

### Hint 1 — Direction
`function_ref` and `unique_function` solve related but distinct problems: one needs to reference an *existing* callable someone else owns, cheaply and temporarily; the other needs to *own* a callable, potentially outliving the scope it was created in, without necessarily requiring that callable to be copyable. Think about why a type designed for the first job should be forbidden from doing the second (owning), and why a type designed for the second job needs different storage than "just two pointers."

### Hint 2 — Technique
For `function_ref`, think about what two pieces of information are sufficient to both remember *which* callable to invoke and know *how* to invoke it, generically, for any callable type matching a given signature — a pointer to the callable's storage, and a pointer to a small non-capturing function that knows how to cast that storage back to the right type and call it with the given arguments. For `unique_function`'s SBO, this is structurally very close to the type-erasure mechanism from [P-2.2](../inplace-any/STATEMENT.md) — inline storage, a small per-type dispatch table generated once per captured-callable type — except here the "operations" you need are specifically "invoke with these arguments," "move," and "destroy," not "copy" (since `unique_function` doesn't need to be copyable at all).

### Hint 3 — Implementation
For `function_ref`'s "invoke" function pointer, think about how a free function template, instantiated per concrete callable type, can capture that type information at the point of construction (via being instantiated with the actual callable's type) while the function pointer's own type erases it away — this is the same "type-specific behavior behind a type-erased pointer" pattern used throughout this workbook's library projects, applied here in its simplest possible form (no vtable struct needed, just one function pointer). For `unique_function`'s move-only support specifically, make sure your invoke/move/destroy operation functions are generated in a way that never requires the contained callable to be copy-constructible — a template that happens to also generate a copy operation (even if unused) could fail to compile for a genuinely move-only capture.

### Hint 4 — Debugging/Design
If your `unique_function`-wrapping-a-move-only-capture test fails to compile with an error mentioning a missing or deleted copy constructor somewhere deep in your own template code (rather than in the user's lambda), check whether any part of your implementation — even a code path that's never actually executed for this specific instantiation — still requires `T` to be copyable, such as a helper that's `if constexpr`-guarded incorrectly, or a fallback path that unconditionally instantiates a copy-based operation. Templates instantiate based on what's referenced, not what's executed, so an unused-but-still-instantiated copy path is a common way this requirement silently breaks.
