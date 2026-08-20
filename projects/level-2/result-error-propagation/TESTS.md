# P-2.4 — Tests

## Visible Tests (GoogleTest)

```cpp
enum class ParseError { MalformedInput, UnexpectedEof };
enum class ValidationError { MissingField, OutOfRange };

TEST(Result, HoldsSuccessValue) {
    Result<int, ParseError> r = 42;
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, HoldsErrorValue) {
    Result<int, ParseError> r = ParseError::MalformedInput;
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), ParseError::MalformedInput);
}

TEST(Result, MapTransformsSuccessValue) {
    Result<int, ParseError> r = 5;
    auto mapped = r.map([](int x) { return x * 2; });
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(mapped.value(), 10);
}

TEST(Result, MapPassesThroughError) {
    Result<int, ParseError> r = ParseError::UnexpectedEof;
    auto mapped = r.map([](int x) { return x * 2; });
    ASSERT_FALSE(mapped.has_value());
    EXPECT_EQ(mapped.error(), ParseError::UnexpectedEof);
}

TEST(Result, AndThenChainsOnSuccess) {
    Result<int, ParseError> r = 5;
    auto chained = r.and_then([](int x) -> Result<int, ParseError> {
        return x + 1;
    });
    ASSERT_TRUE(chained.has_value());
    EXPECT_EQ(chained.value(), 6);
}

TEST(Result, AndThenShortCircuitsOnError) {
    int calls = 0;
    Result<int, ParseError> r = ParseError::MalformedInput;
    auto chained = r.and_then([&](int x) -> Result<int, ParseError> {
        ++calls;
        return x + 1;
    });
    EXPECT_EQ(calls, 0); // must never be invoked
    ASSERT_FALSE(chained.has_value());
}

TEST(Result, OrElseRecoversFromError) {
    Result<int, ParseError> r = ParseError::MalformedInput;
    auto recovered = r.or_else([](ParseError) -> Result<int, ParseError> {
        return 0;
    });
    ASSERT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered.value(), 0);
}

TEST(Result, VoidSuccessCompilesAndBehaves) {
    Result<void, ParseError> r = Result<void, ParseError>::success();
    EXPECT_TRUE(r.has_value());
    Result<void, ParseError> e = ParseError::MalformedInput;
    EXPECT_FALSE(e.has_value());
}

TEST(Pipeline, FullSuccessPathReachesPersist) {
    auto result = run_pipeline("valid: key=value");
    ASSERT_TRUE(result.has_value());
}

TEST(Pipeline, ParseFailureShortCircuitsBeforeValidate) {
    int validate_calls = 0, transform_calls = 0;
    auto result = run_pipeline_instrumented("!!!malformed!!!", validate_calls, transform_calls);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(validate_calls, 0);
    EXPECT_EQ(transform_calls, 0);
}

TEST(Pipeline, ValidationFailureShortCircuitsBeforeTransform) {
    int transform_calls = 0;
    auto result = run_pipeline_instrumented_partial("key_missing_required_field", transform_calls);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(transform_calls, 0);
}

TEST(Pipeline, EachErrorKindIsDistinguishable) {
    auto e1 = run_pipeline("!!!malformed!!!");
    auto e2 = run_pipeline("valid_but_missing_field");
    ASSERT_FALSE(e1.has_value());
    ASSERT_FALSE(e2.has_value());
    EXPECT_NE(std::string(describe(e1.error())), std::string(describe(e2.error())));
}
```

## Hidden Tests

- calling `.value()` on an error-state `Result` (or `.error()` on a success-state one) — checked against whatever documented contract-violation behavior was chosen
- an `and_then` chain mixing per-stage error types with whatever unification approach was documented (a single common error type across the pipeline, or an explicit conversion step) — verified for correctness under the documented approach
- a persistence-stage failure specifically (the 4th named error kind), confirmed reachable and distinguishable from the earlier three
- the exceptions-vs-Result write-up is checked for specificity — a generic, uncited comparison does not satisfy the Acceptance Criteria's "specific, not generic" requirement
