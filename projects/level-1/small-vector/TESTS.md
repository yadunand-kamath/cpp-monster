# P-1.3 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(SmallVector, DefaultConstructedIsEmpty) {
    small_vector<int, 4> v;
    EXPECT_EQ(v.size(), 0u);
}

TEST(SmallVector, PushBackBelowCapacityNeverAllocates) {
    AllocCounter guard;
    small_vector<int, 4> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    EXPECT_EQ(guard.heap_allocations(), 0);
    EXPECT_EQ(v.size(), 3u);
}

TEST(SmallVector, ExactlyNElementsStaysInline) {
    AllocCounter guard;
    small_vector<int, 4> v;
    for (int i = 0; i < 4; ++i) v.push_back(i);
    EXPECT_EQ(guard.heap_allocations(), 0);
}

TEST(SmallVector, ExceedingNTransitionsToHeapExactlyOnce) {
    AllocCounter guard;
    small_vector<int, 4> v;
    for (int i = 0; i < 4; ++i) v.push_back(i);
    guard.reset();
    v.push_back(4); // triggers the transition
    EXPECT_EQ(guard.heap_allocations(), 1);
}

TEST(SmallVector, GrowthPastHeapCapacityIsAmortized) {
    AllocCounter guard;
    small_vector<int, 2> v;
    for (int i = 0; i < 2; ++i) v.push_back(i);
    guard.reset();
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    EXPECT_LT(guard.heap_allocations(), 20); // doubling growth, not O(n) allocations
}

TEST(SmallVector, ElementsPreserveValueAcrossTransition) {
    small_vector<int, 2> v;
    v.push_back(10); v.push_back(20); v.push_back(30);
    EXPECT_EQ(v[0], 10);
    EXPECT_EQ(v[1], 20);
    EXPECT_EQ(v[2], 30);
}

TEST(SmallVector, WorksWithMoveOnlyElementType) {
    small_vector<std::unique_ptr<int>, 2> v;
    v.push_back(std::make_unique<int>(42));
    v.push_back(std::make_unique<int>(43));
    v.push_back(std::make_unique<int>(44)); // triggers transition; must move, not copy
    EXPECT_EQ(*v[0], 42);
    EXPECT_EQ(*v[2], 44);
}

TEST(SmallVector, PopBackDestroysElement) {
    small_vector<int, 4> v;
    v.push_back(1); v.push_back(2);
    v.pop_back();
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 1);
}

TEST(SmallVector, ClearDestroysAllElements) {
    small_vector<int, 4> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    v.clear();
    EXPECT_EQ(v.size(), 0u);
}

TEST(SmallVector, SelfAssignmentIsSafe) {
    small_vector<int, 4> v;
    v.push_back(1); v.push_back(2);
    v = v;
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
}

TEST(SmallVector, CopyConstructorDuplicatesElements) {
    small_vector<int, 4> a;
    a.push_back(1); a.push_back(2);
    small_vector<int, 4> b = a;
    b.push_back(99);
    EXPECT_EQ(a.size(), 2u); // independent storage
    EXPECT_EQ(b.size(), 3u);
}

TEST(SmallVector, AtThrowsOnOutOfRange) {
    small_vector<int, 4> v;
    v.push_back(1);
    EXPECT_THROW(v.at(5), std::out_of_range);
}

TEST(SmallVector, IteratesInOrder) {
    small_vector<int, 4> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    std::vector<int> collected(v.begin(), v.end());
    EXPECT_EQ(collected, (std::vector<int>{1, 2, 3}));
}

TEST(SmallVector, ThrowingMoveConstructorPreservesStrongGuarantee) {
    // ThrowsOnNthConstruction is a test-only helper type that throws
    // from its move constructor on a configured call count.
    small_vector<ThrowsOnNthConstruction, 2> v;
    v.push_back(ThrowsOnNthConstruction{1});
    v.push_back(ThrowsOnNthConstruction{2});
    ThrowsOnNthConstruction::throw_on_call(3); // next move throws
    EXPECT_THROW(v.push_back(ThrowsOnNthConstruction{3}), std::runtime_error);
    // Per the documented guarantee: v must still report its original 2 elements,
    // unmodified, if the guarantee is "strong."
    EXPECT_EQ(v.size(), 2u);
}
```

## Hidden Tests

- allocation count under a sequence that grows past `N`, shrinks below `N` via repeated `pop_back`, then grows again — checked against whatever inline/heap transition-back policy you documented
- `sizeof(small_vector<T,N>)` compared against `N * sizeof(T)` plus a reasonable fixed overhead, for a couple of different `T` and `N` combinations, to catch unintended padding or an oversized bookkeeping struct
- correctness when `T`'s copy constructor is expensive but its move constructor is deleted (forcing copy-based growth) versus when move is available and `noexcept`
- behavior at `N == 0`
- exact iterator invalidation behavior across a transition, checked via debug-build iterator-debugging hooks if you provide any (optional, but if you claim invalidation detection, it will be tested)
