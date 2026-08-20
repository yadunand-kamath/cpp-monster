# P-1.4 — Tests

This project's primary deliverable is a report, not a pass/fail unit-test suite — but the harness itself and each demonstration should still be verifiable mechanically. Treat the following as required assertions embedded in your demonstration programs (GoogleTest or plain assertions are both fine).

## Visible Tests / Required Demonstrations

```cpp
TEST(TrackedHarness, CountersDistinguishAllOperations) {
    Tracked::reset_counts();
    Tracked a;                       // default ctor
    Tracked b{42};                   // value ctor
    Tracked c = b;                   // copy ctor
    Tracked d = std::move(b);        // move ctor
    a = c;                           // copy assignment
    a = std::move(d);                // move assignment
    EXPECT_EQ(Tracked::default_ctor_count(), 1);
    EXPECT_EQ(Tracked::value_ctor_count(), 1);
    EXPECT_EQ(Tracked::copy_ctor_count(), 1);
    EXPECT_EQ(Tracked::move_ctor_count(), 1);
    EXPECT_EQ(Tracked::copy_assign_count(), 1);
    EXPECT_EQ(Tracked::move_assign_count(), 1);
}

TEST(TrackedHarness, MandatoryElisionOnPrvalueReturnProducesNoCopyOrMove) {
    Tracked::reset_counts();
    Tracked t = make_prvalue(); // return Tracked{...}; directly
    EXPECT_EQ(Tracked::copy_ctor_count(), 0);
    EXPECT_EQ(Tracked::move_ctor_count(), 0);
    // must hold at every optimization level — this is a C++17 language guarantee, not a compiler choice
}

TEST(TrackedHarness, SingleNamedLocalReturnAllowsNRVO) {
    Tracked::reset_counts();
    Tracked t = make_named_local(); // Tracked local{...}; return local;
    // NOT asserted as == 0, since NRVO is a permitted optimization, not a guarantee —
    // document the actual observed count instead, and note it may legitimately
    // differ between compilers or optimization levels.
    std::cout << "move+copy count for single-named-local return: "
              << (Tracked::copy_ctor_count() + Tracked::move_ctor_count()) << "\n";
}

TEST(TrackedHarness, MultipleNamedLocalsDefeatsNRVO) {
    Tracked::reset_counts();
    Tracked t = make_multi_branch_local(true); // two different named locals, returned from two branches
    EXPECT_GT(Tracked::copy_ctor_count() + Tracked::move_ctor_count(), 0);
    // at least one real copy or move must occur — no implementation can elide this case
}

TEST(TrackedHarness, MoveIfNoexceptCopiesWhenMoveCtorNotNoexcept) {
    std::vector<TrackedThrowingMove> v;
    v.reserve(2);
    v.emplace_back(); v.emplace_back();
    TrackedThrowingMove::reset_counts();
    v.emplace_back(); // forces reallocation past capacity 2
    EXPECT_GT(TrackedThrowingMove::copy_ctor_count(), 0);
}

TEST(TrackedHarness, MoveIfNoexceptMovesWhenMoveCtorIsNoexcept) {
    std::vector<TrackedNoexceptMove> v;
    v.reserve(2);
    v.emplace_back(); v.emplace_back();
    TrackedNoexceptMove::reset_counts();
    v.emplace_back(); // forces reallocation past capacity 2
    EXPECT_GT(TrackedNoexceptMove::move_ctor_count(), 0);
    EXPECT_EQ(TrackedNoexceptMove::copy_ctor_count(), 0);
}
```

## Report Deliverable

A Markdown report presenting, for each of the demonstrations above: the exact counts observed, at each of the two required optimization levels, and a short explanation citing the specific rule (mandatory elision under C++17 [class.copy.elision], NRVO as a permitted-not-required optimization, `move_if_noexcept`'s `noexcept`-move-constructor check) responsible for each result.

## Hidden Tests

- a demonstration involving a function parameter passed by value and returned directly (not a purely local variable) — probing whether the learner correctly identifies that this is *not* eligible for the same elision as a bare local
- counts for a `Tracked` value passed by value into a function and then returned from that same function unmodified — chained elision/move behavior across a call boundary
- whether the learner's report correctly distinguishes "the compiler didn't elide this" from "the language forbids eliding this" — a common conceptual conflation this project exists specifically to correct
