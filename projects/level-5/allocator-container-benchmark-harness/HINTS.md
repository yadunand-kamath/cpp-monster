# P-5.1 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch12's measurement material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

A benchmark harness has exactly two jobs that should never be tangled together: accurately measuring how long a callable takes to run many times, and meaningfully presenting/comparing those measurements. If you find yourself writing formatting code inside your timing loop, or timing logic inside your report generator, that's a sign the separation has blurred.

## Hint 2 — Technique

To defeat the optimizer on both sides of a computation: force the *input* to the code under test through something opaque to the compiler (so it can't precompute the result at compile time), and force the *output* through something that looks like it could be observed externally (so the compiler can't prove the computation is dead and eliminate it). A minimal portable approach: write the result through a `volatile`-qualified pointer, and issue a compiler-level (not CPU-level) memory barrier immediately after, telling the optimizer "treat memory as having possibly changed here."

## Hint 3 — Implementation

Structure a single benchmark run as: run `warmup_iterations` times and discard their timings entirely (letting caches warm, letting any lazy first-touch costs happen), then run `batch_count` batches of `iterations_per_batch` each, recording one elapsed-time-divided-by-count value per batch — your final statistics (mean, stddev, min, max) are computed across the batch means, not across every single raw iteration, which is both cheaper to compute and less dominated by any one iteration's timer-resolution noise.

## Hint 4 — Debugging/Design

If your regression-gate mode reports a false regression on a rerun you know didn't change anything, check whether your comparison threshold accounts for the spread you're already measuring — comparing only two single mean values with a fixed percentage threshold, while ignoring each run's own reported standard deviation, will flag normal measurement noise as a regression more often than a real threshold-aware comparison would; consider requiring the observed difference to exceed some multiple of the combined spread, not just a flat percentage of the mean.
