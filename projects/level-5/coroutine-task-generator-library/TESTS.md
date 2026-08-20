# P-5.2 — Tests

## Visible Tests (GoogleTest)

```cpp
task<int> compute_value() { co_return 42; }

TEST(CoroTask, BasicTaskReturnsCorrectValue) {
    auto run = [] () -> task<int> { co_return co_await compute_value(); };
    EXPECT_EQ(sync_wait(run()), 42);
}

TEST(CoroTask, SynchronousCompletionFastPathDoesNotDeadlockOrMisbehave) {
    task<int> t = compute_value(); // may already be complete before anyone awaits it
    EXPECT_EQ(sync_wait(std::move(t)), 42);
}

TEST(CoroTask, ExceptionInsideTaskPropagatesToAwaiter) {
    auto throwing = [] () -> task<int> { throw std::runtime_error("boom"); co_return 0; };
    auto run = [&] () -> task<int> { co_return co_await throwing(); };
    EXPECT_THROW(sync_wait(run()), std::runtime_error);
}

TEST(CoroTask, DeepChainOfAwaitedTasksDoesNotStackOverflow) {
    std::function<task<int>(int)> chain = [&](int depth) -> task<int> {
        if (depth == 0) co_return 0;
        co_return co_await chain(depth - 1) + 1;
    };
    EXPECT_EQ(sync_wait(chain(100000)), 100000); // would stack-overflow without symmetric transfer
}

TEST(CoroTask, UnawaitedTaskDoesNotLeakOrCrashOnDestruction) {
    { task<int> t = compute_value(); } // destroyed without ever being awaited
    SUCCEED();
}

TEST(CoroGenerator, YieldsExpectedSequence) {
    auto gen = [] () -> generator<int> { for (int i = 0; i < 5; ++i) co_yield i; };
    std::vector<int> collected;
    for (auto v : gen()) collected.push_back(v);
    EXPECT_EQ(collected, (std::vector<int>{0, 1, 2, 3, 4}));
}

TEST(CoroGenerator, ExceptionDuringIterationPropagatesAtIncrementPoint) {
    auto gen = [] () -> generator<int> {
        co_yield 1;
        throw std::runtime_error("mid-sequence failure");
        co_yield 2;
    };
    auto g = gen();
    auto it = g.begin();
    EXPECT_EQ(*it, 1);
    EXPECT_THROW(++it, std::runtime_error);
}

TEST(CoroGenerator, EarlyAbandonmentDoesNotLeakOrMisbehave) {
    auto gen = [] () -> generator<int> { for (int i = 0;; ++i) co_yield i; }; // infinite
    int count = 0;
    for (auto v : gen()) { if (++count == 5) break; } // abandoned mid-iteration
    EXPECT_EQ(count, 5);
}

TEST(WhenAll, CombinesResultsInOriginalArgumentOrder) {
    auto slow = [] () -> task<int> { co_await delay(std::chrono::milliseconds(30)); co_return 1; };
    auto fast = [] () -> task<int> { co_await delay(std::chrono::milliseconds(5)); co_return 2; };
    auto run = [&] () -> task<std::tuple<int,int>> {
        co_return co_await when_all(slow(), fast()); // fast completes first, but result order must match args
    };
    auto [a, b] = sync_wait(run());
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
}

TEST(CoroFrameAllocator, FramesAreRoutedThroughCustomAllocatorNotGlobalNew) {
    ArenaAllocator arena(1 << 16);
    InstrumentedGlobalAllocCounter counter;
    auto make = [&arena] () -> task_with_allocator<int> { co_return 7; };
    auto t = make();
    EXPECT_GT(arena.bytes_allocated(), 0u);
    EXPECT_EQ(counter.global_new_calls_for_this_frame(), 0u);
}
```

## Hidden Tests

- a mixed-order `when_all` stress test with many more than two tasks, completing in a randomized order, confirming correct result association every time across many runs
- a nested-generator test (a generator that internally iterates another generator) confirming correct lifetime and exception propagation across the nesting
- a repeated create-and-immediately-destroy-without-awaiting stress test (many thousands of `task<T>` instances) run under ASan, confirming no leaks
- a `task<void>` variant test (specialization/overload for void-returning coroutines) confirming it works identically in spirit to `task<T>`
- a combined test awaiting a `task<T>` whose promise's `final_suspend` awaiter is deliberately checked (via instrumentation) to confirm it returns a coroutine handle for tail-resumption rather than calling `.resume()` directly, verifying symmetric transfer structurally and not just via the depth test's absence-of-crash
