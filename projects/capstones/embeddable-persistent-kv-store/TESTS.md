# C-1 — Tests

## Visible Tests (GoogleTest)

### Phase 1 — Core Correctness

```cpp
TEST(KvStore, PutThenGetReturnsSameValue) {
    KvStore store(temp_db_path());
    store.put("key1", "value1");
    auto result = store.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "value1");
}

TEST(KvStore, GetOnMissingKeyReturnsNullopt) {
    KvStore store(temp_db_path());
    EXPECT_FALSE(store.get("nonexistent").has_value());
}

TEST(KvStore, PutOnExistingKeyOverwritesValue) {
    KvStore store(temp_db_path());
    store.put("key1", "first");
    store.put("key1", "second");
    EXPECT_EQ(*store.get("key1"), "second");
}

TEST(KvStore, DeleteRemovesKey) {
    KvStore store(temp_db_path());
    store.put("key1", "value1");
    store.remove("key1");
    EXPECT_FALSE(store.get("key1").has_value());
}

TEST(KvStore, OrderedIterationReturnsKeysInSortedOrder) {
    KvStore store(temp_db_path());
    store.put("banana", "1"); store.put("apple", "2"); store.put("cherry", "3");
    std::vector<std::string> keys;
    for (auto it = store.begin(); it != store.end(); ++it) keys.push_back(it->key);
    EXPECT_THAT(keys, ElementsAre("apple", "banana", "cherry"));
}

TEST(KvStore, ModelBasedRandomOperationSequenceMatchesReferenceMap) {
    KvStore store(temp_db_path());
    std::map<std::string, std::string> reference;
    std::mt19937 rng(12345);
    for (int i = 0; i < 100000; ++i) {
        auto [key, op] = random_operation(rng);
        switch (op) {
            case Op::kPut: {
                std::string value = random_value(rng);
                store.put(key, value);
                reference[key] = value;
                break;
            }
            case Op::kDelete:
                store.remove(key);
                reference.erase(key);
                break;
            case Op::kGet: {
                auto store_result = store.get(key);
                auto ref_it = reference.find(key);
                if (ref_it == reference.end()) EXPECT_FALSE(store_result.has_value());
                else EXPECT_EQ(*store_result, ref_it->second);
                break;
            }
        }
    }
    // final full-scan comparison
    std::map<std::string, std::string> store_snapshot;
    for (auto it = store.begin(); it != store.end(); ++it) store_snapshot[it->key] = it->value;
    EXPECT_EQ(store_snapshot, reference);
}
```

### Phase 2 — Durability Under Failure

```cpp
TEST(KvStoreDurability, CommittedPutSurvivesSimulatedCrashAndRecovery) {
    std::string path = temp_db_path();
    { KvStore store(path); store.put("key1", "value1"); store.flush_and_confirm_durable(); }
    KvStore recovered(path);  // fresh open triggers recovery
    EXPECT_EQ(*recovered.get("key1"), "value1");
}

TEST(KvStoreDurability, CrashInjection_MidWalHeaderWrite_RecoversToLastCompleteRecord) {
    CrashInjectingKvStore store(temp_db_path());
    store.put("key1", "value1");
    store.confirm_durable();
    store.begin_put_and_crash_after_header_bytes("key2", "value2", /*header_bytes=*/6);
    KvStore recovered(store.path());
    EXPECT_EQ(*recovered.get("key1"), "value1");
    EXPECT_FALSE(recovered.get("key2").has_value());
}

TEST(KvStoreDurability, CrashInjection_MidPayloadWrite_RecoversToLastCompleteRecord) {
    CrashInjectingKvStore store(temp_db_path());
    store.put("key1", "value1");
    store.confirm_durable();
    store.begin_put_and_crash_mid_payload("key2", "a_long_value_that_gets_torn");
    KvStore recovered(store.path());
    EXPECT_EQ(*recovered.get("key1"), "value1");
    EXPECT_FALSE(recovered.get("key2").has_value());
}

TEST(KvStoreDurability, CrashInjection_AfterFullWriteBeforeFsync_MatchesDocumentedContract) {
    CrashInjectingKvStore store(temp_db_path());
    store.put("key1", "value1", /*confirm_durable=*/true);
    store.begin_put_and_crash_after_write_before_fsync("key2", "value2");
    KvStore recovered(store.path());
    // documented contract: append() alone (no explicit flush) does NOT guarantee durability
    EXPECT_TRUE(recovered.get("key2").has_value() || !recovered.get("key2").has_value());
    // the real assertion: no corruption, no crash, and key1 is always present
    EXPECT_EQ(*recovered.get("key1"), "value1");
}

TEST(KvStoreDurability, RecoveryReportsNoSilentCorruptionAcrossManyRandomCrashPoints) {
    for (int trial = 0; trial < 500; ++trial) {
        CrashInjectingKvStore store(temp_db_path());
        auto committed = perform_random_puts_with_random_crash_point(store, trial);
        KvStore recovered(store.path());
        for (auto& [key, value] : committed) EXPECT_EQ(*recovered.get(key), value);
        EXPECT_TRUE(recovered.last_recovery_report().corrupted_records == 0 ||
                    recovered.last_recovery_report().corruption_was_at_the_torn_tail);
    }
}
```

### Phase 3 — Concurrency and Scale

```cpp
TEST(KvStoreConcurrency, ManyReaderAndWriterThreadsAreRaceFree) {
    KvStore store(temp_db_path());
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&store, i] {
        for (int j = 0; j < 10000; ++j) {
            store.put("key" + std::to_string(i % 4), "v" + std::to_string(j));
            (void)store.get("key" + std::to_string((i + 1) % 4));
        }
    });
    for (auto& t : threads) t.join();
    SUCCEED(); // race-freedom is verified by running this test under ThreadSanitizer in CI
}

TEST(KvStoreConcurrency, SnapshotReaderIsUnaffectedByConcurrentWrites) {
    KvStore store(temp_db_path());
    for (int i = 0; i < 1000; ++i) store.put("key" + std::to_string(i), "initial");
    auto snapshot = store.snapshot();
    std::thread writer([&store] {
        for (int i = 0; i < 1000; ++i) store.put("key" + std::to_string(i), "modified");
    });
    for (int i = 0; i < 1000; ++i) EXPECT_EQ(*snapshot.get("key" + std::to_string(i)), "initial");
    writer.join();
}

TEST(KvStoreConcurrency, WorkingSetLargerThanConfiguredMemoryBudgetStillReadsCorrectly) {
    KvStore store(temp_db_path(), /*memory_budget_bytes=*/1 << 20); // deliberately small
    for (int i = 0; i < 200000; ++i) store.put(make_key(i), make_value(i));
    for (int i = 0; i < 200000; ++i) EXPECT_EQ(*store.get(make_key(i)), make_value(i));
}
```

### Phase 4 — Performance

```cpp
TEST(KvStorePerformance, MeetsStatedWriteThroughputTargetUnderSequentialWorkload) {
    auto result = run_benchmark("sequential_put", [] { /* ... */ });
    EXPECT_GE(result.ops_per_second, kDocumentedWriteThroughputTarget);
}

TEST(KvStorePerformance, MeetsStatedReadLatencyTargetUnderPointLookupWorkload) {
    auto result = run_benchmark("point_lookup_p99", [] { /* ... */ });
    EXPECT_LE(result.p99_latency_us, kDocumentedReadLatencyTargetUs);
}
```

## Manual/Script-Driven Verification

- ThreadSanitizer run of the full Phase 3 concurrent stress test suite (WSL-Clang, `-fsanitize=thread`) — zero reported races is the exit-bar requirement, not merely "test passes."
- A long-running (multi-minute) fuzz/stress test that randomly interleaves put/get/delete/snapshot/crash-injection across many threads, watched for any assertion failure, deadlock, or corrupted-recovery report.
- The Phase 4 performance report: methodology, stated target, measured result, before/after numbers for at least one profile-guided optimization.

## Hidden Tests

- an exact-recovery-offset test: after a crash mid-write, the recovered store's *next* append does not overwrite or corrupt the last valid record — this specifically catches an off-by-a-few-bytes error in the resume-offset calculation that the "does recovery find the right records" tests above would not catch
- a torn-write test at every single byte offset within one record's header+payload (not just a few hand-picked offsets), confirming recovery is correct universally rather than just at the specific offsets the visible tests happened to pick
- a concurrent-writer-during-compaction (or equivalent background-maintenance-operation) correctness test, if the chosen architecture has such an operation
- a snapshot-reader-outlives-multiple-subsequent-compactions test, confirming old-version data a snapshot depends on is not reclaimed while the snapshot is still alive
- a resource-exhaustion test (disk full during write) producing a clear error rather than a corrupted on-disk state
