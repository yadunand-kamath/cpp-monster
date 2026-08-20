# P-3.2 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(FileWatcher, ReportsFileCreation) {
    TempDir dir;
    FileWatcher w(dir.path());
    EventCollector events;
    w.start([&](const FileEvent& e) { events.push(e); });
    write_file(dir.path() / "a.txt", "hello");
    ASSERT_TRUE(events.wait_for(FileEventKind::Created, "a.txt", 2s));
}

TEST(FileWatcher, ReportsFileModification) {
    TempDir dir;
    write_file(dir.path() / "a.txt", "hello");
    FileWatcher w(dir.path());
    EventCollector events;
    w.start([&](const FileEvent& e) { events.push(e); });
    append_file(dir.path() / "a.txt", " world");
    ASSERT_TRUE(events.wait_for(FileEventKind::Modified, "a.txt", 2s));
}

TEST(FileWatcher, ReportsFileDeletion) {
    TempDir dir;
    write_file(dir.path() / "a.txt", "hello");
    FileWatcher w(dir.path());
    EventCollector events;
    w.start([&](const FileEvent& e) { events.push(e); });
    std::filesystem::remove(dir.path() / "a.txt");
    ASSERT_TRUE(events.wait_for(FileEventKind::Deleted, "a.txt", 2s));
}

TEST(FileWatcher, InDirectoryRenameIsOneLogicalEvent) {
    TempDir dir;
    write_file(dir.path() / "old.txt", "hello");
    FileWatcher w(dir.path());
    EventCollector events;
    w.start([&](const FileEvent& e) { events.push(e); });
    std::filesystem::rename(dir.path() / "old.txt", dir.path() / "new.txt");
    auto ev = events.wait_for_kind(FileEventKind::Renamed, 2s);
    ASSERT_TRUE(ev.has_value());
    EXPECT_EQ(ev->old_path->filename(), "old.txt");
    EXPECT_EQ(ev->path.filename(), "new.txt");
}

TEST(FileWatcher, RapidFireWritesAreCoalesced) {
    TempDir dir;
    FileWatcher w(dir.path(), CoalesceWindow{100ms});
    EventCollector events;
    w.start([&](const FileEvent& e) { events.push(e); });
    for (int i = 0; i < 50; ++i) append_file(dir.path() / "a.txt", "x");
    std::this_thread::sleep_for(500ms);
    EXPECT_LT(events.count_of(FileEventKind::Modified, "a.txt"), 50);
    EXPECT_GE(events.count_of(FileEventKind::Modified, "a.txt"), 1);
}

TEST(FileWatcher, StopUnblocksWaitingThreadPromptly) {
    TempDir dir;
    FileWatcher w(dir.path());
    w.start([](const FileEvent&) {});
    auto start = std::chrono::steady_clock::now();
    w.stop(); // must not hang even with zero filesystem activity
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, 1s);
}

TEST(FileWatcher, WatchedDirectoryDeletedTerminatesCleanly) {
    TempDir dir;
    FileWatcher w(dir.path());
    EventCollector events;
    bool terminated = false;
    w.start([&](const FileEvent& e) { events.push(e); },
             [&](WatchTermination) { terminated = true; });
    std::filesystem::remove_all(dir.path());
    wait_until([&] { return terminated; }, 2s);
    EXPECT_TRUE(terminated);
}

TEST(FileWatcher, NonexistentDirectoryThrowsImmediately) {
    EXPECT_THROW(FileWatcher(std::filesystem::path("/does/not/exist")), std::runtime_error);
}
```

## Hidden Tests

- an out-of-directory move (rename target outside the watched tree) produces the documented fallback event (delete-only, or delete+create per the submission's documented policy) rather than a spuriously invented rename
- a create-then-immediate-delete within the coalescing window produces exactly the documented behavior (either collapsed to nothing, or both events — whichever the submission documents and justifies)
- a high-volume stress test (thousands of rapid file writes into the watched directory) specifically checking that the overflow/missed-events signal is actually delivered to the caller at least once during the stress run, not merely defined in code but never exercised
- a recursive-watch test (if recursive watching is implemented) confirming events in a nested subdirectory are reported with the correct relative/absolute path
- running the clean-shutdown test many times in a loop to catch a shutdown mechanism that "usually" works but occasionally races
