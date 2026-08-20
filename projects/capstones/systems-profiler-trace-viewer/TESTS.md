# C-3 — Tests

## Visible Tests (GoogleTest)

### Phase 1 — Core Correctness

```cpp
TEST(Tracer, SingleThreadNestedScopesRecordCorrectParentChildStructure) {
    Tracer tracer;
    {
        TRACE_SCOPE("outer");
        { TRACE_SCOPE("inner"); }
    }
    auto events = tracer.drain_events();
    EXPECT_EQ(events.size(), 2u);
    EXPECT_TRUE(fully_contains(events[0] /*outer*/, events[1] /*inner*/)); // inner's [begin,end] within outer's
}

TEST(Tracer, MultiThreadEventsAreCorrectlyAttributedToOwningThread) {
    Tracer tracer;
    constexpr int kThreads = 8;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) threads.emplace_back([&, i] {
        TRACE_SCOPE(("thread_task_" + std::to_string(i)).c_str());
    });
    for (auto& t : threads) t.join();
    auto events = tracer.drain_events();
    std::set<ThreadId> distinct_threads;
    for (auto& e : events) distinct_threads.insert(e.thread_id);
    EXPECT_EQ(distinct_threads.size(), static_cast<size_t>(kThreads)); // no cross-thread attribution errors
}

TEST(Tracer, RingBufferWraparoundEvictsOldestWithoutCorruptingRemaining) {
    Tracer tracer(/*capacity_per_thread=*/100);
    for (int i = 0; i < 1000; ++i) { TRACE_SCOPE(("event_" + std::to_string(i)).c_str()); }
    auto events = tracer.drain_events();
    EXPECT_LE(events.size(), 100u);
    for (auto& e : events) EXPECT_TRUE(is_well_formed(e)); // no partially-overwritten/corrupted entries
    EXPECT_EQ(events.back().name, "event_999"); // most recent survives eviction
}

TEST(Tracer, DeeplyNestedScopesAreAllCorrectlyRecorded) {
    Tracer tracer;
    std::function<void(int)> recurse = [&](int depth) {
        if (depth == 0) return;
        TRACE_SCOPE("level");
        recurse(depth - 1);
    };
    recurse(500);
    EXPECT_EQ(tracer.drain_events().size(), 500u);
}
```

### Phase 2 — Sampling and Symbolization

```cpp
TEST(Sampler, SamplingKnownDeepSyntheticCallStackProducesCorrectlyOrderedFrames) {
    // synthetic_deep_call_chain() calls func_a -> func_b -> func_c -> func_d, then blocks
    auto captured_stack = sample_blocked_thread(spawn_synthetic_deep_call_chain());
    auto symbolized = symbolize(captured_stack, current_executable_debug_info());
    EXPECT_THAT(function_names(symbolized), ElementsAre("func_d", "func_c", "func_b", "func_a")); // innermost first
}

TEST(Symbolizer, ResolvesAddressToCorrectSourceFileAndLine) {
    auto symbolized = symbolize({address_of(&known_test_function)}, current_executable_debug_info());
    ASSERT_EQ(symbolized.size(), 1u);
    EXPECT_EQ(symbolized[0].function_name, "known_test_function");
    EXPECT_TRUE(symbolized[0].source_file.ends_with("test_target.cpp"));
}

TEST(Symbolizer, MissingDebugInfoDegradesToRawAddressWithoutCrashing) {
    auto symbolized = symbolize({0xDEADBEEF}, /*debug_info=*/nullptr);
    ASSERT_EQ(symbolized.size(), 1u);
    EXPECT_EQ(symbolized[0].function_name, ""); // explicitly unresolved, not garbage
    EXPECT_EQ(symbolized[0].raw_address, 0xDEADBEEFu);
}
```

### Phase 3 — Overhead and Load Correctness

```cpp
TEST(TracerOverhead, InstrumentedTracingStaysUnderStatedBudgetOnRealisticWorkload) {
    auto baseline = time_workload_without_tracing(realistic_workload);
    auto traced = time_workload_with_tracing(realistic_workload);
    double overhead_pct = 100.0 * (traced - baseline) / baseline;
    EXPECT_LT(overhead_pct, kDocumentedOverheadBudgetPercent);
}

TEST(TracerStress, SustainedHighFrequencyMultiThreadedTracingHasNoCorruption) {
    Tracer tracer;
    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};
    for (int i = 0; i < 16; ++i) threads.emplace_back([&] {
        while (!stop.load()) { TRACE_SCOPE("hot_loop"); }
    });
    std::this_thread::sleep_for(std::chrono::seconds(30));
    stop = true;
    for (auto& t : threads) t.join();
    for (auto& e : tracer.drain_events()) EXPECT_TRUE(is_well_formed(e));
}
```

### Phase 4 — Trace Viewer

```cpp
TEST(TraceViewer, RendersRealMultiThreadedWorkloadTraceWithCorrectThreadLaneCount) {
    auto trace = capture_trace_of(real_multithreaded_workload, /*thread_count=*/4);
    auto rendered = TraceViewer::render_to_html(trace);
    EXPECT_EQ(count_thread_lanes(rendered), 4);
}

TEST(TraceViewer, HotPathAggregationIdentifiesKnownHotFunctionInSyntheticWorkload) {
    auto trace = capture_trace_of(workload_with_one_deliberately_hot_function);
    auto flame_data = TraceViewer::aggregate_hot_paths(trace);
    EXPECT_EQ(flame_data.hottest_function_name(), "deliberately_hot_function");
}
```

## Manual/Script-Driven Verification

- Overhead measurement report against the stated budget (Phase 3), on both Linux and Windows.
- Visual inspection of the rendered trace viewer output (Phase 4) against a real captured trace, confirming it's actually legible and structurally correct, not just programmatically "correct" per the automated assertions above.
- A cross-platform symbolization comparison: the same synthetic call-stack test run on both Linux (DWARF) and Windows (PDB), confirming both correctly resolve to the same function names.

## Hidden Tests

- a signal-safety test (Linux) confirming the sampling mechanism, if signal-based, only calls async-signal-safe functions from within the signal handler itself
- a symbol-cache correctness test confirming repeated symbolization of the same address returns a consistent, correctly-cached result rather than re-parsing debug info every time (a likely Phase 3 overhead contributor if missed)
- a ring-buffer capacity-exhaustion-under-extreme-burst test confirming memory use remains bounded even under a burst far exceeding any realistic sustained rate
- a trace-file round-trip test (if the trace format is persisted to disk): capture, save, reload, and confirm the reloaded trace matches the originally captured one exactly
