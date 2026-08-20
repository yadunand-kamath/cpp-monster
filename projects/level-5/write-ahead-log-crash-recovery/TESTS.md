# P-5.4 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(WriteAheadLog, AppendThenRecoverReturnsRecordsInOrder) {
    {
        WriteAheadLog wal("test.wal");
        wal.append("record1");
        wal.append("record2");
        wal.flush();
    }
    auto records = WriteAheadLog::recover("test.wal");
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0], "record1");
    EXPECT_EQ(records[1], "record2");
}

TEST(WriteAheadLog, MissingLogFileRecoversAsEmpty) {
    auto records = WriteAheadLog::recover("nonexistent.wal");
    EXPECT_TRUE(records.empty());
}

TEST(WriteAheadLog, CorruptedFirstBytesRecoverAsEmptyWithCorruptionReported) {
    write_file("corrupt.wal", std::string(20, '\xFF'));
    auto result = WriteAheadLog::recover_detailed("corrupt.wal");
    EXPECT_TRUE(result.records.empty());
    EXPECT_TRUE(result.corruption_detected);
    EXPECT_EQ(result.valid_tail_offset, 0u);
}

TEST(WriteAheadLog, CrcMismatchOnPayloadIsDetectedAsCorruption) {
    {
        WriteAheadLog wal("crc_test.wal");
        wal.append("valid record");
        wal.flush();
    }
    corrupt_byte_at_offset("crc_test.wal", /*offset=*/15); // flip a byte inside the payload
    auto result = WriteAheadLog::recover_detailed("crc_test.wal");
    EXPECT_TRUE(result.corruption_detected);
    EXPECT_TRUE(result.records.empty());
}

TEST(CrashInjection, CrashMidHeaderRecoversCleanly) {
    WriteAheadLog wal("crash_header.wal");
    wal.append("record1"); wal.flush();
    wal.inject_crash_after_partial_header_write(/*bytes_written=*/2); // simulated torn header
    auto records = WriteAheadLog::recover("crash_header.wal");
    EXPECT_EQ(records.size(), 1u); // only the first, complete record survives
    EXPECT_EQ(records[0], "record1");
}

TEST(CrashInjection, CrashMidPayloadRecoversToPriorCompleteRecord) {
    WriteAheadLog wal("crash_payload.wal");
    wal.append("record1"); wal.flush();
    wal.inject_crash_after_partial_payload_write("record2_partial_", /*bytes_written=*/8);
    auto records = WriteAheadLog::recover("crash_payload.wal");
    EXPECT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0], "record1");
}

TEST(CrashInjection, CrashPostPayloadPreFsyncMatchesDocumentedDurabilityContract) {
    WriteAheadLog wal("crash_presync.wal");
    wal.append("record1"); wal.flush();
    wal.inject_crash_after_payload_write_before_fsync("record2"); // written but not synced
    auto records = WriteAheadLog::recover("crash_presync.wal");
    // per this WAL's documented contract: append() alone does not guarantee durability, only flush() does
    EXPECT_EQ(records.size(), 1u); // record2 correctly lost — it was never flushed
}

TEST(GroupCommit, ThroughputImprovesOverOneFsyncPerAppend) {
    auto grouped = benchmark_concurrent_appends(/*group_commit=*/true, /*threads=*/16, /*appends_per_thread=*/500);
    auto ungrouped = benchmark_concurrent_appends(/*group_commit=*/false, 16, 500);
    EXPECT_GT(grouped.throughput_ops_per_sec, ungrouped.throughput_ops_per_sec * 1.5);
}

TEST(GroupCommit, ConcurrentAppendsReturnValidNonOverlappingPositions) {
    WriteAheadLog wal("concurrent.wal");
    std::vector<std::thread> threads;
    std::vector<LogPosition> positions(100);
    for (int t = 0; t < 100; ++t) {
        threads.emplace_back([&, t] { positions[t] = wal.append("record_" + std::to_string(t)); });
    }
    for (auto& th : threads) th.join();
    wal.flush();
    std::set<LogPosition> unique_positions(positions.begin(), positions.end());
    EXPECT_EQ(unique_positions.size(), 100u); // all distinct, no ambiguity
}
```

## Hidden Tests

- a large-record-count recovery test (tens of thousands of records) confirming recovery time scales reasonably and all records recover correctly
- a repeated crash-then-recover-then-append-more-then-crash-again cycle test, confirming the log correctly resumes appending after a previously recovered valid tail without gaps or overwrites
- a fuzzed-corruption test flipping random bytes throughout an existing valid log file across many trials, confirming recovery never crashes and always reports a sensible valid-tail boundary
- a durability-contract stress test specifically confirming that a record which *was* flushed survives an injected crash immediately afterward, contrasted against the already-tested case of one that wasn't
- a checksum-collision-probability sanity discussion test (not a pass/fail assertion, but a documented calculation) confirming the chosen checksum width is appropriate for this WAL's intended record volume
