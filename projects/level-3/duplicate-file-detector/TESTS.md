# P-3.4 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(SizeGrouping, FilesOfDifferentSizesAreNeverGroupedTogether) {
    auto groups = group_by_size(make_files({{"a", 100}, {"b", 200}, {"c", 100}}));
    EXPECT_EQ(groups.size(), 2);
}

TEST(PartialHash, SameSizeDifferentEarlyContentSplitsGroup) {
    auto a = write_file_with_prefix("a", "AAAA...", 1'000'000);
    auto b = write_file_with_prefix("b", "BBBB...", 1'000'000);
    auto groups = group_by_partial_hash({a, b});
    EXPECT_EQ(groups.size(), 2);
}

TEST(FullHash, SameSizeSamePrefixDifferentTailIsNotFalselyDuplicate) {
    auto a = write_file_with_common_prefix_and_tail("a", "X");
    auto b = write_file_with_common_prefix_and_tail("b", "Y");
    auto dupes = find_duplicates({a, b});
    EXPECT_TRUE(dupes.empty());
}

TEST(FullHash, GenuinelyIdenticalContentIsReportedAsDuplicate) {
    auto a = write_file("a", "identical content");
    auto b = write_file("b", "identical content");
    auto dupes = find_duplicates({a, b});
    ASSERT_EQ(dupes.size(), 1);
    EXPECT_EQ(dupes[0].paths.size(), 2);
}

TEST(LinkHandling, HardLinksAreReportedSeparatelyFromContentDuplicates) {
    auto a = write_file("a", "content");
    auto b = make_hard_link(a, "b");
    auto result = scan({a, b});
    EXPECT_TRUE(result.hard_link_groups.size() >= 1);
    EXPECT_TRUE(result.content_duplicate_groups.empty());
}

TEST(LinkHandling, SymlinksAreNotDereferencedIntoFalseDuplicates) {
    auto a = write_file("a", "content");
    auto link = make_symlink(a, "link_to_a");
    auto result = scan({a, link});
    // documented policy: symlinks either skipped entirely or reported in their own category
    EXPECT_TRUE(result.content_duplicate_groups.empty());
}

TEST(ZeroByteFiles, HandledPerDocumentedPolicy) {
    auto a = write_file("a", "");
    auto b = write_file("b", "");
    auto result = scan({a, b});
    // exact assertion depends on documented policy — either excluded or grouped
}

TEST(CascadeEffectiveness, FullHashCountIsLessThanTotalFileCountOnDecoyCorpus) {
    auto corpus = make_same_size_decoy_corpus(1000); // same size, different content
    ScanStats stats;
    find_duplicates(corpus, &stats);
    EXPECT_LT(stats.full_hashes_computed, corpus.size());
}

TEST(Resume, InterruptedScanResumesWithoutRehashingCompletedFiles) {
    auto corpus = make_large_corpus(500);
    auto state_path = temp_state_path();
    run_scan_and_kill_partway(corpus, state_path);
    ScanStats resumed_stats;
    resume_scan(corpus, state_path, &resumed_stats);
    EXPECT_LT(resumed_stats.files_rehashed, corpus.size());
}
```

## Hidden Tests

- a multi-gigabyte test file, confirming peak memory during hashing stays bounded (not proportional to file size) via a streaming read-and-hash implementation
- a file modified between discovery and hashing (simulated via a hook), checked against the tool's documented consistency policy
- a file becoming inaccessible mid-scan (permission removed or deleted), confirming the scan logs and continues rather than aborting entirely
- a corrupted progress-state file on resume, confirming a clear error rather than a crash or silently wrong result
- peak-memory measurement across a large synthetic corpus (tens of thousands of files) showing bounded, not linearly-growing-without-bound, memory usage
