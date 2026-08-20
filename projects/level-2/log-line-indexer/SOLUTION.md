# P-2.1 — Solution

## Reference Architecture

Three pieces: a thin cross-platform memory-mapping wrapper, a sparse offset/timestamp index built by one linear pass, and a query layer operating on `string_view`s into the mapped region.

```cpp
class MappedFile {
public:
    explicit MappedFile(const std::filesystem::path& p); // opens + maps; throws on failure
    ~MappedFile();
    MappedFile(const MappedFile&) = delete;
    std::string_view view() const noexcept { return {data_, size_}; }
private:
    const char* data_ = nullptr;
    std::size_t size_ = 0;
#ifdef _WIN32
    void* file_handle_ = nullptr;
    void* mapping_handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

struct IndexEntry { std::int64_t timestamp; std::size_t byte_offset; };

class LogIndex {
public:
    explicit LogIndex(const std::filesystem::path& p) : file_(p) {
        build_index();
    }

    std::vector<std::string_view> query_range(std::int64_t t1, std::int64_t t2) const {
        std::vector<std::string_view> out;
        for_each_in_range(t1, t2, [&](std::string_view line) { out.push_back(line); });
        return out;
    }

    template <typename Fn>
    void for_each_in_range(std::int64_t t1, std::int64_t t2, Fn&& fn) const {
        if (t1 >= t2 || sparse_index_.empty()) return;
        auto it = std::lower_bound(sparse_index_.begin(), sparse_index_.end(), t1,
            [](const IndexEntry& e, std::int64_t t) { return e.timestamp < t; });
        std::size_t scan_from = (it == sparse_index_.begin()) ? 0 : (it - 1)->byte_offset;
        std::string_view data = file_.view();
        std::size_t pos = scan_from;
        while (pos < data.size()) {
            std::size_t nl = data.find('\n', pos);
            std::string_view line = data.substr(pos, (nl == std::string_view::npos ? data.size() : nl) - pos);
            auto [ts, ok] = parse_timestamp(line);
            if (ok) {
                if (ts >= t2) break;
                if (ts >= t1) fn(line);
            } else {
                ++malformed_count_;
            }
            if (nl == std::string_view::npos) break;
            pos = nl + 1;
        }
    }

    std::size_t malformed_line_count() const noexcept { return malformed_count_; }

private:
    void build_index() {
        std::string_view data = file_.view();
        std::size_t pos = 0;
        std::size_t lines_since_checkpoint = 0;
        constexpr std::size_t checkpoint_stride = 4096; // sparse: one entry per N lines
        while (pos < data.size()) {
            std::size_t nl = data.find('\n', pos);
            std::string_view line = data.substr(pos, (nl == std::string_view::npos ? data.size() : nl) - pos);
            auto [ts, ok] = parse_timestamp(line);
            if (ok && lines_since_checkpoint == 0) {
                sparse_index_.push_back({ts, pos});
            }
            if (!ok) ++malformed_count_;
            lines_since_checkpoint = (lines_since_checkpoint + 1) % checkpoint_stride;
            if (nl == std::string_view::npos) break;
            pos = nl + 1;
        }
    }

    MappedFile file_;
    std::vector<IndexEntry> sparse_index_;
    mutable std::size_t malformed_count_ = 0;
};
```

## Design Rationale

**Why memory-mapping over chunked explicit reads?** Memory-mapping delegates paging entirely to the OS's virtual memory subsystem — pages are brought in lazily as touched and evicted under memory pressure exactly as any other mapped memory would be, which is precisely the "bounded working set regardless of file size" property this project requires, without writing an LRU-style chunk cache yourself. The trade-off, worth documenting, is platform divergence (`mmap` vs `CreateFileMapping`/`MapViewOfFile`) and page-fault-driven I/O latency on first touch of each page, which chunked reads with an explicit prefetch strategy could smooth out — but for a self-study project, `mmap`-style access is the more direct application of the "let the OS help you" principle and is far less code than a correct custom paging cache.

**Why a sparse index (one entry per N lines) rather than one entry per line?** An entry per line would make the index itself scale linearly with line *count*, defeating the "index size independent of file size" spirit even though it wouldn't literally violate "no per-line heap-allocated string." A sparse index bounds the index's own memory to `file_size / (average_line_length * checkpoint_stride)`, and the trade-off — a linear scan of up to `checkpoint_stride` lines per query to find the exact boundary — is deliberately made cheap by choosing a stride that keeps the scan window small (thousands of lines is still a fast linear scan over contiguous, already-mapped memory).

**Why does `for_each_in_range` take a callback rather than returning a container?** This is the direct fix for the most common pitfall this project is designed to expose: a `query_range` that returns `std::vector<std::string_view>` is fine for a *view* per element (no per-line allocation), but if a caller expects that vector to stay small, they'll be surprised when a query matching most of a huge file produces a huge vector anyway. The callback-based `for_each_in_range` is the actually memory-bounded primitive — `query_range` is provided as a convenience built on top of it, and the report should be explicit that convenience API still allocates O(matches) `string_view`s (cheap, but not zero, and not O(1) memory for pathological queries).

## Reference Implementation

The snippets above cover the two structurally interesting pieces (mapping wrapper interface, sparse-index build-and-query). Remaining work for a full submission:
1. `MappedFile`'s actual platform-specific bodies (`mmap`/`munmap` on POSIX; `CreateFileMappingW`/`MapViewOfFile`/`UnmapViewOfFile`/`CloseHandle` on Windows).
2. Level-filtered query variants layered on top of `for_each_in_range` with an additional field-parse-and-compare step.
3. A malformed-line policy decision made explicit in code comments/report (this reference counts and continues; a fail-fast variant would throw on first malformed line instead).
4. The synthetic large-file generator script and the peak-RSS measurement harness (platform RSS query: `/proc/self/status` on Linux, `GetProcessMemoryInfo` on Windows).

## Testing Strategy

Beyond the hand-written small-file tests: cross-check against a naive, deliberately simple linear-scan reference implementation (or `awk`) on the same small-to-medium files, to catch a case where the fast-path index logic has an off-by-one or boundary bug the naive path wouldn't share. For the large-file test, treat it as a documented manual/CI-optional step rather than part of the fast suite, given its size and runtime — but still script it reproducibly so it can be rerun when the implementation changes.

## Performance Analysis

Index build is a single O(file size) linear pass. Query cost is O(log(index entries)) for the binary search plus O(checkpoint_stride) for the local scan — effectively O(log n) in practical terms since `checkpoint_stride` is a small constant. Peak memory is dominated by the sparse index (a small, bounded fraction of file size) plus whatever pages the OS has currently resident for the mapped region — which is why memory usage should be observed to grow far sub-linearly with file size, not because of any explicit cap you coded, but because you never forced the whole file into resident memory at once.

## Failure Modes

- Choosing too small a `checkpoint_stride` (index becomes large, approaching per-line cost) or too large (linear-scan window per query becomes slow) without measuring and documenting the trade-off.
- A level-filter query implemented as "materialize the range-filtered results, then filter by level" — doubling the intermediate allocation compared to filtering both conditions in a single pass.
- Assuming file timestamps are sorted without validating it, and silently returning wrong results on an out-of-order file rather than the explicitly documented behavior.

## Extensions

- A persistent on-disk index format so repeated runs against the same log file skip the indexing pass entirely.
- Support for multiple concurrent readers/queries against one mapped file (mapping is inherently shareable; document what, if anything, needs synchronization).
