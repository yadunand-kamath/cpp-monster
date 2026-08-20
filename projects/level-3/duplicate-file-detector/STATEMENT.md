# P-3.4 — Content-Addressed Duplicate File Detector

**Level:** 3 (Realistic utility) · **Category:** Dev Tools · **Requires:** Ch01–09 · **Est. effort:** L (16-24h)

## Objective

Build a tool that finds duplicate files across a large directory tree (millions of files, terabytes of data) using a cascade of cheap-to-expensive comparisons rather than hashing every byte of every file up front, and that can resume a scan interrupted partway through without restarting from scratch.

## Functional Requirements

1. Walk a directory tree and group files by size first (files of different sizes cannot be duplicates — the cheapest possible filter).
2. Within each size group, compute a cheap partial hash (e.g. first N KB) to further split groups before committing to a full-file hash — full-content hashing should only happen for files that survive both the size and partial-hash filters.
3. Compute a full cryptographic-strength content hash (e.g. SHA-256, or a documented equivalent) only for files that reach that stage, and group files by that hash to produce final duplicate sets.
4. Persist enough progress state (e.g. to a local database file or structured log) that an interrupted scan can resume without re-hashing files already processed in a previous run.
5. Report duplicate groups with enough information (paths, size, hash) to act on them, without performing any destructive action (deletion/linking) automatically — surfacing candidates is this tool's job, not removing files.
6. Handle symbolic links and hard links correctly and explicitly (documented policy: e.g. hard links to the same inode are not "duplicates" in the interesting sense and should be identified and reported separately from true content duplicates).

## Input

One or more root directory paths to scan, plus an optional path to a previous progress/state file to resume from.

## Output

A report of duplicate file groups (grouped by content hash), plus a persisted state file capturing scan progress.

## Constraints

- C++20. Must scale to millions of files without loading the entire file list or all file hashes into memory simultaneously in a way that would exhaust available RAM — bounded working-set memory usage is required, similar in spirit to P-2.1's bounded-memory requirement.
- Must not silently occupy unbounded disk I/O bandwidth in a way that would make a machine unusable during a scan — a configurable throttle or at minimum a documented single-threaded-by-default posture is acceptable, but this must be a stated design decision, not an oversight.
- The cascade must be demonstrably cheaper than "hash every file fully" for a realistic corpus containing many same-sized-but-different-content files (a naive full-hash-everything approach is not merely undesirable style but a documented failure to meet this project's actual point).

## Edge Cases

- Two files of identical size and identical first-N-KB but differing content later in the file — must not be falsely reported as duplicates (the cascade must actually complete to the full hash before concluding duplication).
- Zero-byte files — all zero-byte files are trivially "duplicates" of each other; decide and document whether this degenerate case is included in the report or explicitly excluded as uninteresting.
- A file that is modified (content changes) between the scan discovering it and the scan actually hashing it — document the tool's consistency guarantee (or explicit lack thereof) for this race.
- A scan interrupted (process killed) mid-run, then resumed — must not re-hash already-processed files, and must not report a corrupted/partial result as if it were complete.
- Hard links vs symbolic links vs genuinely independent files with identical content — three distinct cases requiring three distinct, documented behaviors.

## Error Handling

- A file that becomes inaccessible (permissions, deleted mid-scan) after being discovered by the directory walk but before being hashed — logged and skipped, not a fatal error terminating the whole scan.
- A corrupted/unreadable progress-state file on resume — a clear error recommending a fresh scan, not a crash or silently-wrong resume.
- Running out of disk space while writing progress state — detected and reported, not silently losing progress data.

## Acceptance Criteria

- Correctly identifies duplicate groups in a test corpus containing: genuine duplicates, same-size-different-content decoys, hard links, symbolic links, and at least one multi-gigabyte file (to prove the tool doesn't require loading whole files into memory to hash them).
- Demonstrates, with measured evidence (e.g. a count of full-hash operations actually performed vs the total file count), that the cascade meaningfully reduces expensive full-file hashing on a corpus engineered to contain many same-sized decoys.
- Demonstrates successful resume: a scan is killed partway through (e.g. via a test hook or signal) and a second invocation completes without redoing already-hashed work, verified by comparing elapsed time or explicit progress counters against a from-scratch run.
- Peak memory usage is measured and shown to scale sub-linearly with (or at least remain bounded independent of) total file count for a large synthetic corpus, not merely asserted.

## Testing Requirements

- Unit tests for each cascade stage in isolation (size grouping, partial-hash grouping, full-hash grouping) including the same-size-different-content decoy case.
- The hard-link-vs-symlink-vs-independent-duplicate distinction, each with its own test.
- The resume-after-interruption test with a concrete before/after work-avoided measurement.
- A large-corpus (thousands to tens of thousands of files, synthetically generated for CI feasibility) test measuring both cascade effectiveness and peak memory.

## Hints

### Hint 1 — Direction
The core insight this project is built around is that hashing is the most expensive operation in this pipeline, and most files can be proven "definitely not duplicates of each other" using much cheaper signals (size, then a small content prefix) before ever needing a full hash — think of the pipeline as progressively expensive filters, each one only running on the survivors of the previous, cheaper filter.

### Hint 2 — Technique
A natural data structure for each cascade stage is a hash map from "the cheap signal" (file size; then the partial-hash digest) to a list of candidate file paths sharing that signal — a group of size 1 at any stage can be immediately discarded (nothing to compare it against), and only groups of size ≥2 need to proceed to the next, more expensive stage.

### Hint 3 — Implementation
For resumable progress, consider what the minimum durable record is that lets you skip redoing work: for each file, its path, size, mtime (to detect if it changed since last recorded), and whichever cascade stage's result you've already computed for it. A simple append-only log or a small embedded key-value/SQLite-style store both work; the key design question is making sure a partially-written record (from a process killed mid-write) doesn't get treated as valid on resume — consider what "commit" means for a single record's durability.

### Hint 4 — Debugging/Design
If your measured full-hash count doesn't drop meaningfully compared to a naive full-hash-everything baseline on your test corpus, check whether your test corpus actually contains enough same-sized files with genuinely different early content — a cascade that's implemented correctly will show no benefit if the corpus doesn't create same-size groups in the first place, so make sure your engineered decoy files are actually exercising the filter you're trying to prove works.
