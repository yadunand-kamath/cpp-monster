# P-2.1 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch04's `string_view`/iterator material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

The core tension here is between "fast lookup by timestamp" and "don't load the whole file into memory." Think about what kind of index structure lets you answer "roughly where in the file does timestamp T live" using a data structure whose size scales with the number of *lines*, not with the total size of their *content* — and how that's different from indexing the content of each line versus indexing positions within the file.

## Hint 2 — Technique

For accessing line content without a per-line heap allocation, think about what it means to view the file's bytes as one large contiguous (or chunked-but-contiguous-per-chunk) block of memory, and let each "line" be a lightweight view — an offset and a length — into that block, rather than an independently owned copy. This is the `string_view` idea from Ch04 applied at a much larger scale than a typical example. For actually getting that contiguous view of a file too large to comfortably read in a single call, look into what your operating system gives you for treating a file's contents as directly addressable memory without reading the whole thing into a buffer up front.

## Hint 3 — Implementation

Your index doesn't need to store every line's content — it needs a sparse set of entries (timestamp, byte offset) frequent enough that a binary search over the index narrows "where roughly does timestamp T occur" down to a small, bounded window, which you then linearly scan for the exact answer, without ever scanning the whole file per query. Think about the trade-off between index density (memory used by the index itself) and scan-window size (work done per query) — what determines a good balance? For the two-platform requirement, isolate the OS-specific mapping calls behind one small interface (open / map / get pointer-and-size / unmap) so the bulk of your indexing and query logic is written once against that interface, not duplicated per platform.

## Hint 4 — Debugging/Design

If your peak-memory measurement shows usage scaling linearly with file size despite believing you're using views rather than copies, check whether something in your query path is materializing an intermediate owning collection (like a `std::vector<std::string>`) of all matching lines before returning or printing them, rather than streaming each match as it's found — a query API that hands back "all matches" as a fully-materialized collection defeats the bounded-memory goal even if your indexing phase itself is view-based and memory-mapped correctly. Check this specifically for queries that match a large fraction of the file, since a query matching only a few lines might hide the problem.
