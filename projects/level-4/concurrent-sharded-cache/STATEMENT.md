# P-4.5 — Concurrent Sharded Cache

**Level:** 4 (Systems component) · **Category:** Storage · **Requires:** Ch01–12 · **Est. effort:** L (16-24h)

## Objective

Build a thread-safe in-memory cache bounded by actual *memory usage* (not entry count), sharded across multiple independently-locked partitions to reduce contention, with TTL-based expiry and hit-rate/eviction metrics — the kind of cache that sits in front of an expensive lookup in a real service.

## Functional Requirements

1. `get(key) -> optional<Value>`, `put(key, value, ttl)`, `erase(key)` — standard cache operations, safe under concurrent access from multiple threads.
2. The cache is sharded into N independent partitions (key hashed to a shard, each shard independently locked), so operations on different shards don't contend with each other — this is the primary mechanism for reducing lock contention under concurrent load, and must be demonstrated to actually reduce it, not merely be present in the code.
3. Bounded by a configured *memory budget* (e.g. "don't exceed roughly 100MB of cached value data"), not a fixed entry count — since real workloads have wildly varying per-entry sizes, an entry-count bound doesn't actually bound memory usage. Track approximate memory usage (a caller-provided or computed size-of-value function is acceptable) and evict when over budget.
4. An eviction policy when over budget — LRU is the natural default; document the chosen policy and why it fits a general-purpose cache's needs.
5. TTL-based expiry: entries older than their configured TTL are treated as absent on `get()`, and are eventually actually reclaimed (not just logically ignored forever, which would still leak memory).
6. Metrics: hit count, miss count, eviction count (broken down by "evicted due to TTL" vs "evicted due to memory pressure"), queryable at runtime.

## Input

Key/value pairs (generic over key/value types, with a documented minimal set of required operations — hashable key, at minimum) plus a per-entry TTL and a way to estimate a value's memory footprint.

## Output

Cached values on hit, `nullopt`/miss signal otherwise, plus queryable metrics.

## Constraints

- C++20. Must be race-free under ThreadSanitizer across a concurrent stress test exercising all operations (get/put/erase/expiry/eviction) simultaneously from multiple threads.
- Sharding must be demonstrated to reduce measured contention (e.g. via a benchmark comparing throughput at 1 shard vs N shards under concurrent multi-threaded load) — the design isn't credited just for having a `shard_count` parameter.
- Must not require a global lock for any read-heavy operation — reads within a shard may still need that shard's lock, but no operation should require locking *every* shard simultaneously except for genuinely whole-cache operations (if any are even offered — a `clear_all()` is a reasonable exception, documented as such).

## Edge Cases

- A key that hashes to a shard whose eviction policy just evicted the very key being looked up moments earlier (a hit becomes a miss due to a concurrent eviction) — this is legitimate concurrent behavior, not a bug, but the metrics must still be internally consistent (no double-counted or lost events).
- An entry whose value is much larger than the average — the memory-budget tracking must handle wide size variance correctly, not assume roughly-uniform entry sizes.
- `put()` for a key that already exists — must correctly update both the value and the memory-budget accounting (not double-count the old entry's size after replacement).
- TTL expiry racing with a concurrent `get()` for the same key at nearly the same moment — must resolve to one clear, consistent outcome (either a hit with the about-to-expire value, or a clean miss), never a corrupted/torn read.

## Error Handling

- A caller-provided size-estimation function that returns 0 or an unreasonable value — document the resulting behavior (the memory-budget mechanism becomes ineffective, which is a caller error, not a cache bug) rather than silently producing nonsensical eviction behavior without any documentation.

## Acceptance Criteria

- A concurrent stress test (multiple threads doing gets/puts/erases against overlapping key ranges) runs correctly and is ThreadSanitizer-clean.
- Sharding's contention-reduction benefit is measured and reported: a benchmark comparing achieved throughput at shard-count 1 versus a realistic higher shard count (e.g. matching core count) under concurrent load from enough threads to actually create contention at shard-count 1.
- Memory-budget enforcement is demonstrated: inserting entries whose total size well exceeds the configured budget results in evictions keeping actual tracked usage near the budget, not unboundedly growing.
- TTL expiry is demonstrated: an entry inserted with a short TTL is confirmed absent on `get()` after that TTL elapses, and confirmed to have actually been reclaimed (not merely logically hidden) via a memory-usage check.
- Metrics correctly and consistently reflect a scripted sequence of hits, misses, and both eviction categories.

## Testing Requirements

- Correctness tests for get/put/erase under single-threaded use first, then under concurrent multi-threaded stress.
- The memory-budget enforcement test with wide entry-size variance.
- The TTL expiry-and-reclaim test.
- The metrics-consistency test against a scripted, verifiable sequence of operations.
- The shard-count-1-vs-N contention benchmark.

## Hints

### Hint 1 — Direction
Think of the cache as N entirely independent smaller caches (each with its own lock, its own LRU list, its own share of the memory budget), unified only by a top-level `hash(key) % N` routing function that picks which shard's cache to delegate an operation to — almost none of the interesting logic (LRU eviction, TTL tracking) needs to know sharding exists at all; it's implemented once, inside a single shard's type, and sharding is purely a routing layer on top.

### Hint 2 — Technique
For LRU within a single shard under its own lock, the classic combination of an intrusive doubly-linked list (for O(1) move-to-front on access and O(1) eviction from the back) plus a hash map from key to list-node pointer (for O(1) lookup) gives you O(1) get/put/evict, which matters since a shard's lock is held for the duration of each operation — anything slower directly increases that shard's contention window.

### Hint 3 — Implementation
For dividing the memory budget across shards, the simplest defensible approach is an equal per-shard budget (`total_budget / shard_count`) — since keys hash roughly uniformly across shards for a reasonably-behaved hash function, this tends to work well in practice; document this choice and its assumption (uniform-ish key distribution across shards) rather than building a more complex cross-shard budget-rebalancing scheme unless you have a specific reason to.

### Hint 4 — Debugging/Design
If your contention benchmark shows no meaningful throughput improvement going from 1 shard to N shards, check whether your benchmark's key access pattern is actually spread across shards — a benchmark that repeatedly hits a small number of "hot" keys will funnel most traffic into whichever few shards those keys happen to hash to, regardless of how many shards exist in total, which would make your sharding implementation look ineffective even if it's implemented correctly; make sure the benchmark's key distribution genuinely exercises multiple shards.
