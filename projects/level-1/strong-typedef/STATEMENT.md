# P-1.1 — Strong Typedef & Unit-Safe Quantity Type

**Level:** 1 (Focused component) · **Category:** Libraries · **Requires:** Ch01 · **Est. effort:** S (4-8h)

## Objective

Build a small header-only library that lets a caller define distinct, unit-tagged numeric types — e.g. `Meters`, `Seconds`, `Kilograms` — that each behave like their underlying arithmetic type for the operations that make physical sense, and fail to compile for the operations that don't (adding `Meters` to `Seconds`, for instance). The goal is to make an entire category of unit-confusion bug unrepresentable in the type system rather than caught at runtime or not at all.

## Functional Requirements

1. A template, e.g. `Quantity<T, Tag>`, that wraps an underlying arithmetic type `T` and is distinguished at compile time by a tag type `Tag` (an empty struct used purely as a type-level label — `struct MetersTag {};`, etc.).
2. Two `Quantity<T, Tag>` instantiations with the *same* `Tag` must support: construction from `T`, addition, subtraction, equality and ordering comparisons, and explicit (not implicit) conversion back to `T`.
3. Two `Quantity<T, Tag>` instantiations with *different* `Tag`s must **not** be implicitly interoperable — attempting to add a `Quantity<double, MetersTag>` to a `Quantity<double, SecondsTag>` must fail to compile.
4. Support at least one **derived-unit** construction: multiplying a `Quantity<T, Tag1>` by a `Quantity<T, Tag2>` (or dividing) produces a `Quantity<T, Tag3>` for a distinct, meaningful derived tag (e.g. `Meters / Seconds → MetersPerSecond`). You decide how the derived tag relationship is expressed and enforced — this is part of the design exercise.
5. Support scaling by a raw, untagged scalar (`Quantity<double, MetersTag> * 2.0` must be valid and produce the same tag) — this is a different operation from tag-to-tag multiplication in requirement 4, and must be distinguished from it.
6. Construction from a raw `T` value must be explicit, not implicit — `Meters m = 5.0;` must not compile; `Meters m{5.0};` or `Meters m = Meters{5.0};` must.

## Input

This is a compile-time/library correctness project, not one with a runtime input format. Your deliverable is the header library itself plus a test/demonstration program exercising it. If you provide a small demo `main()`, it may take no arguments or accept simple numeric arguments to construct example quantities — your choice, documented in your `README`/comments.

## Output

N/A in the traditional sense — correctness is demonstrated by what compiles, what fails to compile, and what a test suite (GoogleTest, per this workbook's toolchain) asserts about runtime values and comparisons.

## Constraints

- C++20 (`/std:c++20` on MSVC per this workbook's primary toolchain), header-only.
- No dynamic allocation anywhere in the type — a `Quantity<T, Tag>` must have `sizeof(Quantity<T,Tag>) == sizeof(T)` for any trivial `T`, and must be usable in a `constexpr` context.
- The underlying `T` should be a template parameter, not hardcoded to `double` — your library must work for at least `int`, `long`, and `double` instantiations.
- No use of RTTI, `dynamic_cast`, or virtual dispatch anywhere — this is a compile-time-only mechanism by design.

## Edge Cases

Consider (not an exhaustive list — some cases are deliberately left for you to discover):
- What should happen when a `Quantity` is compared or combined with a *literal* of the underlying type (e.g. `meters + 5.0`) versus another same-tagged `Quantity`?
- Negative quantities, and whether your chosen units make negative values meaningful (they do for some tags, not obviously for others — is this the library's problem to solve, or the caller's?).
- Integer overflow behavior when `T` is an integral type — is it your library's job to guard against this, or does it inherit whatever `T`'s own arithmetic does?

## Error Handling

There is no runtime "error" path in the ordinary sense — the entire point of this project is that invalid *unit* combinations are compile errors, not runtime failures. However:
- Division by a zero-valued `Quantity` (in the derived-unit division case) should have defined, documented behavior — decide and document whether this mirrors `T`'s own division-by-zero behavior (e.g. producing `inf`/`nan` for floating point, being UB for integral types as it would be for raw `T`) or whether your library adds its own guard, and justify the choice.

## Acceptance Criteria

- All functional requirements above are met and demonstrated by a passing GoogleTest suite.
- At least one `static_assert(!std::is_convertible_v<...>)` or an equivalent compile-time check demonstrating that cross-tag operations are actually rejected at compile time, not merely "not tested."
- `sizeof(Quantity<double, MetersTag>) == sizeof(double)` verified by a `static_assert`.
- No heap allocation anywhere in the type, verified by inspection (there should be no `new`/container member to check).
- Builds cleanly under `/W4 /permissive-` (MSVC) with zero warnings.

## Testing Requirements

- Unit tests covering: construction, arithmetic between same-tagged quantities, explicit conversion back to `T`, scalar multiplication, and the derived-unit construction from requirement 4.
- At least one test file that is expected to **fail to compile** — since GoogleTest cannot assert a compile failure at runtime, structure this as a separate, clearly-marked source file (e.g. `should_not_compile.cpp.txt` or guarded behind a `#ifdef COMPILE_FAIL_TEST` never defined in the normal build) with a comment explaining exactly what line should fail and why, so a reviewer (or your future self) can manually confirm the type system is actually enforcing the constraint.

## Hints

### Hint 1 — Direction
The core mechanism you need is a type that is distinguished not by what data it holds, but by an extra, unused type parameter that exists purely to make otherwise-identical instantiations of a template incompatible with each other.

### Hint 2 — Technique
Think about which operators need to be member functions restricted to same-tag operands, and which need to be free functions — and consider what it means for a *conversion* to be explicit versus implicit in terms of which constructor qualifier you use.

### Hint 3 — Implementation
For the derived-unit requirement, you need some way to express "multiplying a `Tag1`-quantity by a `Tag2`-quantity produces a `Tag3`-quantity" at the type level. Consider whether this relationship should be expressed via a trait you specialize per tag-pair, or via some other compile-time lookup mechanism — and consider why a single generic `operator*` between any two arbitrary tags would be the wrong default.

### Hint 4 — Debugging/Design
If your `static_assert(!std::is_convertible_v<...>)` check unexpectedly passes when it shouldn't (i.e. it reports two different-tagged quantities *are* convertible), check whether you've accidentally given `Quantity<T, Tag>` a converting constructor or conversion operator that's more permissive than intended — a template constructor taking `Quantity<T, OtherTag>` with no `Tag == OtherTag` constraint is a common way this leaks through unintentionally.
