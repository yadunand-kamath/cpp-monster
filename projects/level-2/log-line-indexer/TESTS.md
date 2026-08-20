# P-2.1 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(LogIndexer, IndexesSmallHandWrittenFile) {
    auto path = write_temp_log({
        "2026-01-01T00:00:00 INFO start",
        "2026-01-01T00:00:05 ERROR failure one",
        "2026-01-01T00:00:10 WARN degraded",
        "2026-01-01T00:00:15 ERROR failure two",
    });
    LogIndex idx(path);
    auto results = idx.query_range(parse_ts("2026-01-01T00:00:00"), parse_ts("2026-01-01T00:00:12"));
    EXPECT_EQ(results.size(), 3u);
}

TEST(LogIndexer, RangeQueryExcludesUpperBound) {
    auto path = write_temp_log({
        "2026-01-01T00:00:00 INFO a",
        "2026-01-01T00:00:10 INFO b",
    });
    LogIndex idx(path);
    auto results = idx.query_range(parse_ts("2026-01-01T00:00:00"), parse_ts("2026-01-01T00:00:10"));
    EXPECT_EQ(results.size(), 1u); // [T1, T2) — b at exactly T2 excluded
}

TEST(LogIndexer, LevelFilterWithinRange) {
    auto path = write_temp_log({
        "2026-01-01T00:00:00 INFO a",
        "2026-01-01T00:00:05 ERROR b",
        "2026-01-01T00:00:10 ERROR c",
    });
    LogIndex idx(path);
    auto results = idx.query_range_level(parse_ts("2026-01-01T00:00:00"), parse_ts("2026-01-01T00:00:20"), "ERROR");
    EXPECT_EQ(results.size(), 2u);
}

TEST(LogIndexer, EmptyFileProducesEmptyIndex) {
    auto path = write_temp_log({});
    LogIndex idx(path);
    auto results = idx.query_range(parse_ts("2026-01-01T00:00:00"), parse_ts("2027-01-01T00:00:00"));
    EXPECT_TRUE(results.empty());
}

TEST(LogIndexer, RangeOutsideFileReturnsEmpty) {
    auto path = write_temp_log({"2026-01-01T00:00:00 INFO a"});
    LogIndex idx(path);
    auto results = idx.query_range(parse_ts("2030-01-01T00:00:00"), parse_ts("2031-01-01T00:00:00"));
    EXPECT_TRUE(results.empty());
}

TEST(LogIndexer, InvertedRangeReturnsEmptyNotError) {
    auto path = write_temp_log({"2026-01-01T00:00:00 INFO a"});
    LogIndex idx(path);
    auto results = idx.query_range(parse_ts("2026-01-02T00:00:00"), parse_ts("2026-01-01T00:00:00"));
    EXPECT_TRUE(results.empty());
}

TEST(LogIndexer, MalformedLinesAreCountedNotMisparsed) {
    auto path = write_temp_log({
        "2026-01-01T00:00:00 INFO good",
        "this line has no timestamp at all",
        "2026-01-01T00:00:05 INFO also good",
    });
    LogIndex idx(path);
    EXPECT_EQ(idx.malformed_line_count(), 1u);
    auto results = idx.query_range(parse_ts("2026-01-01T00:00:00"), parse_ts("2026-01-01T00:00:10"));
    EXPECT_EQ(results.size(), 2u);
}

TEST(LogIndexer, NonExistentFileIsAClearError) {
    EXPECT_THROW(LogIndex("/path/does/not/exist.log"), std::runtime_error);
}

TEST(LogIndexer, LineContentIsNonOwningView) {
    auto path = write_temp_log({"2026-01-01T00:00:00 INFO hello world"});
    LogIndex idx(path);
    auto results = idx.query_range(parse_ts("2026-01-01T00:00:00"), parse_ts("2026-01-01T00:00:10"));
    static_assert(std::is_same_v<decltype(results[0]), std::string_view>);
}
```

## Large-File / Manual Verification (documented, not part of fast CI)

- A generator script producing a synthetic ~1 GB log file with a documented, reproducible line-count and timestamp distribution.
- Index-build time and peak RSS recorded for this file and for a 10x-smaller file, showing memory does not scale linearly with file size.
- Cross-check: for at least 5 randomly chosen timestamp ranges, compare your tool's output against a naive linear-scan reference (or `awk`) over the same file, confirming identical results.

## Hidden Tests

- correctness when the file's lines are wildly uneven in length (some very short, some very long) — checking whether index memory usage tracks line count or total content size
- correctness when timestamps are not monotonically increasing in the file — behavior should match whatever policy is documented, and the hidden test specifically checks that an out-of-sortedness violation doesn't silently corrupt results if the documented policy claims to handle it
- a query mode's returned collection (or iterator) is checked to confirm it does not materialize all matches into an owning container before the caller can begin consuming them, for a query matching a very large fraction of a large file
- behavior on a file with no trailing newline on its last line
