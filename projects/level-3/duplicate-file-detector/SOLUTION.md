# P-3.4 — Solution

## Reference Architecture

```cpp
struct FileRecord {
    std::filesystem::path path;
    std::uintmax_t size;
    std::filesystem::file_time_type mtime;
    ino_t inode_id; // for hard-link detection; platform-abstracted
};

struct ScanStats {
    std::size_t files_discovered = 0;
    std::size_t partial_hashes_computed = 0;
    std::size_t full_hashes_computed = 0;
    std::size_t files_rehashed = 0; // on resume, files skipped because already done
};

template <typename Key, typename KeyFn>
std::vector<std::vector<FileRecord>> group_and_filter_singletons(
        std::vector<FileRecord> candidates, KeyFn key_of) {
    std::unordered_map<Key, std::vector<FileRecord>> groups;
    for (auto& f : candidates) groups[key_of(f)].push_back(std::move(f));
    std::vector<std::vector<FileRecord>> survivors;
    for (auto& [k, v] : groups)
        if (v.size() >= 2) survivors.push_back(std::move(v));
    return survivors;
}
```

The cascade driver, showing each stage only operating on the previous stage's survivors:

```cpp
std::vector<DuplicateGroup> find_duplicates(std::vector<FileRecord> all, ScanStats* stats) {
    auto by_size = group_and_filter_singletons<std::uintmax_t>(
        std::move(all), [](auto& f) { return f.size; });

    std::vector<DuplicateGroup> result;
    for (auto& size_group : by_size) {
        auto by_partial = group_and_filter_singletons<PartialDigest>(
            size_group, [&](auto& f) {
                stats->partial_hashes_computed++;
                return partial_hash(f.path); // e.g. first 64 KB
            });
        for (auto& partial_group : by_partial) {
            auto by_full = group_and_filter_singletons<FullDigest>(
                partial_group, [&](auto& f) {
                    stats->full_hashes_computed++;
                    return streaming_full_hash(f.path); // reads in fixed-size chunks
                });
            for (auto& final_group : by_full)
                result.push_back(DuplicateGroup{std::move(final_group)});
        }
    }
    return result;
}
```

`streaming_full_hash` reads the file in fixed-size chunks (e.g. 1 MB) into a stack or reused heap buffer, feeding each chunk into an incremental SHA-256 context, so peak memory during hashing is bounded by the chunk size regardless of file size — this is the mechanism satisfying the multi-gigabyte-file acceptance criterion.

## Design Rationale

**Why filter out singleton groups after every stage rather than carrying the full candidate list through every stage?** A file with a unique size has, by definition, no possible duplicate — carrying it forward into the (expensive) partial-hash stage would waste work computing a hash nobody will ever compare it against. Dropping singletons immediately after each stage is what makes the cascade's cost actually proportional to "files that might plausibly be duplicates" rather than "all files," which is the entire measured claim the Acceptance Criteria requires evidence for.

**Why hash-map-group-then-filter rather than sorting and scanning for runs?** Both are valid; the hash-map approach was chosen here because file size and hash digests don't have a naturally useful ordering beyond equality comparison, so a sort buys nothing beyond what a hash map's O(1) grouping already provides, and it avoids the additional complexity of a stable-sort-then-adjacent-scan.

**Why append-only records for resumable progress rather than in-place updates to a fixed-size record file?** An in-place update can be caught by a process kill mid-write, leaving a record in an unknown, possibly-corrupted state that must then be specially detected and discarded on resume. An append-only log makes every fully-written record trivially valid (if the process died mid-append, that one incomplete record is simply the last line, which can be detected — e.g. missing its terminating newline or checksum — and safely ignored, while every prior line remains fully trustworthy).

## Reference Implementation

The above covers the cascade's core shape and the streaming-hash memory-boundedness mechanism. Remaining work for the learner:
1. The resumable-state file's concrete format (append-only records with a recognizable terminator, and a fast index — e.g. path→last-known-stage — rebuilt by replaying the log at startup) and the actual skip-if-already-done check integrated into the cascade driver above.
2. Hard-link detection (grouping discovered files by inode identity, reported separately per the documented policy, before they ever enter the content-duplicate cascade) and the symlink handling policy.
3. Concurrency (optional but natural extension): the partial- and full-hash stages are independently parallelizable across files within a group, since each file's hash computation is independent.

## Testing Strategy

Engineer the test corpus deliberately rather than relying on naturally-occurring files: same-size decoy files with deliberately differing early bytes (to prove the partial-hash stage does real filtering work) and deliberately differing late bytes only (to prove the full-hash stage is necessary and not skippable) are both required to actually exercise — not just theoretically justify — the cascade's stages.

## Performance Analysis

Best case (no duplicates, all distinct sizes): O(n) — one size computation per file, nothing more. Worst case (all files identical size, most with distinct early content): O(n) size ops + O(k) partial hashes where k is the same-size group's size + O(m) full hashes where m ≤ k is the same-size-and-prefix group's size. The measured claim in Acceptance Criteria is precisely that m and k are much smaller than n for realistic corpora.

## Failure Modes

- Computing a full hash for every file regardless of cascade results, technically producing correct duplicate groups but failing the entire point of this project.
- Loading multi-gigabyte files fully into memory to hash them, rather than streaming in fixed-size chunks.
- Treating hard links as content duplicates rather than as a distinct, separately-reported category, misleading a user into thinking they'd reclaim space by deleting one when the OS already shares the underlying data.
- An in-place progress-record update scheme that leaves a genuinely ambiguous, hard-to-detect corrupted record after a kill, defeating the resumability guarantee.

## Extensions

- A `--apply` mode (explicitly opt-in, clearly separated from the default report-only behavior) that replaces duplicates with hard links to reclaim disk space, with a dry-run preview required before any destructive action.
- Parallelizing the hash stages across a thread pool (a natural forward reference to [P-4.3](../../level-4/work-stealing-thread-pool/STATEMENT.md)).
