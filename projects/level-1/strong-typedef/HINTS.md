# P-1.1 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread the relevant Ch01 Crash Course section before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

You need a type that is distinguished not by the data it stores, but by an *extra template parameter that carries no runtime information at all* — a type that exists purely so that two otherwise-identical template instantiations become, to the compiler, two completely unrelated types. Think about what makes two instantiations of the same class template "the same type" versus "different types" in C++, and how you could exploit that rule deliberately.

## Hint 2 — Technique

For the explicit-construction requirement, recall the difference between a constructor the compiler is allowed to invoke implicitly (for an argument conversion) and one it is only allowed to invoke when you write the type out explicitly at the call site. For the "same-tag operations only" requirement, think about whether the operator should be a plain, unconstrained free function, a member function, or a function template constrained (via a `requires` clause or `static_assert`, or simply by only being *declared* for matching tags) so that a mismatched pair of tags never resolves to a callable overload at all — as opposed to compiling and failing at link time or runtime.

## Hint 3 — Implementation

For the derived-unit requirement (`Meters / Seconds → MetersPerSecond`), you need a way to say "the result type of dividing a `Tag1`-quantity by a `Tag2`-quantity is specifically `Tag3`" — and this relationship is inherently per-pair-of-tags, not something a single generic template can guess. Consider a small trait template that you specialize once per meaningful tag pair, which your `operator/` consults to determine its return type — this keeps arbitrary, meaningless tag combinations from silently compiling into a bogus derived unit nobody asked for. Separately: your *scalar* multiplication (`Quantity * raw_T`) needs to be a genuinely different overload from your *tag-to-tag* multiplication/division — make sure the two don't accidentally collide or shadow each other in overload resolution.

## Hint 4 — Debugging/Design

If cross-tag operations that should fail to compile are instead compiling successfully, the most common cause is an overly permissive templated constructor or conversion function — for instance, a constructor template `template <typename OtherTag> Quantity(const Quantity<T, OtherTag>&)` with no constraint forcing `OtherTag` to equal `Tag` will silently accept *any* other tag's quantity as convertible. Audit every constructor and every operator overload you've written and ask, for each one: "if I instantiate the type variables involved with two different tags, does this still compile?" — if the answer is yes and it shouldn't be, that's your leak.
