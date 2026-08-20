# P-1.1 — Solution

## Reference Architecture

A single class template `Quantity<T, Tag>` holding one `T` member, with:
- an explicit constructor from `T`,
- an explicit conversion operator back to `T`,
- member `operator+`/`operator-`/comparison operators constrained (via a trailing, same-type parameter — no template parameter needed since both operands are already the same concrete `Quantity<T, Tag>`) to same-tag-only, since a same-tag binary operator simply never has an overload accepting a different tag,
- a free `operator*(Quantity<T,Tag>, T)` (and the reverse order) for scalar scaling, distinct in signature from the tag-to-tag path,
- a free `operator/` (and `operator*`) templated over *two* tags, whose return type is looked up through a small `DerivedUnit<Tag1, Op, Tag2>` trait that must be explicitly specialized per meaningful pair — an unspecialized pair fails to compile with a clear "no derived unit defined for this combination" error rather than silently producing something nonsensical.

```cpp
template <typename T, typename Tag>
class Quantity {
    T value_;
public:
    constexpr explicit Quantity(T v) : value_(v) {}
    constexpr explicit operator T() const { return value_; }

    constexpr Quantity operator+(Quantity o) const { return Quantity{value_ + o.value_}; }
    constexpr Quantity operator-(Quantity o) const { return Quantity{value_ - o.value_}; }
    constexpr bool operator==(Quantity o) const { return value_ == o.value_; }
    constexpr auto operator<=>(Quantity o) const { return value_ <=> o.value_; }

    constexpr Quantity operator*(T scalar) const { return Quantity{value_ * scalar}; }
    friend constexpr Quantity operator*(T scalar, Quantity q) { return q * scalar; }
};

template <typename Tag1, typename Tag2> struct DivResultTag; // specialized per meaningful pair
template <typename Tag1, typename Tag2> struct MulResultTag; // specialized per meaningful pair

template <typename T, typename Tag1, typename Tag2>
constexpr auto operator/(Quantity<T, Tag1> a, Quantity<T, Tag2> b) {
    using ResultTag = typename DivResultTag<Tag1, Tag2>::type;
    return Quantity<T, ResultTag>{static_cast<T>(a) / static_cast<T>(b)};
}
```

```cpp
struct MetersTag {}; struct SecondsTag {}; struct MetersPerSecondTag {};
using Meters = Quantity<double, MetersTag>;
using Seconds = Quantity<double, SecondsTag>;
using MetersPerSecond = Quantity<double, MetersPerSecondTag>;

template <> struct DivResultTag<MetersTag, SecondsTag> { using type = MetersPerSecondTag; };
```

## Design Rationale

**Why a trait specialized per tag pair, rather than a generic templated `operator/`?** A fully generic `operator/` that synthesizes some default derived tag for *any* two tags would happily let you divide `Kilograms` by `MetersTag` and get a nonsense unit nobody defined — the whole point of this library is that meaningless combinations should be compile errors, not silently-accepted garbage. Requiring an explicit specialization for each meaningful pair means an *unspecified* combination fails to compile (no matching specialization, and depending on how strict you want the error, you can `static_assert(false, "no derived unit for this pair")` in the unspecialized primary template body for a much clearer diagnostic than the default "incomplete type" error).

**Why explicit constructor and explicit conversion operator, rather than implicit?** Implicit construction from `T` would defeat much of the type safety this project exists to provide — `void setSpeed(MetersPerSecond)` accepting a bare `double` silently would reintroduce exactly the unit-confusion risk the whole exercise is meant to eliminate. Implicit conversion back to `T` has the same problem in the other direction (a `Meters` silently decaying to a `double` in a context expecting `Seconds`-as-double would defeat cross-type protection too, if any function happens to accept a raw `double`).

**Why not use `std::is_convertible_v` constraints via SFINAE/concepts on the arithmetic member operators, rather than just... not writing an overload that accepts other tags?** Because a same-tag operator (`Quantity operator+(Quantity)`) simply never has any overload that could accept a different-tagged `Quantity` in the first place — there's no implicit conversion path from `Quantity<T,OtherTag>` to `Quantity<T,Tag>` (since we deliberately made the conversion-from-`T` constructor explicit and gave it no cross-tag constructor at all), so overload resolution for `a + b` where `a` and `b` have different tags simply finds no viable candidate. No SFINAE is needed — the absence of a cross-tag constructor already does the job.

## Reference Implementation

The core template above is close to complete; the remaining work is:
1. Defining `MulResultTag` similarly to `DivResultTag` for the multiplication direction (if you want both `Meters * Hertz` and `Hertz * Meters` to yield `MetersPerSecond`, you'll want both to explicitly specialize, or a single canonical ordering with a helper).
2. Confirming `sizeof(Quantity<T,Tag>) == sizeof(T)` — true automatically since `Quantity` has exactly one non-static data member and no virtual functions, no base classes, and (for T with no special alignment requirements) no padding.
3. Confirming triviality: since `Quantity<T,Tag>`'s only member is `T` itself and it declares no destructor/copy/move constructors (the compiler-generated ones are trivial when `T`'s are), `std::is_trivially_copyable_v<Quantity<T,Tag>>` should hold whenever it holds for `T`.

## Testing Strategy

Beyond the visible tests: instantiate `Quantity` with at least `int`, `long`, and `double` in a parameterized-or-just-repeated fashion to confirm the template genuinely works generically rather than accidentally depending on `double`-specific behavior (e.g. accidentally using a floating-point literal `0.0` somewhere in a comparison default). For the compile-failure case, actually attempt to build the forbidden snippet in isolation (a separate small translation unit, temporarily) and capture the real compiler diagnostic — confirming the rejection is a real, current compile error rather than something that happened to be true of an earlier draft of your header.

## Performance Analysis

Every operation here should compile down to exactly the underlying `T` operation with zero overhead — there is no indirection, no virtual dispatch, no heap allocation, and (given `constexpr`-qualified operators) many uses can be fully evaluated at compile time. The `DerivedUnit`/`DivResultTag` trait lookups are pure compile-time metaprogramming with zero runtime cost — they exist only to select a return type, never to perform any runtime computation.

## Failure Modes

The only "failure mode" this library has is a compile error — either an intentional one (mismatched tags, unspecialized derived-unit trait) or, if you've made a mistake in your operator constraints, an *unintentional* compile success where a mismatched-tag operation should have failed. There is no runtime failure surface: no allocation to fail, no I/O, no exception path.

## Extensions

- Extend the derived-unit mechanism to support a genuinely dimensional system (tracking exponents of base dimensions like length/time/mass as compile-time integers, so any combination of base units automatically produces the correct derived tag without per-pair specialization) — this is essentially reimplementing a small subset of `std::chrono`'s ratio-based design, or libraries like Boost.Units, and is a substantial jump in sophistication beyond this project's scope but a natural "if you want to go further" direction.
- Add a `Quantity` formatting hook (`std::formatter` specialization) that prints the tag's unit symbol alongside the value, e.g. `"5.2 m/s"` for `MetersPerSecond` — requires associating a display string with each tag, which is a nice small follow-up exercise in tag-to-metadata mapping.
