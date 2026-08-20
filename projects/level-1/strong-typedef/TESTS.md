# P-1.1 — Tests

## Visible Tests (GoogleTest)

These are the tests you are expected to write and pass yourself before considering the project done. They mirror the functional requirements directly.

```cpp
TEST(Quantity, ConstructsFromUnderlyingType) {
    Meters m{5.0};
    EXPECT_DOUBLE_EQ(static_cast<double>(m), 5.0);
}

TEST(Quantity, ConstructionFromRawTIsExplicitNotImplicit) {
    static_assert(!std::is_convertible_v<double, Meters>,
                  "Quantity construction from raw T must be explicit");
}

TEST(Quantity, SameTagAdditionWorks) {
    Meters a{3.0}, b{4.0};
    Meters c = a + b;
    EXPECT_DOUBLE_EQ(static_cast<double>(c), 7.0);
}

TEST(Quantity, SameTagSubtractionWorks) {
    Meters a{10.0}, b{4.0};
    EXPECT_DOUBLE_EQ(static_cast<double>(a - b), 6.0);
}

TEST(Quantity, EqualityAndOrderingWork) {
    Meters a{5.0}, b{5.0}, c{6.0};
    EXPECT_EQ(a, b);
    EXPECT_LT(a, c);
    EXPECT_GT(c, a);
}

TEST(Quantity, CrossTagAdditionDoesNotCompile) {
    // Meters + Seconds must be a compile error.
    // See should_not_compile.md for the isolated, documented case.
    SUCCEED() << "Verified manually per should_not_compile.md — see that file.";
}

TEST(Quantity, ScalarMultiplicationPreservesTag) {
    Meters m{5.0};
    Meters doubled = m * 2.0;
    EXPECT_DOUBLE_EQ(static_cast<double>(doubled), 10.0);
    static_assert(std::is_same_v<decltype(m * 2.0), Meters>);
}

TEST(Quantity, DerivedUnitFromDivision) {
    Meters distance{100.0};
    Seconds time{10.0};
    MetersPerSecond speed = distance / time;
    EXPECT_DOUBLE_EQ(static_cast<double>(speed), 10.0);
}

TEST(Quantity, DerivedUnitTypeIsDistinctFromBothInputs) {
    static_assert(!std::is_same_v<MetersPerSecond, Meters>);
    static_assert(!std::is_same_v<MetersPerSecond, Seconds>);
}

TEST(Quantity, SizeofMatchesUnderlyingType) {
    static_assert(sizeof(Meters) == sizeof(double));
    static_assert(sizeof(Quantity<int, struct SomeTag>) == sizeof(int));
}

TEST(Quantity, UsableInConstexprContext) {
    constexpr Meters m{5.0};
    constexpr Meters n = m + Meters{2.0};
    static_assert(static_cast<double>(n) == 7.0);
}

TEST(Quantity, IntegralUnderlyingTypeWorks) {
    using Widgets = Quantity<int, struct WidgetsTag>;
    Widgets a{3}, b{4};
    EXPECT_EQ(static_cast<int>(a + b), 7);
}

TEST(Quantity, NegativeQuantitiesPreserveArithmeticSemantics) {
    Meters a{-5.0}, b{3.0};
    EXPECT_DOUBLE_EQ(static_cast<double>(a + b), -2.0);
}
```

## Compile-Failure Case (documented, not GoogleTest-executable)

`should_not_compile.md` in your submission should contain, verbatim, a snippet like:

```cpp
Meters m{5.0};
Seconds s{2.0};
auto x = m + s; // must fail to compile: no operator+ accepts (Meters, Seconds)
```

along with the actual compiler error you observed when you uncommented and attempted to build this snippet in isolation, confirming the rejection is a real compile error and not merely untested behavior.

## Hidden Tests

Hidden tests exist and will probe (without further detail — reasoning about these is part of the exercise, per this workbook's testing philosophy):
- behavior when `T` is a non-default-constructible or user-defined arithmetic-like type
- whether your derived-unit mechanism generalizes to a *third* level of derivation (e.g. combining a derived unit with another quantity) or was hardcoded to work only for the one example pair you tested
- whether copy/move of a `Quantity` is trivial (checked via `std::is_trivially_copyable_v`) given the "no heap allocation, `sizeof == sizeof(T)`" constraint
- ordering-comparison behavior specifically for `T = int` at values near `INT_MIN`/`INT_MAX`
