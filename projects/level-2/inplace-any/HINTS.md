# P-2.2 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch07's object-model/layout material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

You need one piece of storage — sized and aligned to accommodate whatever type is currently held — plus some way to remember, at runtime, what operations (copy, move, destroy) are appropriate for whatever's currently in that storage, since the raw bytes themselves carry no information about their own type. Think about what a single `inplace_any<N>` instance needs to carry around alongside its bytes, at different points in its life, to still behave correctly when it holds completely unrelated types at different times.

## Hint 2 — Technique

Consider a small struct of function pointers (or a hand-rolled vtable) generated once per contained type via a template function — pointers to "copy this specific type out of this buffer into that buffer," "move it," and "destroy it," each instantiated for the specific `T` currently held at the moment of construction. For the trivially-copyable fast path, think about what those three operations actually reduce to when `T` is trivially copyable — a `memcpy` and nothing else — and how `if constexpr` combined with `std::is_trivially_copyable_v<T>` lets you generate genuinely different code at compile time rather than a runtime branch that pays for both possibilities.

## Hint 3 — Implementation

For alignment correctness, your inline buffer's declared alignment needs to be at least the maximum alignment you intend to support — think about whether that should be a single fixed value chosen once, or a second template parameter alongside `N` that callers can adjust for stricter needs (this workbook's test suite specifically checks an over-aligned type, so whichever you choose needs to actually work for that case). For reassignment across different held types, make sure your assignment operator's sequence of operations is: destroy whatever is currently held (using the operations captured for *that* type), then construct the new value — reversing or skipping either half is the most common bug in hand-rolled type-erasure containers.

## Hint 4 — Debugging/Design

If your instrumented-type test shows more constructions or destructions than expected during a reassignment (holding type `A`, then assigned a new value of type `B`), check that your assignment logic actually destroys the old value using *its own* correct destructor — captured via the function-pointer/vtable set up for type `A` — before constructing the new value in the same storage. Reusing the buffer without destroying what's there first is undefined behavior even when it happens not to visibly crash. If an over-aligned type test fails or corrupts data only in a larger program (not in an isolated small test), suspect that your buffer's declared alignment is not actually what you assumed — double check the `alignas` value against what the specific type under test actually requires, not what you intended to support in general.
