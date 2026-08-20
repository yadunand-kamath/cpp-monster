# P-2.4 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch06's error-handling Crash Course before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Think about what a type needs to safely hold *either* a `T` or an `E` at any given time, without wasting space on both simultaneously and without requiring either to be default-constructible. This is a strong hint toward a specific kind of storage you've likely already built a variant of elsewhere in this workbook's earlier projects — applied here to exactly two possible alternatives instead of an arbitrary open set of types.

## Hint 2 — Technique

For `and_then`, think about its signature: given a `Result<T,E>` and a function `T -> Result<U,E>`, what should the combined operation return, and specifically what should happen to the function call itself when the input `Result` is already in the error state — should the function even be invoked? For `map`, contrast this with a function `T -> U` that does *not* return a `Result` itself — why does `map` need to wrap the function's return value in a new `Result` for the caller, while `and_then` can trust the function to already return one?

## Hint 3 — Implementation

For `Result<void, E>`, consider what changes if you specialize (partially or fully) your primary `Result<T,E>` template for `T = void` — specifically, which member functions stop making sense (an accessor for "the value" when there is no value type to hold at all) and which continue to make sense completely unchanged (checking success/failure, accessing the error). Think about whether a full specialization or a more surgical `if constexpr`-based conditional within a single template better fits how much of the implementation genuinely differs for the void case versus how much stays identical.

## Hint 4 — Debugging/Design

If your `and_then` chain doesn't actually short-circuit — a later stage's code runs even though an earlier stage returned an error — check whether your `and_then` implementation unconditionally calls the provided function and only *afterward* checks whether the original `Result` held an error, rather than checking the error state *first* and skipping the function call entirely when it's already in the error state. This ordering mistake is easy to make if you write `and_then` structurally symmetric to `map`, but the two have a genuinely different control-flow shape — `map` always has something to transform on the success path; `and_then` needs to decide whether to call anything at all.
