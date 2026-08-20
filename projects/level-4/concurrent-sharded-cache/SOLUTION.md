# P-4.5 — Solution

## Reference Architecture

A single shard, holding its own lock, LRU list, and TTL/memory accounting:

```cpp
template <typename Key, typename Value>
class CacheShard {
public:
    CacheShard(std::size_t memory_budget, std::function<std::size_t(const Value&)> size_of)
        : memory_budget_(memory_budget), size_of_(std::move(size_of)) {}

    std::optional<Value> get(const Key& key) {
        std::lock_guard lock(mutex_);
        auto it = index_.find(key);
        if (it == index_.end()) { metrics_.miss_count.fetch_add(1, std::memory_order_relaxed); return std::nullopt; }
        if (is_expired(it->second)) {
            evict_locked(it, EvictionReason::kTtl);
            metrics_.miss_count.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
        touch_locked(it); // move to front of LRU list
        metrics_.hit_count.fetch_add(1, std::memory_order_relaxed);
        return it->second->value;
    }

    void put(const Key& key, Value value, std::chrono::milliseconds ttl) {
        std::lock_guard lock(mutex_);
        std::size_t new_size = size_of_(value);
        if (auto it = index_.find(key); it != index_.end()) {
            approx_memory_used_ -= it->second->size; // subtract old before adding new — Hint 3
            it->second->value = std::move(value);
            it->second->size = new_size;
            it->second->deadline = clock::now() + ttl;
            approx_memory_used_ += new_size;
            touch_locked(it);
        } else {
            list_.push_front({key, std::move(value), new_size, clock::now() + ttl});
            index_[key] = list_.begin();
            approx_memory_used_ += new_size;
        }
        evict_while_over_budget_locked();
    }
private:
    void evict_while_over_budget_locked() {
        while (approx_memory_used_ > memory_budget_ && !list_.empty()) {
            auto last = std::prev(list_.end());
            evict_locked(index_.find(last->key), EvictionReason::kMemoryPressure);
        }
    }
    // ... mutex_, list_ (intrusive doubly-linked, per Hint 2), index_ (key -> list iterator), metrics_
};
```

Top-level router, showing how little sharding logic there actually is:

```cpp
template <typename Key, typename Value>
class ShardedCache {
public:
    ShardedCache(std::size_t shard_count, std::size_t total_memory_budget,
                 std::function<std::size_t(const Value&)> size_of = default_size_of<Value>)
        : shards_(shard_count) {
        std::size_t per_shard_budget = total_memory_budget / shard_count; // Hint 3's stated assumption
        for (auto& shard : shards_) shard = std::make_unique<CacheShard<Key, Value>>(per_shard_budget, size_of);
    }

    std::optional<Value> get(const Key& key) { return shard_for(key).get(key); }
    void put(const Key& key, Value value, std::chrono::milliseconds ttl) { shard_for(key).put(key, std::move(value), ttl); }
private:
    CacheShard<Key, Value>& shard_for(const Key& key) {
        return *shards_[std::hash<Key>{}(key) % shards_.size()];
    }
    std::vector<std::unique_ptr<CacheShard<Key, Value>>> shards_;
};
```

## Design Rationale

**Why is memory-budget division equal-per-shard rather than a shared global counter?** A shared global counter would require cross-shard synchronization on every `put()`/eviction decision, defeating the entire purpose of sharding (independent, low-contention locks). Splitting the budget up front trades a small amount of precision (a hot shard could theoretically want more budget than a cold one) for keeping every accounting decision entirely local to one shard's lock — a deliberate, documented tradeoff, not an oversight.

**Why subtract the old value's size before adding the new one's on `put()` for an existing key, rather than just tracking total insertions minus total evictions?** Values vary in size (a core requirement, not an edge case here), so a `put()` that replaces a small value with a large one — or vice versa — must adjust the running total by the *difference*, not simply add the new size on top of a total that still includes the stale old size. Getting this subtraction order wrong is the single most common bug in this kind of accounting and is exactly what Edge Cases and Hint 3 call out.

**Why do metrics use relaxed atomics rather than being protected by the shard's mutex?** The metrics counters don't participate in any correctness decision the cache makes (unlike the LRU list or the memory-budget check) — they exist purely for external observation. Requiring them to take the shard's lock would add contention for no correctness benefit; a relaxed atomic increment is sufficient because there's no ordering relationship metrics readers need relative to other memory operations, only a guarantee that concurrent increments aren't lost or torn.

## Reference Implementation

The above shows `get`/`put`'s core shape and the sharding router. Remaining work for the learner: `erase()` (mirroring the eviction-accounting pattern), the intrusive LRU list's node structure and `touch_locked`/splice-to-front operation, a background TTL-sweep thread (or fully lazy expiry-on-access, a documented choice with its own tradeoffs — a lazy-only design can let expired-but-never-accessed entries sit uncollected against the memory budget, which interacts with Acceptance Criteria's "actually reclaimed" requirement and should be resolved deliberately), and the `default_size_of<Value>` helper (e.g. `sizeof(Value)` for fixed-size types, with the caller expected to supply a real estimator for anything variable-length like strings).

## Testing Strategy

Test the shard-count-1-vs-N contention benchmark with a key-generation strategy that's explicitly verified to spread across shards (e.g. sequential integer keys with a well-distributing hash) — Hint 4/HINTS.md's debugging note about "hot key" benchmarks producing a false negative applies directly to how this benchmark must be constructed, not just how it's debugged after the fact.

## Performance Analysis

Within a shard, `get`/`put`/`erase` are all O(1) (hash map lookup plus O(1) list splice/insert/removal). The sharding layer adds only an O(1) hash-and-modulo routing step. The overall system's throughput scaling with shard count is sub-linear in practice (shared cache-line effects, imperfect key distribution) but should show a clear, measurable improvement moving from 1 shard to a realistic multiple — that measured improvement, not a theoretical claim, is what Acceptance Criteria requires.

## Failure Modes

- Forgetting to subtract the old value's size on a `put()` that replaces an existing key, causing the memory-budget accounting to drift upward indefinitely under a workload that frequently updates the same keys.
- Locking every shard for a per-key operation (e.g. accidentally taking a single cache-wide lock "just to be safe"), silently defeating the entire point of sharding while still passing correctness tests.
- A TTL implementation that only ever *hides* expired entries on access without any reclamation path, satisfying "absent on get()" but violating "eventually actually reclaimed" and quietly leaking memory under a workload that writes many short-TTL keys it never reads again.

## Extensions

- A per-shard adaptive memory-budget rebalancing scheme relaxing the equal-split assumption documented above, for workloads with genuinely skewed key distributions.
- Exposing shard-level metrics (not just aggregated totals) to make an unevenly-loaded shard visible without needing a separate profiling pass.
