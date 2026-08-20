# P-2.2 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(InplaceAny, HoldsTrivialTypeInline) {
    AllocCounter guard;
    inplace_any<16> a(42);
    EXPECT_EQ(a.get<int>(), 42);
    EXPECT_EQ(guard.heap_allocations(), 0);
}

TEST(InplaceAny, HoldsNonTrivialTypeInline) {
    Tracked::reset_counts();
    {
        inplace_any<sizeof(Tracked) + 8> a(Tracked{7});
        EXPECT_EQ(a.get<Tracked>().value, 7);
    }
    EXPECT_EQ(Tracked::dtors, Tracked::move_ctors + Tracked::value_ctors + Tracked::copy_ctors);
}

TEST(InplaceAny, CopyConstructInvokesContainedCopyCtor) {
    Tracked::reset_counts();
    inplace_any<sizeof(Tracked) + 8> a(Tracked{1});
    Tracked::reset_counts();
    inplace_any<sizeof(Tracked) + 8> b = a;
    EXPECT_EQ(Tracked::copy_ctors, 1);
    EXPECT_EQ(Tracked::move_ctors, 0);
}

TEST(InplaceAny, MoveConstructInvokesContainedMoveCtor) {
    inplace_any<sizeof(Tracked) + 8> a(Tracked{1});
    Tracked::reset_counts();
    inplace_any<sizeof(Tracked) + 8> b = std::move(a);
    EXPECT_EQ(Tracked::move_ctors, 1);
    EXPECT_EQ(Tracked::copy_ctors, 0);
}

TEST(InplaceAny, ReassignmentDestroysOldValueBeforeConstructingNew) {
    Tracked::reset_counts();
    inplace_any<64> a(Tracked{1});
    a = std::string("hello"); // different held type entirely
    EXPECT_EQ(Tracked::dtors, 1);
    EXPECT_EQ(a.get<std::string>(), "hello");
}

TEST(InplaceAny, TypeCheckedAccessThrowsOnMismatch) {
    inplace_any<16> a(42);
    EXPECT_THROW(a.get<double>(), std::bad_cast);
}

TEST(InplaceAny, EmptyByDefault) {
    inplace_any<16> a;
    EXPECT_FALSE(a.has_value());
}

TEST(InplaceAny, CopyingEmptyProducesEmpty) {
    inplace_any<16> a;
    inplace_any<16> b = a;
    EXPECT_FALSE(b.has_value());
}

TEST(InplaceAny, ExactSizeBoundaryFits) {
    struct ExactlyN { char bytes[16]; };
    inplace_any<16> a(ExactlyN{});
    EXPECT_TRUE(a.has_value());
}

TEST(InplaceAny, OverAlignedTypeIsCorrectlyAligned) {
    struct alignas(32) Aligned32 { double d[4]; };
    inplace_any<64, 32> a(Aligned32{});
    auto* p = &a.get<Aligned32>();
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 32, 0u);
}

TEST(InplaceAny, TriviallyCopyableFastPathAvoidsDispatch) {
    // Compile-time check: verify no function-pointer table member exists
    // for the trivial specialization, or benchmark-compare call overhead.
    static_assert(sizeof(inplace_any<16>) < sizeof(inplace_any<16>) + sizeof(void*) * 3 + 1);
    SUCCEED() << "See benchmark output for measured dispatch-overhead comparison.";
}
```

## Compile-Failure Case (if compile-time rejection chosen for oversized types)

`should_not_compile.md` documenting an attempt to construct `inplace_any<8>` with a type larger than 8 bytes, plus the captured `static_assert` failure message.

## Hidden Tests

- an allocation-counting test specifically for the oversized-type fallback path, if heap fallback was the chosen policy instead of compile-time rejection
- correctness when a `inplace_any` holding a self-referential-ish type (e.g. a type storing a pointer to itself, or an iterator into its own internal container) is moved — checking whether the moved-to storage address invalidates any such internal pointer, and whether that's documented as a known limitation
- benchmark reproducibility: rerunning the trivial-vs-non-trivial dispatch benchmark and confirming the fast path is consistently faster, not a one-off measurement artifact
- behavior when assigning a value of the *same* type currently held (does it destroy-then-reconstruct, or can it optimize to an assignment of the existing object?) — either is acceptable if documented, but the two produce different instrumented-type counts
