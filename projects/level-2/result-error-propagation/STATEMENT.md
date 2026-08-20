# P-2.4 — Result/Error Propagation Library

**Level:** 2 (Multi-concept component) · **Category:** Libraries · **Requires:** Ch01–06 · **Est. effort:** M/L (10-18h)

## Objective

Build an `expected<T, E>`-style result type (or, if you prefer, a well-justified adapter over `std::expected` if your toolchain has it) with monadic composition operations (`map`/`and_then`/`or_else`), and use it to build a small multi-step pipeline (e.g. parse → validate → transform → persist) with zero exceptions thrown on any expected failure path, demonstrating that error handling via values composes at least as cleanly as exceptions for this kind of pipeline.

## Functional Requirements

1. `Result<T, E>` represents either a success value of type `T` or an error value of type `E`, queryable (`has_value()`, `operator bool()`), with accessors for the success value and the error value that are only valid to call in the matching state (document the contract clearly).
2. Provide `map` (transform the success value, pass through errors unchanged), `and_then` (chain a fallible operation, short-circuiting on error), and `or_else` (recover from or transform an error) as composable member or free functions.
3. Support `Result<void, E>` (a success case carrying no value) without contorting the rest of the API — this is a common real-world need (an operation that can fail but has nothing to return on success) and a genuine design wrinkle worth solving deliberately.
4. Build a small demonstration pipeline (parse a structured input → validate its fields → transform it into a different representation → "persist" it, e.g. write to a file or an in-memory store) with at least 4 distinct, named error kinds/variants across the whole pipeline, where a failure at any stage short-circuits the remaining stages and the caller receives specific, actionable error information — not a generic "something went wrong."
5. Compare, in your submission notes, the exception-based version of the same pipeline (write a short parallel version) against your `Result`-based version on: call-site verbosity, whether error-handling code is visually separable from happy-path code, and whether the compiler enforces that callers actually deal with the error case (or can silently ignore it) in each style.

## Input

Whatever structured input format you choose for the demonstration pipeline (a simple line-based or key-value format is sufficient — this project isn't about parser sophistication).

## Output

Console output from the demonstration pipeline showing at least one full success run and multiple distinct-error-kind failure runs, each showing which stage failed and why.

## Constraints

- C++20 (or C++23 if your toolchain and you prefer to build directly on `std::expected` — document your choice either way).
- No exceptions used for any of the pipeline's *expected* failure modes (a malformed input, a validation failure, a persistence failure are all expected, not exceptional, in this domain) — reserve exceptions, if used at all, only for genuinely unexpected/programmer-error conditions (e.g. calling `.value()` on an error-state `Result`), consistent with this workbook's Ch06 error-handling philosophy.
- `Result<T, E>` must not require `T` or `E` to be default-constructible.

## Edge Cases

- `Result<void, E>` — accessing "the value" on success should either not exist as an operation at all, or be a no-op that compiles cleanly; decide and document which.
- Chaining `and_then` calls where an intermediate stage's error type differs from the previous stage's error type — does your API require a common error type across the whole chain (simpler, more restrictive) or support per-stage error types with some unification step (more flexible, more complex)? Either is acceptable if deliberate and documented.
- Calling the value accessor on an error-state `Result` (or vice versa) — must be a well-defined, documented failure (an assertion, a thrown exception specifically for this programmer-error case, or UB-by-precondition with a clearly stated contract — pick one, consistently).

## Error Handling

This entire project *is* an error-handling exercise, so the "error handling" section here is about the meta-level: your error types themselves should be designed thoughtfully — an `enum class` per failure category, or a small struct carrying a code plus context, are both reasonable; a bare `std::string` message with no structure is explicitly discouraged, since it makes `or_else`-based recovery logic unable to distinguish error kinds programmatically.

## Acceptance Criteria

- `Result<T,E>` and its monadic operations pass a GoogleTest suite covering success/error propagation through `map`/`and_then`/`or_else` chains, including chains where an early stage fails and later stages are demonstrably never invoked.
- `Result<void, E>` compiles and behaves sensibly through the same operations.
- The demonstration pipeline runs at least one success path and one distinct failure path per named error kind, with output distinguishing each.
- The exceptions-vs-Result comparison write-up is included and specific (citing actual code from both versions), not generic.
- Builds cleanly under `/W4 /permissive-`.

## Testing Requirements

- Unit tests for `Result<T,E>` construction, accessor contract enforcement, and each monadic operation in isolation.
- A "later stages never invoked after an early failure" test — verifiable via a counter incremented by each stage's implementation, checked to have stopped incrementing after the failing stage.
- Integration tests running the full demonstration pipeline through each documented error kind plus the success path.

## Hints

### Hint 1 — Direction
Think about what a type needs to safely hold *either* a `T` or an `E` at any given time, without wasting space on both simultaneously and without requiring either to be default-constructible — this is a strong hint toward a specific kind of storage you've likely already built a variant of elsewhere in this workbook's earlier projects, applied here to exactly two possible types instead of an arbitrary set.

### Hint 2 — Technique
For `and_then`, think about its signature: given a `Result<T,E>` and a function `T -> Result<U,E>`, what should the combined operation return, and specifically what should happen to the function call itself when the input `Result` is already in the error state — should the function even be invoked? For `map`, contrast this with a function `T -> U` (not returning a `Result` itself) — why does `map` need to wrap the function's return value in a new `Result` for you, while `and_then` trusts the function to already return one?

### Hint 3 — Implementation
For `Result<void, E>`, consider what changes if you specialize (partially or fully) your primary `Result<T,E>` template for `T = void` — specifically, which member functions stop making sense (an accessor for "the value" when there is no value type to hold) and which continue to make sense unchanged (checking success/failure, accessing the error). Think about whether a full specialization or a more surgical `if constexpr`-based conditional compilation within a single template better fits how much of the implementation genuinely differs for the void case versus how much stays the same.

### Hint 4 — Debugging/Design
If your `and_then` chain doesn't actually short-circuit — i.e., a later stage's code runs even though an earlier stage returned an error — check whether your `and_then` implementation unconditionally calls the provided function and only *afterward* checks whether the original `Result` held an error, rather than checking the error state *first* and skipping the function call entirely when it's already in the error state. This ordering mistake is easy to make when the temptation is to write `and_then` symmetrically to `map`, but the two have a genuinely different control-flow shape.
