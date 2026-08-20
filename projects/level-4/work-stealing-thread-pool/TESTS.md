# P-4.3 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(ThreadPool, SubmitReturnsCorrectResult) {
    ThreadPool pool(4);
    auto fut = pool.submit([] { return 21 * 2; });
    EXPECT_EQ(fut.get(), 42);
}

TEST(ThreadPool, VoidReturningTaskCompletes) {
    ThreadPool pool(4);
    std::atomic<bool> ran = false;
    auto fut = pool.submit([&] { ran = true; });
    fut.get();
    EXPECT_TRUE(ran.load());
}

TEST(ThreadPool, ThrownExceptionIsCapturedAndRethrown) {
    ThreadPool pool(4);
    auto fut = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    EXPECT_THROW(fut.get(), std::runtime_error);
}

TEST(ThreadPool, StopTokenAllowsCooperativeCancellation) {
    ThreadPool pool(2);
    std::atomic<bool> exited_early = false;
    auto fut = pool.submit([&](std::stop_token tok) {
        while (!tok.stop_requested()) std::this_thread::sleep_for(1ms);
        exited_early = true;
    });
    std::this_thread::sleep_for(20ms);
    pool.request_stop();
    fut.wait();
    EXPECT_TRUE(exited_early.load());
}

TEST(ThreadPool, RecursiveSubtaskSpawningCompletesWithoutDeadlock) {
    ThreadPool pool(4);
    std::function<int(int)> par_fib = [&](int n) -> int {
        if (n < 2) return n;
        auto left = pool.submit([&, n] { return par_fib(n - 1); });
        auto right = pool.submit([&, n] { return par_fib(n - 2); });
        return left.get() + right.get();
    };
    auto fut = pool.submit([&] { return par_fib(15); });
    EXPECT_EQ(fut.get(), 610);
}

TEST(ThreadPool, WorkIsRedistributedAcrossWorkersUnderUnbalancedSubmission) {
    ThreadPool pool(4);
    std::vector<std::atomic<int>> per_worker_count(4);
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 4000; ++i)
        futures.push_back(pool.submit([&] {
            per_worker_count[pool.current_worker_index()]++;
            std::this_thread::sleep_for(1ms);
        }));
    for (auto& f : futures) f.get();
    int min_count = *std::min_element(per_worker_count.begin(), per_worker_count.end());
    EXPECT_GT(min_count, 0); // every worker did real work, not just the submitter's own
}

TEST(ThreadPool, SubmitAfterShutdownIsRejected) {
    ThreadPool pool(2);
    pool.shutdown(/*drain_pending=*/true);
    EXPECT_THROW(pool.submit([] {}), std::runtime_error);
}

TEST(ThreadPool, DrainingShutdownCompletesAlreadyQueuedTasks) {
    ThreadPool pool(2);
    std::atomic<int> completed = 0;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) futures.push_back(pool.submit([&] { completed++; }));
    pool.shutdown(/*drain_pending=*/true);
    EXPECT_EQ(completed.load(), 100);
}

TEST(ThreadPool, IdlePoolDoesNotBusyPoll) {
    ThreadPool pool(4);
    auto cpu_before = read_process_cpu_time();
    std::this_thread::sleep_for(200ms);
    auto cpu_used = read_process_cpu_time() - cpu_before;
    EXPECT_LT(cpu_used, 40ms); // 4 idle workers over 200ms wall time
}
```

## Hidden Tests

- a ThreadSanitizer-instrumented stress test specifically targeting the steal-vs-owner-pop race on a deque with exactly one remaining task, run at high iteration count
- an immediate-shutdown-mode test confirming queued-but-not-yet-started tasks are abandoned rather than run, contrasted explicitly against the draining-mode test
- a "new submission during active drain" test confirming the documented cutover point (submissions rejected once shutdown begins, regardless of in-flight draining)
- a large-scale parallel divide-and-conquer benchmark (e.g. parallel merge sort over a large array) used as the redistribution/correctness demonstration at greater scale than the unit test
- a worker-count-of-1 configuration, confirming the pool still functions correctly (degenerating to no actual stealing, but no crash or deadlock either)
