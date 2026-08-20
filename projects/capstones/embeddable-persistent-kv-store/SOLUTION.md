# C-1 — Solution

This capstone deliberately has no single canonical solution — the architecture choice itself is part of the assessment. This document sketches one credible reference path (an LSM-tree) at the design-and-key-snippets depth used throughout this workbook's `SOLUTION.md` files, and explicitly discusses the B+tree alternative it did not take.

## Reference Architecture

### Chosen design: LSM-tree with a WAL, in-memory memtable, and sorted on-disk SSTables

```
put/delete → WAL (durability, Hint 2) → memtable (in-memory sorted map)
                                              │
                              memtable full? │ flush
                                              ▼
                                   immutable SSTable on disk (sorted key/value runs + sparse index)
                                              │
                                  background  │ compaction (merge overlapping SSTables, drop tombstones/old versions)
                                              ▼
                                       fewer, larger SSTables
```

```cpp
class LsmStore {
public:
    void put(std::string_view key, std::string_view value) {
        SequenceNumber seq = next_sequence_.fetch_add(1);
        wal_.append(WalRecord::put(seq, key, value));   // Phase 2: durable before acknowledging
        wal_.flush();
        std::lock_guard lock(memtable_mutex_);
        memtable_.insert_or_assign(seq, key, value);
        if (memtable_.approx_size_bytes() > memtable_flush_threshold_) schedule_flush_locked();
    }

    std::optional<std::string> get(std::string_view key) const {
        return get_as_of(key, current_sequence());  // ordinary reads use "now" as their snapshot
    }

    Snapshot snapshot() const { return Snapshot(this, current_sequence()); } // Hint 3: MVCC via sequence number
};
```

```cpp
// per-SSTable on-disk layout, sorted by key, written once and never mutated (immutability simplifies concurrency)
struct SsTableFooter {
    uint64_t index_block_offset;
    uint64_t index_block_size;
    uint64_t min_sequence, max_sequence;
    uint32_t footer_checksum;
};
// index block: sparse array of (key, block_offset) pairs — binary search to find the candidate data block,
// then linear scan within that block, bounding memory use regardless of SSTable size (Phase 3 requirement)
```

Snapshot-consistent read across memtable + all SSTables:

```cpp
std::optional<std::string> LsmStore::get_as_of(std::string_view key, SequenceNumber snapshot_seq) const {
    if (auto v = memtable_.find_visible_at(key, snapshot_seq)) return v;   // newest source first
    for (auto& table : sstables_in_newest_to_oldest_order()) {
        if (auto v = table.find_visible_at(key, snapshot_seq)) return v;  // Hint 3: version ≤ snapshot_seq only
    }
    return std::nullopt;
}
```

## Design Rationale

**Why LSM over B+tree?** An LSM-tree turns random writes into sequential ones (append to WAL, append to memtable, sequential flush to a new SSTable) at the cost of read amplification (a read may have to check the memtable and several SSTables) and periodic compaction overhead. A B+tree gives more predictable single-tree read latency and in-place update simplicity, at the cost of random-write I/O patterns and more complex crash-consistency for in-place page mutation (typically requiring its own separate WAL or shadow-paging scheme just to make in-place updates crash-safe at all). Given this project's explicit crash-recovery and durability emphasis (Phase 2's exit bar), the LSM-tree's naturally append-only, already-WAL-shaped write path was judged the lower-risk path to a correct crash-recovery story — the tradeoff accepted is read amplification, mitigated by a per-SSTable sparse index and (optionally) a bloom filter per table to skip tables that provably don't contain a key.

**Why immutable SSTables?** Once written, an SSTable is never mutated in place — only replaced wholesale by compaction. This sidesteps an entire category of concurrent-mutation correctness problems: a reader holding a reference to an SSTable never has to worry about its contents changing underneath it, which is what makes Phase 3's snapshot-consistency requirement tractable without pervasive locking.

**Why sequence numbers for MVCC rather than a global reader-writer lock?** A lock protecting "the whole store" during a snapshot read would block writers for the read's entire duration, defeating the purpose of supporting concurrent readers and writers (Phase 3's functional requirement). Tagging every write with a monotonically increasing sequence number, and letting a snapshot simply remember the sequence number active when it started, lets readers and writers proceed fully concurrently — the cost is that old versions of overwritten/deleted keys must be retained until no live snapshot could still need them, which requires an explicit reclamation policy (see Failure Modes).

## Reference Implementation

Left to the learner: the on-disk WAL and SSTable binary formats in full byte-level detail (feeding directly from [P-5.4](../../level-5/write-ahead-log-crash-recovery/STATEMENT.md)'s record-framing design); the memtable's concrete data structure (a sorted associative container, e.g. a skip list or a balanced tree, supporting the sequence-number-aware `find_visible_at` lookup); the compaction algorithm (which SSTables to merge, when to trigger it, how it drops keys with no live snapshot referencing their old version); the sparse index and optional bloom filter per SSTable; the background flush/compaction threading model and its interaction with the memtable-mutex-protected foreground write path; and the full recovery procedure (replay WAL records not yet flushed into a durable SSTable at last clean shutdown, or, on crash, not yet flushed at all).

## Testing Strategy

Build Phase 1's data structure and its model-based correctness test entirely in memory first, with the simplest possible "serialize everything on close" persistence — this isolates and de-risks the core data-structure correctness question before durability (Phase 2) and concurrency (Phase 3) compound the number of things that could be wrong at once. Layer in the WAL next, reusing P-5.4's crash-injection technique (separately callable write steps: `write_wal_record`, `fsync_wal`) directly against this store's actual write path. Only once Phase 1 and 2 are independently solid should the memtable-mutex and multi-SSTable read path be made genuinely concurrent and stress-tested — introducing concurrency before the sequential logic is trusted makes every subsequent bug ambiguous between "logic bug" and "race."

## Performance Analysis

An LSM-tree's performance story is dominated by: memtable-flush frequency (larger memtables amortize flush/compaction overhead across more writes, at the cost of a larger crash-recovery replay window and more memory use), compaction strategy (how aggressively old SSTables are merged trades write-amplification against read-amplification and space-amplification), and index density (a denser sparse index trades memory for fewer disk seeks per read). Phase 4's performance report should pick one specific, measurable axis (e.g. sequential-write throughput, or point-lookup p99 latency at a specific working-set size) and show a before/after from a deliberate change along one of these three axes, using [P-5.1](../../level-5/allocator-container-benchmark-harness/STATEMENT.md)'s harness for statistically sound measurement.

## Failure Modes

- Old MVCC versions never reclaimed because no policy exists for "no live snapshot could still need this" — the store's disk usage grows unboundedly even though logically most of that data is dead; a real design needs an explicit "oldest live snapshot sequence number" tracked across all currently-open snapshots, and compaction must only drop versions older than that watermark.
- A compaction that merges SSTables while a snapshot reader is mid-iteration over the *old* (pre-compaction) SSTables, if those SSTables are deleted from disk immediately rather than being kept alive (e.g. via a reference count) until no reader still references them.
- Recovery replaying WAL records past the point the memtable was already flushed to a durable SSTable, silently re-adding stale data — the WAL truncation/checkpoint point after a successful flush must be tracked and honored during recovery, mirroring P-5.4's exact-resume-offset lesson.
- Treating memtable-flush and WAL-fsync as independent events without a clear ordering guarantee between them, risking a state where an SSTable exists on disk but the WAL records it was built from were never confirmed durable (or vice versa) — the flush/checkpoint protocol must define this ordering precisely.

## Extensions

- Bloom filters per SSTable, to skip provably-absent-key SSTables during lookup, directly reducing the read-amplification cost accepted in the Design Rationale above.
- A leveled or tiered compaction strategy (rather than a single-tier "merge everything" policy) for more predictable read/write/space amplification tradeoffs at scale.
- Range queries and prefix iteration built on top of the already-sorted SSTable and memtable structure.
