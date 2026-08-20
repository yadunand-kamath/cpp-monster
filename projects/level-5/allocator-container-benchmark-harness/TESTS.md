# P-5.1 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(BenchmarkHarness, RegistersAndRunsNamedBenchmark) {
    BenchmarkRegistry registry;
    registry.register_benchmark("trivial", [] { volatile int x = 1 + 1; (void)x; });
    auto results = registry.run_all(RunConfig{.iterations = 100, .warmup_iterations = 10});
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].name, "trivial");
}

TEST(BenchmarkHarness, ReportsCorrectRelativeCostBetweenTwoSyntheticWorkloads) {
    BenchmarkRegistry registry;
    registry.register_benchmark("sleep_1x", [] { std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
    registry.register_benchmark("sleep_2x", [] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
    auto results = registry.run_all(RunConfig{.iterations = 20, .warmup_iterations = 2});
    double ratio = results[1].mean_ns / results[0].mean_ns;
    EXPECT_NEAR(ratio, 2.0, 0.5); // approximate — real sleep has scheduler slack
}

TEST(BenchmarkHarness, ReportsSpreadNotJustMean) {
    BenchmarkRegistry registry;
    registry.register_benchmark("variable", [] {
        static thread_local int i = 0;
        if (++i % 2 == 0) std::this_thread::sleep_for(std::chrono::microseconds(500));
    });
    auto results = registry.run_all(RunConfig{.iterations = 50, .warmup_iterations = 5});
    EXPECT_GT(results[0].stddev_ns, 0.0);
    EXPECT_GT(results[0].max_ns, results[0].min_ns);
}

TEST(BenchmarkHarness, OptimizerDefeatDemonstrationShowsExpectedContrast) {
    auto without_defeat = measure_trivial_computation(/*use_defeat=*/false);
    auto with_defeat = measure_trivial_computation(/*use_defeat=*/true);
    EXPECT_LT(without_defeat.mean_ns, 1.0); // optimized away to ~nothing
    EXPECT_GT(with_defeat.mean_ns, 1.0);    // actually performed
}

TEST(BenchmarkHarness, ExceptionInWorkloadIsReportedAsFailureNotCrash) {
    BenchmarkRegistry registry;
    registry.register_benchmark("throws", [] { throw std::runtime_error("boom"); });
    auto results = registry.run_all(RunConfig{.iterations = 10, .warmup_iterations = 1});
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].failed);
}

TEST(RegressionGate, DetectsIntentionallyIntroducedRegression) {
    auto baseline = run_benchmark_suite(/*with_extra_allocation=*/false);
    auto current = run_benchmark_suite(/*with_extra_allocation=*/true); // deliberately slower
    auto verdict = compare_against_baseline(baseline, current, /*threshold_pct=*/10.0);
    EXPECT_FALSE(verdict.passed);
    EXPECT_FALSE(verdict.regressed_benchmarks.empty());
}

TEST(RegressionGate, UnchangedRerunReportsNoRegression) {
    auto baseline = run_benchmark_suite(false);
    auto rerun = run_benchmark_suite(false);
    auto verdict = compare_against_baseline(baseline, rerun, 10.0);
    EXPECT_TRUE(verdict.passed);
}

TEST(RegressionGate, MalformedBaselineFileIsClearError) {
    write_file("bad_baseline.json", "{ not valid json");
    EXPECT_THROW(load_baseline("bad_baseline.json"), BaselineParseError);
}
```

## Manual/Script-Driven Verification

- Coverage benchmark run against [P-4.4](../../level-4/arena-pool-allocators/STATEMENT.md)'s arena/pool allocators versus the default allocator, on the target workload shape each was designed for, with captured numbers.
- Coverage benchmark of at least one container operation (e.g. `std::vector::push_back` with and without `reserve`) across varying input sizes.

## Hidden Tests

- a JSON/CSV machine-readable output format round-trip test (write results, parse them back, confirm values match)
- a timer-resolution-warning test using an artificially tiny synthetic workload, confirming the harness flags low measurement confidence rather than reporting a misleadingly precise number
- a batches-vs-single-giant-run comparison confirming the batching technique (Hint 3) produces comparable mean results to a naive approach while additionally surfacing spread
- a cross-toolchain sanity run (same benchmark suite compiled and run under both MSVC and WSL-GCC/Clang) confirming the harness itself runs correctly on both, independent of the (expected-to-differ) absolute numbers
- a regression-gate run using a baseline file captured under different iteration-count settings than the current run, confirming the harness either normalizes correctly or produces a clear, documented caveat rather than a silently misleading comparison
