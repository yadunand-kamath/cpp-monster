# BC-1 — Allocation Reduction Under a Frozen Public API

**Placement:** After Chapter 3 (Value Categories, Move Semantics, Forwarding) · **Format:** Blind Challenge — requirements only, no canonical solution, no named target concepts.

## Premise

> "Cut this pipeline's allocation count 10x without changing its public API."

You are handed a small data-processing pipeline: a chain of functions that take a collection of records, transform them through several stages, and produce a result collection. It works correctly. It allocates far more than it needs to. Your task is to reduce total heap allocations by at least 10x, measured across a representative run, without changing any function's signature, return type, or observable behavior — every existing caller must continue to compile and behave identically.

## What You're Given

- A working pipeline (several hundred lines) with a full passing test suite covering its observable behavior.
- A benchmark harness that reports total heap allocation count (via an instrumented allocator or equivalent) for a representative workload.
- No hints about which specific mechanism is responsible for the excess allocations — that diagnosis is part of the challenge.

## Requirements

- The existing test suite must continue to pass unmodified — behavioral changes of any kind are a failure, not just a public-API signature change.
- The public API (every function signature reachable from outside the pipeline's own internals) must be byte-for-byte unchanged: same parameter types, same return types, same overload set.
- Total heap allocation count on the representative benchmark workload must drop by at least 10x from the measured baseline.
- You must produce a short written explanation of where the allocations were coming from and why your changes eliminate them — a correct fix arrived at by guessing-and-checking without understanding why it works does not meet this requirement.
- No correctness regression: the benchmark's output values, not just its allocation count, must match the original implementation's output exactly.

## What Success Looks Like

A before/after allocation count meeting the 10x bar, the full test suite green, and a clear written account of the specific allocation sources found and eliminated — in terms of *why* each was unnecessary, not just *what* was changed.

## Self-Assessment Questions

- Did you verify the 10x reduction against the *same* representative workload as the baseline, or a different, easier one?
- Could any of your changes have altered behavior in a way the existing test suite doesn't happen to exercise? What would you add to the test suite to be confident it doesn't?
- Is there a remaining allocation source you identified but chose not to address — and if so, why, and what would it take?
