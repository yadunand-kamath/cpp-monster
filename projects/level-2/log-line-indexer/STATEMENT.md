# P-2.1 — Log Line Indexer

**Level:** 2 (Multi-concept component) · **Category:** Dev Tools · **Requires:** Ch01–04 · **Est. effort:** M (10-16h)

## Objective

Build a tool that indexes a large (up to ~1 GB) log file for fast line-based access and simple field/timestamp queries, without loading the entire file into heap-allocated line objects and without holding more than a bounded, documented amount of memory regardless of file size.

## Functional Requirements

1. Given a path to a log file (one record per line; assume a simple, documented format such as `TIMESTAMP LEVEL message...`), build an index allowing O(log n) or better lookup of "the line starting at or after timestamp T" without scanning the whole file per query.
2. Provide a query mode that returns all lines within a timestamp range `[T1, T2)`, streamed to output rather than materialized entirely in memory.
3. Provide a query mode that filters by log level (e.g. `ERROR`, `WARN`) within a timestamp range.
4. Line access must use `string_view`-style non-owning references into a memory-mapped (or otherwise bulk-read, chunked) view of the file — you must document and justify whichever underlying I/O strategy you choose, but individual line records must not each be a separately heap-allocated `std::string`.
5. The tool must handle a file too large to comfortably fit in the process's available memory as a single read — meaning either memory-mapping (letting the OS manage paging) or explicit chunked reading with a bounded working set; loading the whole file into a single `std::string`/`std::vector<char>` and calling it done does not satisfy this requirement.
6. Malformed lines (unparseable timestamp, missing fields) must be handled per a documented policy (skip-and-count, or fail-fast) — not silently misinterpreted as valid data.

## Input

A path to a log file, plus query parameters (timestamp range, optional level filter) via command-line arguments or a simple query API if built as a library-first tool.

## Output

Matching lines printed to stdout (or returned via an iterator/range-based API if used as a library), plus, at minimum, a count of malformed lines encountered during indexing.

## Constraints

- C++20; must build and run correctly on both the MSVC/Windows toolchain and WSL Clang/GCC, since memory-mapping APIs differ fundamentally between the two (`CreateFileMapping`/`MapViewOfFile` vs `mmap`) — you are expected to either abstract this behind a small platform seam or provide two implementations behind one interface.
- Peak resident memory must stay bounded and roughly independent of file size (document your actual measured bound) — this is the core constraint the whole project exists to exercise.
- No full-file linear scan per query once the index is built; a full scan of the file is only acceptable during the one-time indexing pass.

## Edge Cases

- An empty file, or a file with exactly one line and no trailing newline.
- A file whose lines are wildly uneven in length (some 20 bytes, some 20,000 bytes) — does your index's memory usage scale with line *count* or with total line *content*, and which did you intend?
- A timestamp range query where `T1 > T2`, or where no lines fall in range at all.
- Timestamps that are not monotonically increasing in the file (out-of-order log lines) — decide and document whether your index assumes sortedness (and what happens if that assumption is violated) or handles out-of-order data correctly at some cost.

## Error Handling

- A file that doesn't exist, or can't be opened (permissions) — a clear, documented error, not a crash.
- Malformed lines during indexing — per your documented skip-and-count or fail-fast policy, decided explicitly rather than left as accidental behavior of whatever parsing code you happened to write.
- A query timestamp range entirely outside the file's actual range — a well-defined empty result, not undefined behavior or a crash.

## Acceptance Criteria

- Correctly indexes and queries a synthetically generated ~1 GB log file (a generator script for producing this test file should be part of your submission) within a documented, reasonable time budget for the one-time index build.
- Range and level-filtered queries return exactly the expected lines, verified against a naive independent implementation (e.g. `grep`/`awk` or a simple linear-scan reference) on a smaller sample file.
- Peak memory measured and documented across at least two file sizes differing by 10x, showing memory does not scale linearly with file size (or, if it does to some bounded degree, that degree is explained).
- Builds and runs correctly on both MSVC/Windows and WSL Clang/GCC.

## Testing Requirements

- Correctness tests against a small, hand-constructed log file with known content, covering range queries, level filtering, and malformed-line handling.
- A generated large-file test (documented as a manual/CI-optional step given its size and runtime, rather than a normal fast unit test).
- A memory-usage measurement step (using OS-provided peak-RSS reporting or an equivalent) comparing at least two file sizes.

## Hints

### Hint 1 — Direction
The core tension here is between "fast lookup by timestamp" and "don't load the whole file into memory." Think about what kind of index structure lets you answer "roughly where in the file does timestamp T live" using a data structure whose size scales with the number of lines, not with the total size of their content — and how that's different from indexing the *content* of each line versus indexing *positions* within the file.

### Hint 2 — Technique
For accessing line content without a per-line heap allocation, consider what it means to view the file's bytes as one large contiguous (or chunked-but-still-contiguous-per-chunk) block of memory, and let each "line" be a lightweight view (offset + length) into that block rather than an independently-owned copy — this is the direct application of the `string_view` material from Ch04, applied at a much larger scale than a typical example. For actually getting that contiguous view of a file too large to comfortably read in one call, look into what your OS gives you for treating a file's contents as directly-addressable memory.

### Hint 3 — Implementation
Your index itself doesn't need to store every line's content — it needs to store enough (byte offset) entries, sparsely, that a binary search over the index can narrow "where roughly does timestamp T occur" down to a small window that you then linearly scan for the exact answer, without ever scanning the whole file per query. Think about how sparse that index can be while still bounding the linear-scan window to something small and predictable. For the two-platform memory-mapping requirement, isolate the OS-specific calls behind one small interface (open/map/unmap/get-pointer-and-size) so the bulk of your indexing and query logic is written once against that interface, not duplicated per platform.

### Hint 4 — Debugging/Design
If your peak-memory measurement shows usage scaling linearly with file size despite believing you're using views rather than copies, check whether something in your query path is materializing an intermediate `std::vector<std::string>` (or similar owning collection) of all matching lines before returning/printing them, rather than streaming each match as it's found — a query API that returns "all matches" as a fully-materialized collection defeats the bounded-memory goal even if your indexing phase itself is view-based and memory-mapped correctly.
