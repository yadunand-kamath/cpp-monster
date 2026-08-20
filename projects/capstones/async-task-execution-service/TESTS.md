# C-2 — Tests

## Visible Tests (GoogleTest)

### Phase 1 — Core Correctness

```cpp
TEST(TaskExecutor, SingleTaskRunsAndReturnsResult) {
    TaskExecutor exec;
    auto result = exec.run_sync([]() -> task<int> { co_return 42; }());
    EXPECT_EQ(result, 42);
}

TEST(TaskExecutor, TaskAwaitingTaskComposesCorrectly) {
    TaskExecutor exec;
    auto inner = [](int x) -> task<int> { co_return x * 2; };
    auto outer = [&]() -> task<int> {
        int a = co_await inner(3);
        int b = co_await inner(a);
        co_return a + b;
    };
    EXPECT_EQ(exec.run_sync(outer()), 18); // 6 + 12
}

TEST(TaskExecutor, AwaitingTaskDoesNotBlockWorkerThread) {
    TaskExecutor exec(/*worker_count=*/1);
    std::atomic<int> other_task_ran{0};
    auto blocker = [&]() -> task<void> {
        co_await exec.suspend_until_signalled(); // suspends without occupying the thread
    };
    auto other = [&]() -> task<void> { other_task_ran = 1; co_return; };
    exec.spawn(blocker());
    exec.run_sync(other()); // must complete even though blocker() is still suspended
    EXPECT_EQ(other_task_ran.load(), 1);
}

TEST(TaskExecutor, ExceptionInAwaitedTaskPropagatesToAwaiter) {
    TaskExecutor exec;
    auto failing = []() -> task<int> { throw std::runtime_error("boom"); co_return 0; };
    auto outer = [&]() -> task<int> { co_return co_await failing(); };
    EXPECT_THROW(exec.run_sync(outer()), std::runtime_error);
}
```

### Phase 2 — Structured Concurrency and Cancellation

```cpp
TEST(StructuredConcurrency, CancellingScopeStopsAllInFlightChildTasks) {
    TaskExecutor exec;
    TaskScope scope(exec);
    std::atomic<int> cleanup_ran{0};
    for (int i = 0; i < 10; ++i) {
        scope.spawn([&]() -> task<void> {
            struct Cleanup { std::atomic<int>* c; ~Cleanup() { c->fetch_add(1); } } cleanup{&cleanup_ran};
            co_await wait_forever_or_until_cancelled(scope.token());
        }());
    }
    scope.cancel();
    scope.join(); // blocks until all children have observed cancellation and unwound
    EXPECT_EQ(cleanup_ran.load(), 10);
}

TEST(StructuredConcurrency, NoChildTaskOutlivesParentScopeDestruction) {
    TaskExecutor exec;
    std::atomic<bool> child_still_running{false};
    {
        TaskScope scope(exec);
        scope.spawn([&]() -> task<void> {
            child_still_running = true;
            co_await wait_forever_or_until_cancelled(scope.token());
            child_still_running = false;
        }());
    } // scope destructor must cancel and join
    EXPECT_FALSE(child_still_running.load());
}

TEST(StructuredConcurrency, CancellationDuringNestedAwaitChainUnwindsAllLevels) {
    TaskExecutor exec;
    TaskScope scope(exec);
    std::vector<int> unwind_order;
    std::mutex m;
    auto level2 = [&](CancellationToken tok) -> task<void> {
        struct R { std::vector<int>* o; std::mutex* m; ~R() { std::lock_guard l(*m); o->push_back(2); } } r{&unwind_order, &m};
        co_await wait_forever_or_until_cancelled(tok);
    };
    auto level1 = [&](CancellationToken tok) -> task<void> {
        struct R { std::vector<int>* o; std::mutex* m; ~R() { std::lock_guard l(*m); o->push_back(1); } } r{&unwind_order, &m};
        co_await level2(tok);
    };
    scope.spawn(level1(scope.token()));
    scope.cancel();
    scope.join();
    EXPECT_THAT(unwind_order, ElementsAre(2, 1)); // innermost unwinds first
}

TEST(StructuredConcurrency, TaskCompletingNormallyBeforeCancelIsUnaffected) {
    TaskExecutor exec;
    TaskScope scope(exec);
    std::atomic<int> result{0};
    scope.spawn([&]() -> task<void> { result = 42; co_return; }());
    scope.join(); // no cancel — normal completion
    EXPECT_EQ(result.load(), 42);
}
```

### Phase 3 — Scheduling, Backpressure, Determinism

```cpp
TEST(Scheduler, MixedCpuAndIoBoundTasksDoNotStarveEachOther) {
    TaskExecutor exec(/*worker_count=*/4);
    std::atomic<int> cpu_completions{0}, io_completions{0};
    for (int i = 0; i < 1000; ++i)
        exec.spawn([&]() -> task<void> { busy_spin_for(1ms); cpu_completions++; co_return; }());
    for (int i = 0; i < 1000; ++i)
        exec.spawn([&]() -> task<void> { co_await exec.io_sleep(1ms); io_completions++; co_return; }());
    exec.run_until_idle();
    EXPECT_EQ(cpu_completions.load(), 1000);
    EXPECT_EQ(io_completions.load(), 1000);
    // fairness: neither type's average completion latency should be many multiples of the other's
}

TEST(Scheduler, BackpressurePolicyRejectsOrBlocksUnderSustainedOverload) {
    TaskExecutor exec(/*worker_count=*/1, /*max_queue_depth=*/100);
    int accepted = 0, rejected = 0;
    for (int i = 0; i < 10000; ++i) {
        auto submit_result = exec.try_spawn(slow_task());
        submit_result ? ++accepted : ++rejected;
    }
    EXPECT_GT(rejected, 0);        // documented policy actually engages under overload
    EXPECT_LE(accepted, 100 + exec.worker_count()); // bounded, per documented policy
}

TEST(DeterministicScheduler, ReproducesSameInterleavingAcrossRepeatedRuns) {
    for (int run = 0; run < 20; ++run) {
        DeterministicTestScheduler sched(/*seed=*/42);
        auto trace = run_scripted_interleaving_scenario(sched);
        static std::vector<int> first_trace;
        if (run == 0) first_trace = trace;
        else EXPECT_EQ(trace, first_trace); // identical seed -> identical interleaving, every time
    }
}
```

## Manual/Script-Driven Verification

- ThreadSanitizer run of the full Phase 3 mixed-workload and structured-concurrency test suites.
- A long-running soak test: continuous mixed CPU/I/O task submission with periodic scope cancellation, watched for orphaned tasks (via the observability surface's in-flight count reaching zero after full drain) or deadlock.
- The Phase 4 performance report with methodology, target, result, and before/after optimization evidence.

## Hidden Tests

- a cancellation-race test: cancelling a scope at the exact moment a child task is transitioning from "suspended awaiting I/O" to "about to resume on a worker thread," confirming no double-resume, no missed-cancellation, and no crash regardless of which side wins the race
- a task-spawns-task-which-spawns-task (three levels) cancellation propagation test, confirming the "no orphaned task" guarantee holds transitively, not just for direct children
- a graceful-shutdown-during-active-backpressure test: shutdown must not silently drop tasks already accepted into the queue
- a fairness test under many more task-type categories than just CPU/I/O, confirming no single task type dominates the worker pool under a skewed submission pattern
- a panic/unexpected-throw-outside-normal-error-channel test (e.g. a task's coroutine frame allocation itself throwing `bad_alloc`), confirming the scheduler's own bookkeeping remains consistent rather than corrupting scheduler state
