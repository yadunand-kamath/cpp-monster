# P-5.1 — Allocator & Container Benchmark Harness

**Level:** 5 (Production-style) · **Category:** Performance · **Requires:** Ch01–12 · **Est. effort:** L (16-24h)

## Objective

Build a statistically sound benchmarking harness — not a single-shot timer wrapper — that measures allocator and container operation throughput/latency with enough rigor to serve as a CI regression gate: it must reject noisy or misleading measurements rather than silently reporting them as fact.

This project is structured in phases. Each phase has its own exit bar; don't move on until the current one is met.

## Phase 1 — Correct, Non-Optimized-Away Measurement

Get the benchmark registration mechanism working and producing numbers you can trust are actually measuring the workload, not a compiler-eliminated no-op. Exit bar: a deliberately trivial benchmark (e.g. an empty loop) is shown, with evidence (disassembly or a before/after with the optimizer-defeat mechanism removed), to *not* be silently optimized away.

## Phase 2 — Statistical Rigor

Add multiple iterations, warm-up discarding, and spread reporting (not just a mean). Exit bar: running the same benchmark twice reports consistent spread characteristics, and a benchmark with injected artificial noise (e.g. an occasional sleep) is visibly flagged as noisy rather than averaged away into a falsely confident number.

## Phase 3 — Real Coverage

Wire up the mandated coverage: P-4.4's arena/pool allocators vs. the default allocator, and at least one standard container operation across varying input sizes. Exit bar: all mandated benchmarks run and produce plausible, explainable results (e.g. the arena allocator measurably outperforming the default allocator on the workload it's designed for).

## Phase 4 — Regression Gate

Build the baseline-comparison mode with a configurable regression threshold. Exit bar: a deliberately regressed benchmark (e.g. an artificially slowed-down variant) is correctly flagged as a regression against a stored baseline, and a benchmark within noise tolerance is correctly *not* flagged.

## Phase 5 — CI-Ready Output

Machine-readable output format and a worked example of wiring the regression-gate mode into a CI failure exit code. Exit bar: a shell invocation of the harness against a regressed baseline exits non-zero; against a clean baseline it exits zero.

## Functional Requirements

1. A benchmark registration mechanism (register a named callable workload, optionally parameterized by input size) analogous in spirit to Google Benchmark, but built by you — this project is about understanding what such a harness must actually do, not wrapping an existing one.
2. Statistically sound measurement: multiple iterations with warm-up runs discarded, reporting not just a mean but a measure of spread (e.g. min/median/max, or standard deviation) so a caller can judge measurement noise rather than trusting a single number.
3. Defense against compiler over-optimization eliding the very work being measured (per [P-1.4](../../level-1/copy-move-harness/STATEMENT.md)'s lessons) — the harness itself must provide the optimizer-defeat mechanism (e.g. a `DoNotOptimize`-style helper) so individual benchmarks don't each have to reinvent it, and don't accidentally omit it.
4. A regression-gate mode: given a baseline result file and a fresh run, report which benchmarks regressed beyond a configurable threshold (e.g. >10% slower), suitable for wiring into CI as a pass/fail gate.
5. Benchmark coverage of this workbook's own allocator/container projects: at minimum, the arena/pool allocators from [P-4.4](../../level-4/arena-pool-allocators/STATEMENT.md) against the default allocator, and at least one standard container operation (e.g. `std::vector` push_back with/without reserve, or a custom container from an earlier project) under varying input sizes.
6. Human-readable and machine-readable (e.g. JSON or CSV) output formats — the latter is what the regression-gate mode consumes, and what would feed a CI dashboard in a real deployment.

## Input

Registered benchmark workloads (callables), a target iteration/warm-up configuration, and (in regression-gate mode) a baseline result file to compare against.

## Output

A results report (human-readable table and machine-readable file) with per-benchmark timing statistics; in regression-gate mode, a pass/fail verdict and a list of regressed benchmarks.

## Constraints

- C++20. Must run correctly (and produce sensible, not necessarily fast, results) on both the MSVC and WSL-GCC/Clang toolchains, since benchmark numbers are expected to be compared across environments in later use.
- The optimizer-defeat mechanism must be verified to actually work (i.e., demonstrate that without it, a trivial no-op-looking benchmark reports impossibly fast/zero time, and with it, it reports a believable, non-zero time) — asserting it works without demonstrating it is not sufficient.
- The regression-gate threshold and iteration/warm-up counts must be configurable, not hard-coded, since different benchmarks legitimately need different settings (a microsecond-scale benchmark needs many more iterations than a millisecond-scale one to get a stable measurement).

## Edge Cases

- A benchmark whose workload has highly variable timing (e.g. due to unpredictable cache/branch behavior) — the harness's spread-reporting must make this visible rather than hiding it behind a single misleadingly-precise mean.
- A benchmark run on a noisy machine (e.g. other processes competing for CPU) — the harness cannot eliminate this, but should document how a caller might mitigate it (pinning, isolated runs, discarding outlier iterations) even if full isolation is out of scope.
- Comparing a regression-gate baseline captured on one machine against a fresh run on a different machine — must be explicitly flagged as a caveat in the harness's documentation (absolute numbers aren't portable across machines; regression-gate mode is meaningful only when baseline and fresh run share the same machine/environment).
- An extremely fast workload (sub-nanosecond per iteration after optimization) where the timer's own resolution becomes a significant fraction of the measured time — the harness should detect and warn about this rather than silently reporting a number dominated by timer noise.

## Error Handling

- A benchmark workload that throws an exception — reported as a benchmark failure distinguishable from a timing result, not silently swallowed or crashing the whole harness run.
- A malformed or missing baseline file in regression-gate mode — a clear, actionable error, not a crash or a false "no regressions" result.

## Acceptance Criteria

- The optimizer-defeat demonstration (with-vs-without) produces the expected contrast (near-zero vs believable non-zero timing) and is included in the project's own test/demo output.
- Running the harness against P-4.4's allocators reproduces (with this harness's own measurement, not copy-pasted numbers) a measurable throughput advantage for the arena/pool allocators on their target workloads.
- The regression-gate mode correctly identifies an intentionally-introduced regression (e.g. temporarily add an unnecessary allocation to a benchmarked workload) as a threshold-exceeding regression, and correctly reports no regression for an unchanged workload run twice.
- Spread reporting (not just mean) is present in both human- and machine-readable output.

## Testing Requirements

- Correctness tests for the benchmark registration/execution machinery itself (using fast, deterministic synthetic workloads with known relative cost, e.g. one workload that sleeps 2x as long as another, to confirm the harness reports that ratio correctly).
- The optimizer-defeat with-vs-without demonstration.
- The regression-gate mode's detection of an intentionally introduced regression, and its correct pass on an unchanged rerun.
- Coverage benchmarks against P-4.4's allocators and at least one container operation.

## Hints

### Hint 1 — Direction
Build this in two clearly separated layers: a measurement core (run a callable N times, discard the first K as warm-up, record per-iteration timings, compute statistics) that knows nothing about what it's timing, and a reporting/comparison layer (formatting, regression-gate diffing) that knows nothing about how timing was performed. Keeping these separate is what lets you test the statistics logic with synthetic, instant-running fake workloads rather than needing real slow benchmarks for every test of the harness itself.

### Hint 2 — Technique
The classic optimizer-defeat pattern is two-sided: prevent the compiler from proving a computed value is unused (force a value through something the optimizer can't see through, such as a volatile write, inline asm, or a function pointer call — `std::atomic_signal_fence` combined with forcing the value through a pointer is one portable-ish technique) and prevent the compiler from noticing memory isn't actually being read/observed elsewhere (a compiler memory barrier telling it "assume anything could have changed"). A benchmark missing either half can still be silently over-optimized.

### Hint 3 — Implementation
For spread reporting without excessive complexity, running a benchmark as many short *batches* of iterations (not one giant batch) and computing statistics across the batches' per-batch mean times is a good balance — it smooths out per-iteration timer-resolution noise while still surfacing batch-to-batch variance from real sources (cache state, scheduler interference, thermal throttling).

### Hint 4 — Debugging/Design
If your optimizer-defeat demonstration doesn't show the expected contrast (i.e., the "without defeat" version isn't actually near-zero), check your compiler's actual optimization level for the benchmark build — a benchmark harness accidentally built in a debug/unoptimized configuration will not exhibit the over-optimization problem at all, which would make the whole demonstration meaningless; the demonstration is only informative when compiled with real optimizations enabled (matching how these benchmarks would actually be run in practice).
