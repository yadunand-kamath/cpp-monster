# P-2.3 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(FunctionRef, CallsThroughLambda) {
    int captured = 10;
    auto lambda = [&](int x) { return x + captured; };
    function_ref<int(int)> ref = lambda;
    EXPECT_EQ(ref(5), 15);
}

TEST(FunctionRef, CallsThroughFunctionPointer) {
    function_ref<int(int, int)> ref = +[](int a, int b) { return a * b; };
    EXPECT_EQ(ref(3, 4), 12);
}

TEST(FunctionRef, IsTriviallyCopyableAndMinimalSize) {
    static_assert(sizeof(function_ref<void()>) == 2 * sizeof(void*));
    static_assert(std::is_trivially_copyable_v<function_ref<void()>>);
}

TEST(FunctionRef, VoidReturnSignatureWorks) {
    int calls = 0;
    auto lambda = [&] { ++calls; };
    function_ref<void()> ref = lambda;
    ref();
    EXPECT_EQ(calls, 1);
}

TEST(FunctionRef, ReferenceParameterForwardsCorrectly) {
    auto lambda = [](int& x) { x *= 2; };
    function_ref<void(int&)> ref = lambda;
    int v = 5;
    ref(v);
    EXPECT_EQ(v, 10);
}

TEST(UniqueFunction, SmallCaptureNeverAllocates) {
    AllocCounter guard;
    int x = 5;
    unique_function<int()> f = [x] { return x * 2; };
    EXPECT_EQ(f(), 10);
    EXPECT_EQ(guard.heap_allocations(), 0);
}

TEST(UniqueFunction, OversizedCaptureAllocatesExactlyOnce) {
    AllocCounter guard;
    std::array<int, 64> big{};
    unique_function<int()> f = [big] { return big[0]; };
    guard.reset();
    unique_function<int()> g = std::move(f); // move should not re-allocate
    EXPECT_EQ(guard.heap_allocations(), 0);
}

TEST(UniqueFunction, WrapsMoveOnlyCapturedState) {
    auto ptr = std::make_unique<int>(42);
    unique_function<int()> f = [p = std::move(ptr)] { return *p; };
    EXPECT_EQ(f(), 42);
}

TEST(UniqueFunction, MoveTransfersOwnershipNotCopy) {
    Tracked::reset_counts();
    unique_function<int()> f = [t = Tracked{1}] { return t.value; };
    Tracked::reset_counts();
    unique_function<int()> g = std::move(f);
    EXPECT_EQ(Tracked::copy_ctors, 0);
    EXPECT_EQ(g(), 1);
}

TEST(UniqueFunction, IsNotCopyable) {
    static_assert(!std::is_copy_constructible_v<unique_function<void()>>);
}

TEST(UniqueFunction, VoidReturnAndReferenceParamsWork) {
    int calls = 0;
    unique_function<void(int&)> f = [&](int& x) { ++calls; x += 1; };
    int v = 0;
    f(v);
    EXPECT_EQ(v, 1);
    EXPECT_EQ(calls, 1);
}
```

## Compile-Failure Case

`should_not_compile.md` demonstrating that `std::function<int()> sf = [p = std::make_unique<int>(1)] { return *p; };` fails to compile (or compiles but is unusable, depending on your standard library — document the actual observed error), while the equivalent `unique_function` construction succeeds — this is the concrete demonstration required by the project statement.

## Hidden Tests

- calling an empty (default-constructed or moved-from) `unique_function` — behavior must match whatever policy your submission documents
- `function_ref` constructed from a stateless (capture-less) lambda vs. a capturing one — both must work identically through the same interface
- a `unique_function` whose capture size lands exactly at the documented SBO threshold boundary
- confirming `unique_function`'s heap-fallback path correctly frees its heap allocation on destruction (no leak, checked via an allocation/deallocation balance count, not just a "does it crash" check)
