# P-5.1 — Solution

## Reference Architecture

The measurement core, deliberately unaware of reporting or comparison (Hint 1):

```cpp
template <typename T>
void DoNotOptimize(T&& value) {
    static volatile std::remove_reference_t<T> sink;
    sink = value; // forces the value through something the optimizer can't prove is unobserved
    std::atomic_signal_fence(std::memory_order_acq_rel); // compiler barrier — Hint 2
}

struct BatchResult { double mean_ns, stddev_ns, min_ns, max_ns; bool failed; std::string failure_reason; };

BatchResult run_benchmark(std::function<void()> workload, RunConfig config) {
    for (int i = 0; i < config.warmup_iterations; ++i) workload(); // discarded entirely
    std::vector<double> batch_means_ns;
    try {
        for (int b = 0; b < config.batch_count; ++b) {
            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < config.iterations_per_batch; ++i) workload();
            auto elapsed = std::chrono::steady_clock::now() - start;
            batch_means_ns.push_back(
                std::chrono::duration<double, std::nano>(elapsed).count() / config.iterations_per_batch);
        }
    } catch (const std::exception& e) {
        return {.failed = true, .failure_reason = e.what()};
    }
    return compute_statistics(batch_means_ns); // mean/stddev/min/max across batch means, per Hint 3
}
```

The regression-gate comparison, showing the spread-aware check from Hint 4:

```cpp
struct GateVerdict { bool passed; std::vector<std::string> regressed_benchmarks; };

GateVerdict compare_against_baseline(const BenchmarkSuite& baseline, const BenchmarkSuite& current, double threshold_pct) {
    GateVerdict verdict{.passed = true};
    for (auto& [name, current_result] : current.results) {
        auto it = baseline.results.find(name);
        if (it == baseline.results.end()) continue;
        double pct_change = (current_result.mean_ns - it->second.mean_ns) / it->second.mean_ns * 100.0;
        double combined_spread_pct = (current_result.stddev_ns + it->second.stddev_ns) / it->second.mean_ns * 100.0;
        if (pct_change > threshold_pct && pct_change > combined_spread_pct * 2.0) { // noise-aware, not a flat threshold
            verdict.passed = false;
            verdict.regressed_benchmarks.push_back(name);
        }
    }
    return verdict;
}
```

## Design Rationale

**Why does `DoNotOptimize` write through a `volatile` static rather than, say, calling an external non-inlinable function?** Both are valid portable-ish techniques; the `volatile`-write-plus-barrier approach avoids introducing real function-call overhead into the measurement (which an external function call would add, distorting the very timing being measured), while still being opaque enough that no mainstream compiler at reasonable optimization levels will prove the write is dead and eliminate the computation feeding it.

**Why compute statistics across per-batch means rather than across every raw iteration's individual timing?** A single iteration of a fast workload can be shorter than the timer's own resolution, making individual-iteration timings dominated by measurement artifact rather than real signal. Batching amortizes that resolution cost across many iterations per timing call, while still taking multiple batches (rather than one giant batch) to preserve visibility into real run-to-run variance — the exact tension Hint 3 and the Edge Cases section both point at.

**Why does the regression-gate check compare the observed change against combined spread, not just a flat percentage threshold?** A flat threshold treats a benchmark's own measured noise as if it were zero, which means normal noise on a noisy benchmark gets flagged as a false regression far more often than on a quiet one. Requiring the observed change to exceed both the configured threshold *and* a multiple of the measured spread makes the gate's false-positive rate depend on the benchmark's own actual stability, which is what a caller intuitively wants from "did this really get slower."

## Reference Implementation

The above covers the measurement core's shape, the optimizer-defeat helper, and the regression-gate's noise-aware comparison. Remaining work for the learner: the `BenchmarkRegistry` registration/lookup machinery, JSON/CSV serialization for machine-readable output, the timer-resolution warning check (comparing a batch's total elapsed time against the platform's reported clock resolution), and wiring the coverage benchmarks against [P-4.4](../../level-4/arena-pool-allocators/STATEMENT.md)'s allocators and a container operation.

## Testing Strategy

Validate the harness's own statistics logic using synthetic workloads with a *known* relative cost (e.g. one workload that sleeps for a fixed duration twice as long as another) before trusting it on any real allocator/container benchmark — if the harness can't correctly report a 2x ratio between two workloads whose actual cost ratio is known in advance, no benchmark result it produces for an unknown workload should be trusted either.

## Performance Analysis

The harness's own overhead (loop bookkeeping, the `DoNotOptimize` write/barrier) should be small relative to the workloads it measures — verify this by benchmarking an empty workload and confirming its reported time is near the timer's resolution floor rather than dominated by harness overhead, since harness overhead bleeding into every measurement would systematically bias every benchmark result upward by a roughly constant amount.

## Failure Modes

- An optimizer-defeat helper that only handles one side (input or output) of the computation, still allowing the optimizer to eliminate the other half's dependent work in cases the harness's own demonstration test doesn't happen to exercise.
- Reporting only a single mean number in the machine-readable output while including spread in the human-readable table, silently degrading the regression-gate's ability to make the noise-aware comparison this project's design deliberately requires.
- A regression-gate comparison that silently proceeds when baseline and current runs used different iteration/warm-up configurations, producing a comparison that looks precise but isn't actually meaningful — this should be caught and flagged, not silently computed.

## Extensions

- A continuous historical trend view (not just baseline-vs-current) plotting a benchmark's measured performance across many CI runs over time.
- Automatic outlier-batch discarding (e.g. dropping batches whose timing is a large multiple of the median batch, likely reflecting external scheduler interference) as a documented, opt-in noise-reduction mode.
