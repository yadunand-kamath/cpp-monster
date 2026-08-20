# P-1.3 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread the relevant Ch01–02 object-lifetime sections before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

You need a piece of storage that can hold up to `N` fully-constructed `T` objects without requiring `T` to be default-constructible, which rules out a plain array member `T storage[N]` for any `T` lacking a default constructor. Think about what kind of raw storage lets you control exactly when and how each `T` object is constructed and destroyed within it, independent of the storage's own lifetime — and how that differs from an array of already-constructed objects.

## Hint 2 — Technique

Consider representing your inline buffer as uninitialized storage that reserves the correct size and alignment for `N` `T` objects but does not itself construct any of them, using placement construction and explicit destructor calls to manage the objects that live inside it. For deciding whether `data()` currently points into the inline buffer or a heap buffer, think about what single piece of state your container needs to track this — and whether that state can be derived from something you already store (like comparing a capacity value against `N`) rather than needing an extra flag.

## Hint 3 — Implementation

For the transition itself, when growing past `N` (or past current heap capacity): you need to move-construct each existing element into the new storage, destroy the old objects, and only then update your internal "where is my data" state. Think carefully about ordering if any of those moves could throw partway through — that ordering decision is exactly what determines your exception-safety guarantee. For deciding move vs. copy during this transition, recall that `std::vector` doesn't always move — it only moves when doing so can't compromise the guarantee it promises, falling back to copying when a type's move constructor isn't `noexcept`; the same reasoning applies here if you want the strong guarantee.

## Hint 4 — Debugging/Design

If your allocation-count test unexpectedly shows an allocation happening below `N` elements, check whether your `push_back` implementation unconditionally goes through a "grow" code path rather than checking against the *current* effective capacity, which starts at `N`, not at some smaller default. If a throwing-constructor test leaves your container's reported size and its actually-constructed-object count disagreeing (a common symptom: the destructor later double-destroys or under-destroys elements), check that you only count an element as "constructed" — and thus something your destructor must clean up — at the exact point its construction succeeds, not before. A strong-guarantee implementation typically constructs everything in the *new* storage first and only destroys the *old* storage after every new construction has succeeded, so that a failure partway through leaves the original storage completely untouched.
