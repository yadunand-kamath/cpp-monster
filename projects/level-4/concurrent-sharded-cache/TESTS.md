# P-4.5 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(ShardedCache, PutThenGetReturnsSameValue) {
    ShardedCache<std::string, std::string> cache(/*shard_count=*/4, /*memory_budget=*/1 << 20);
    cache.put("key1", "value1", /*ttl=*/std::chrono::seconds(60));
    auto result = cache.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");
}

TEST(ShardedCache, GetOnMissingKeyReturnsNullopt) {
    ShardedCache<std::string, std::string> cache(4, 1 << 20);
    EXPECT_FALSE(cache.get("nonexistent").has_value());
}

TEST(ShardedCache, EraseRemovesEntry) {
    ShardedCache<std::string, std::string> cache(4, 1 << 20);
    cache.put("key1", "value1", std::chrono::seconds(60));
    cache.erase("key1");
    EXPECT_FALSE(cache.get("key1").has_value());
}

TEST(ShardedCache, PutOnExistingKeyUpdatesValueAndAccounting) {
    ShardedCache<std::string, std::string> cache(4, 1 << 20);
    cache.put("key1", "short", std::chrono::seconds(60));
    cache.put("key1", std::string(1000, 'x'), std::chrono::seconds(60));
    auto stats = cache.stats();
    EXPECT_LT(stats.approx_memory_used, 1100u); // not double-counted against the old "short" size
}

TEST(ShardedCache, TtlExpiryHidesEntryAfterDeadline) {
    ShardedCache<std::string, std::string> cache(4, 1 << 20);
    cache.put("key1", "value1", std::chrono::milliseconds(10));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(cache.get("key1").has_value());
}

TEST(ShardedCache, TtlExpiredEntryIsActuallyReclaimed) {
    ShardedCache<std::string, std::string> cache(4, 1 << 20);
    cache.put("key1", std::string(10000, 'x'), std::chrono::milliseconds(10));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cache.get("key1"); // triggers lazy reclaim path, or a background sweep already ran
    EXPECT_LT(cache.stats().approx_memory_used, 100u);
}

TEST(ShardedCache, MemoryBudgetEnforcedUnderWideSizeVariance) {
    ShardedCache<std::string, std::string> cache(4, /*memory_budget=*/10000);
    for (int i = 0; i < 100; ++i) {
        cache.put("key" + std::to_string(i), std::string(500, 'x'), std::chrono::seconds(60));
    }
    EXPECT_LE(cache.stats().approx_memory_used, 12000u); // near budget, not unbounded
    EXPECT_GT(cache.stats().eviction_count_memory_pressure, 0u);
}

TEST(ShardedCache, MetricsReflectScriptedSequence) {
    ShardedCache<std::string, std::string> cache(4, 1 << 20);
    cache.put("a", "1", std::chrono::seconds(60));
    cache.get("a");             // hit
    cache.get("b");             // miss
    cache.get("a");             // hit
    auto stats = cache.stats();
    EXPECT_EQ(stats.hit_count, 2u);
    EXPECT_EQ(stats.miss_count, 1u);
}

TEST(ShardedCache, ConcurrentGetsPutsErasesAreRaceFree) {
    ShardedCache<int, int> cache(8, 1 << 22);
    constexpr int kThreads = 8, kOpsPerThread = 5000;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&cache, t] {
            for (int i = 0; i < kOpsPerThread; ++i) {
                int key = (t * kOpsPerThread + i) % 200; // overlapping key range across threads
                switch (i % 3) {
                    case 0: cache.put(key, key, std::chrono::seconds(30)); break;
                    case 1: cache.get(key); break;
                    case 2: cache.erase(key); break;
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    // no crash, no TSan report — correctness is that this completes cleanly
}
```

## Manual/Script-Driven Verification

- Shard-count-1-vs-N throughput benchmark: run the same concurrent get/put workload (enough threads to exceed core count) against a cache configured with 1 shard, then against the same cache configured with a shard count matching hardware concurrency, using a key distribution that spreads across shards; report both throughput numbers and the ratio.

## Hidden Tests

- a ThreadSanitizer-instrumented run of the concurrent stress test above, confirming a clean report
- a targeted test that a hit-becomes-miss due to concurrent eviction never produces a metrics inconsistency (hit_count + miss_count exactly equal to total get() calls issued, checked via an atomic external counter)
- a scripted eviction-category test confirming TTL-based evictions and memory-pressure evictions are counted separately and never conflated
- a large-entry-count-variance test (a mix of tiny and very large values) confirming the memory-budget accounting doesn't drift from actual tracked usage over many put/erase cycles
- a repeated put-same-key-many-times-with-different-sizes test confirming accounting never leaks or double-counts across replacements
