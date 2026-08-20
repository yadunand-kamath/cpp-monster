# C-1 — Embeddable Persistent Key-Value Store

**Level:** Capstone · **Est. effort:** XL (40h+, multi-week)

## Objective

Build an embeddable (in-process, linked-in-library-style, not a client-server service) persistent key-value store that survives a crash without losing committed data, scales to a working set larger than available memory, and provides consistent point-in-time snapshot reads while writes continue concurrently. This capstone deliberately does not name which storage-engine architecture, concurrency-control scheme, or specific data structures to use — figuring that out, and defending the choice, is the point.

This project is structured in five phases. Each phase has its own bar; do not move to the next phase until the current one's bar is met. A capstone that nails Phase 1-2 and documents honestly why Phase 4-5 weren't reached is a far stronger deliverable than one that rushes through all five with a shaky foundation.

## Phase 1 — Core Correctness

Get single-threaded, in-process `get`/`put`/`delete` working correctly against an on-disk representation, with the basic read/write/iteration semantics a caller of a key-value store would expect (ordered iteration if you choose an ordered design; point lookups; overwrite-on-put; tombstone-or-remove on delete). No concurrency, no crash safety yet — get the fundamental data structure and its correctness right first. Exit bar: a comprehensive single-threaded correctness test suite passes, including a randomized model-based test comparing your store's behavior against a simple in-memory reference (e.g. `std::map`) under long random operation sequences.

## Phase 2 — Durability Under Failure

Make writes durable across a crash. A `put` that returns "committed" before a crash must be recoverable after that crash; a `put` that hadn't yet been acknowledged as committed when the crash occurred may legitimately be lost, but that boundary must be exact, not fuzzy. Exit bar: a crash-injection test harness (simulating a crash at multiple precise points during a write) demonstrates correct recovery for every injected scenario, with zero silent data corruption.

## Phase 3 — Concurrency and Scale

Support concurrent readers and writers correctly, and a working set larger than available memory (i.e., the store cannot simply hold everything resident and rely on the OS page cache alone as its entire strategy — some deliberate on-disk organization and bounded-memory access pattern is required). Snapshot reads: a reader that begins iterating/reading at time T continues to see a consistent view of the data as of T, even as concurrent writers commit further changes. Exit bar: a concurrent stress test (many reader and writer threads) is race-free under ThreadSanitizer, and a snapshot-consistency test demonstrates a long-running reader is unaffected by concurrent writes that occur during its lifetime.

## Phase 4 — Performance

Establish and meet a specific, stated performance target for your design (e.g. a specific read/write throughput or latency percentile under a specific workload shape) using [P-5.1](../../level-5/allocator-container-benchmark-harness/STATEMENT.md)'s benchmark harness or an equivalent sound methodology, and demonstrate you understand *why* your design performs the way it does relative to that target — including at least one deliberate optimization made in response to a measured bottleneck, with before/after numbers. Exit bar: a documented performance report with methodology, target, measured result, and at least one profile-guided optimization with before/after evidence.

## Phase 5 — Production Hardening

Address at least three of: comprehensive error handling for disk-full/permission-denied/corrupted-file scenarios; an observability/metrics surface (operation counts, latency histograms, storage-engine-internal statistics like compaction activity if relevant to your design); a documented data format versioning/migration story; a fuzz-tested parser/deserializer boundary (if your on-disk format has one, which it will); resource limits (bounding memory/file-handle usage under adversarial conditions). Exit bar: each addressed area has a specific test or demonstration, not just a design note.

## Constraints

- C++20. No third-party storage-engine library (e.g. no LevelDB, RocksDB, SQLite) for the core engine — this capstone's entire value is in designing and building that engine yourself. Lower-level utility reuse from earlier workbook projects (allocators, the WAL primitive from [P-5.4](../../level-5/write-ahead-log-crash-recovery/STATEMENT.md), a benchmark harness) is expected and encouraged.
- Must build and run correctly on both the MSVC and WSL-GCC/Clang toolchains.
- Every phase's exit bar must be met with an actual demonstrated test/benchmark, not a design-document claim.

## Documentation Deliverable

Alongside the code, a design document explaining: the chosen storage-engine architecture and why (with at least one credible alternative architecture named and why it was not chosen), the concurrency-control scheme and its correctness argument, the crash-recovery protocol and its correctness argument, and an honest accounting of which phases were fully reached and which were partially or not reached, with reasons.

## Acceptance Criteria

- Phase 1 through at least Phase 3's exit bars are met, with Phase 4 and 5 attempted to whatever extent time allows and honestly documented either way.
- The randomized model-based correctness test (Phase 1) passes across a large number of random operation sequences.
- The crash-injection test suite (Phase 2) passes for every injected crash scenario.
- The concurrent stress test and snapshot-consistency test (Phase 3) both pass, the former confirmed under ThreadSanitizer.
- The design document is complete and honestly accounts for scope reached.

## Hints

### Hint 1 — Direction
This capstone deliberately doesn't tell you whether to build an LSM-tree, a B+tree, or something else — that decision, and being able to defend it, is itself a major part of what this capstone is assessing. Research both broad families' tradeoffs (write amplification, read amplification, space amplification, and how each handles a working set larger than memory) before committing, and write down your reasoning before writing code — you'll want that reasoning for the design document regardless of which you choose.

### Hint 2 — Technique
Whichever architecture you choose, a write-ahead log for crash safety and a separate, periodically-updated on-disk index/data structure for efficient reads is a near-universal pattern across real storage engines — [P-5.4](../../level-5/write-ahead-log-crash-recovery/STATEMENT.md)'s WAL primitive (or a design very much like it) is a natural, directly reusable foundation for Phase 2's durability requirement regardless of which broader architecture you pick for Phase 1's data structure.

### Hint 3 — Implementation
For Phase 3's snapshot-read consistency without blocking writers, look into multi-version concurrency control (MVCC) — readers see a consistent snapshot by reading data tagged with (or organized by) a version/sequence number no newer than the version active when their read began, while writers continue creating new versions; this avoids readers and writers contending for the same lock, at the cost of needing a policy for when old versions can be reclaimed.

### Hint 4 — Debugging/Design
If Phase 3's concurrent stress test reveals races that are hard to localize, resist the urge to add broad locking as a quick fix — go back to Phase 1/2's design and identify exactly which specific piece of shared mutable state is actually being raced on (the in-memory index? the WAL's write pointer? a shared buffer?) and apply the narrowest correct synchronization to that specific piece; a capstone-scale project with one big lock protecting everything will pass ThreadSanitizer but will have failed the actual concurrency-design assessment this phase is checking for.
