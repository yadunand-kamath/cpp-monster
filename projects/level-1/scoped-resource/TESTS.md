# P-1.2 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(ScopedFile, ClosesOnDestruction) {
    int close_count = 0;
    {
        scoped_file f(open_test_file(), [&]{ ++close_count; });
    }
    EXPECT_EQ(close_count, 1);
}

TEST(ScopedFile, ReleaseDisarmsWrapper) {
    int close_count = 0;
    FILE* raw;
    {
        scoped_file f(open_test_file(), [&]{ ++close_count; });
        raw = f.release();
    }
    EXPECT_EQ(close_count, 0);
    fclose(raw); // caller now owns it
}

TEST(ScopedFile, MoveConstructionTransfersOwnership) {
    int close_count = 0;
    {
        scoped_file a(open_test_file(), [&]{ ++close_count; });
        scoped_file b(std::move(a));
        EXPECT_EQ(close_count, 0); // a's move must not have triggered cleanup
    }
    EXPECT_EQ(close_count, 1); // b's destruction triggers it exactly once
}

TEST(ScopedFile, MoveAssignmentClosesPreviousTargetResource) {
    int close_a = 0, close_b = 0;
    {
        scoped_file a(open_test_file(), [&]{ ++close_a; });
        scoped_file b(open_test_file(), [&]{ ++close_b; });
        b = std::move(a);
        EXPECT_EQ(close_b, 1); // b's original resource closed during the assignment
        EXPECT_EQ(close_a, 0);
    }
    EXPECT_EQ(close_a, 1); // a's resource (now owned by b) closed once at scope end
}

TEST(ScopedFile, SelfMoveAssignmentIsSafe) {
    int close_count = 0;
    scoped_file a(open_test_file(), [&]{ ++close_count; });
    a = std::move(a);
    // must not crash, must not double-close
}

TEST(ScopedFile, IsNotCopyable) {
    static_assert(!std::is_copy_constructible_v<scoped_file>);
    static_assert(!std::is_copy_assignable_v<scoped_file>);
}

TEST(ScopedTimer, ReportsElapsedOnDestruction) {
    std::optional<std::chrono::nanoseconds> reported;
    {
        scoped_timer t([&](auto d) { reported = d; });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(reported.has_value());
    EXPECT_GE(*reported, std::chrono::milliseconds(4));
}

TEST(ScopeExit, RunsOnNormalScopeExit) {
    int calls = 0;
    { scope_exit guard([&]{ ++calls; }); }
    EXPECT_EQ(calls, 1);
}

TEST(ScopeExit, DismissCancelsTheAction) {
    int calls = 0;
    { scope_exit guard([&]{ ++calls; }); guard.dismiss(); }
    EXPECT_EQ(calls, 0);
}

TEST(ScopeExit, RunsExactlyOnceDuringExceptionUnwinding) {
    int calls = 0;
    EXPECT_THROW({
        scope_exit guard([&]{ ++calls; });
        throw std::runtime_error("boom");
    }, std::runtime_error);
    EXPECT_EQ(calls, 1);
}

TEST(ScopeExit, MoveTransfersResponsibility) {
    int calls = 0;
    {
        scope_exit a([&]{ ++calls; });
        scope_exit b(std::move(a));
        EXPECT_EQ(calls, 0);
    }
    EXPECT_EQ(calls, 1); // exactly one call total, from b
}

TEST(ScopeExit, WorksWithFunctionPointerAndStdFunction) {
    static int fn_calls = 0;
    void (*fp)() = []{ ++fn_calls; };
    { scope_exit guard(fp); }
    EXPECT_EQ(fn_calls, 1);

    int calls = 0;
    std::function<void()> f = [&]{ ++calls; };
    { scope_exit guard(f); }
    EXPECT_EQ(calls, 1);
}
```

## Hidden Tests

Hidden tests probe (reasoning about these is part of the exercise):
- whether `scoped_file`'s destructor correctly avoids invoking cleanup on a default-constructed or already-`release()`d instance (no crash, no spurious callback call)
- ASan-detected double-free under a deliberately adversarial sequence of move-assignments and self-move-assignments
- `scope_exit`'s behavior when the guarded action itself throws during unwinding that was already in progress from a different exception (a nested-exception-during-unwinding scenario) — does your destructor's handling of this avoid `std::terminate` where the design intends it to, and is that intent actually documented?
- `scoped_timer`'s reported duration under a callback that itself takes a nontrivial amount of time — does the measured duration reported reflect the guarded scope's work, or does it get contaminated by the callback's own execution time?
