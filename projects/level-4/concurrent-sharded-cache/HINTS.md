# P-4.5 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch11's concurrency material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Resist the temptation to design "the sharded cache" as one big class from the start. Design a single, unsharded cache first — one lock, one LRU list, one memory budget, one TTL map — get it fully correct and tested on its own. Sharding is then a thin routing wrapper that owns N instances of that single-shard cache and picks one by `hash(key) % N`; almost nothing about LRU or TTL logic needs to change to add sharding on top.

## Hint 2 — Technique

For O(1) LRU within a shard, pair an intrusive doubly-linked list (most-recently-used at the front, least-recently-used at the back, so eviction always removes from the back and access always splices to the front) with a hash map from key to the list node holding that key's value — this combination is the standard technique and avoids O(n) scans on every access.

## Hint 3 — Implementation

Track memory usage as a single running counter per shard, incremented by the estimated size of a value on `put()` and decremented by that same size on `erase()` or eviction — the critical detail is that on a `put()` for an *already-present* key, you must subtract the old value's estimated size before adding the new one's, not simply add the new size on top, or the counter drifts upward every time a key is updated.

## Hint 4 — Debugging/Design

If your TSan run reports a race on a metrics counter (hit_count/miss_count/eviction_count), remember these don't need to participate in the same lock that protects the LRU list/map for correctness — but they do need *some* synchronization (a `std::atomic<uint64_t>` with relaxed increments is sufficient, since metrics are read for reporting, not used to make cache-correctness decisions) or the count itself is a data race even though the cached values are fine.
