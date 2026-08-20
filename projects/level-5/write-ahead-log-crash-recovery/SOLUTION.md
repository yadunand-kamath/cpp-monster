# P-5.4 — Solution

## Reference Architecture

The record format and recovery loop, showing Hint 1's "repeatedly try to parse one record" design:

```cpp
struct RecordHeader {
    std::uint32_t magic;          // distinguishes a real header from leftover/uninitialized bytes
    std::uint32_t header_checksum; // CRC over the other header fields — Hint 4's header-integrity check
    std::uint32_t payload_length;
    std::uint32_t payload_checksum;
};
static constexpr std::uint32_t kMagic = 0x57414C31; // "WAL1"

RecoveryResult WriteAheadLog::recover_detailed(const std::string& path) {
    RecoveryResult result;
    std::ifstream file(path, std::ios::binary);
    if (!file) return result; // missing file — valid empty state, not an error

    std::uint64_t offset = 0;
    while (true) {
        RecordHeader header;
        if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) break; // torn/absent header — stop here
        if (header.magic != kMagic || header.header_checksum != crc32(&header, offsetof(RecordHeader, header_checksum))) {
            result.corruption_detected = true;
            break; // torn or corrupted header — Hint 4's structural detection point
        }
        std::string payload(header.payload_length, '\0');
        if (!file.read(payload.data(), header.payload_length)) {
            result.corruption_detected = true;
            break; // torn payload — fewer bytes present than the header promised
        }
        if (crc32(payload.data(), payload.size()) != header.payload_checksum) {
            result.corruption_detected = true;
            break; // payload present but content doesn't match its checksum
        }
        result.records.push_back(std::move(payload));
        offset = file.tellg(); // exact resume point — Hint 4's "wrong resume offset" pitfall
    }
    result.valid_tail_offset = offset;
    return result;
}
```

Group commit, showing Hint 3's batch-generation waiting technique:

```cpp
LogPosition WriteAheadLog::append(std::string_view record) {
    std::unique_lock lock(mutex_);
    LogPosition position = next_position_;
    pending_buffer_.push_back(encode_record(record));
    next_position_ += encoded_size(record);
    std::uint64_t my_generation = current_generation_;

    if (!commit_in_progress_) {
        commit_in_progress_ = true;
        auto batch = std::move(pending_buffer_);
        pending_buffer_.clear();
        lock.unlock();
        write_batch_to_file(batch); // Hint 2's separately-callable step
        fsync_file();              // Hint 2's separately-callable step
        lock.lock();
        current_generation_++;
        commit_in_progress_ = false;
        commit_cv_.notify_all();
    } else {
        commit_cv_.wait(lock, [&] { return current_generation_ > my_generation; });
    }
    return position;
}
```

## Design Rationale

**Why does the header itself carry a checksum, separate from the payload's checksum?** A torn write can leave a file with a header that reads as structurally plausible (right size, non-garbage-looking length field) purely by coincidence of whatever bytes happened to already be on disk — without a header checksum, recovery might trust a torn header's `payload_length` field and attempt to read that many payload bytes, potentially reading past the actual end of written data or into leftover bytes from a previous write cycle. Checksumming the header lets recovery reject a torn header immediately, before it can mislead the payload-reading step at all — exactly Hint 4's point.

**Why does group commit track a "batch generation" counter rather than, say, giving each append its own individual completion flag?** A single monotonically increasing generation counter lets every waiter's wake-check be a simple, uniform comparison (`current_generation_ > my_generation`) regardless of how many other appends were batched alongside it — avoiding a per-append data structure (a flag, a future, whatever) that would need to be individually allocated and cleaned up for every single append, which would undercut some of the throughput benefit group commit exists to provide.

**Why does `recover_detailed` return the exact byte offset (`valid_tail_offset`) where valid data ends, not just the list of valid records?** As Hint 4 warns, resuming appends at the wrong offset after recovery — even if every previously-recovered record was correctly identified — would corrupt the log on the very next write, because that write would land at the wrong file position relative to whatever torn bytes were left behind by the crash. This offset is the actual load-bearing piece of recovery's output for continued operation, even though the record list is what most tests directly assert on.

## Reference Implementation

The above covers the record format, the recovery loop's core stop conditions, and group commit's waiting mechanism. Remaining work for the learner: `encode_record`/CRC computation helpers, the three `inject_crash_after_*` methods (each simply calling a prefix of the separated write steps from Hint 2 and stopping before the rest), resuming append position correctly after `recover_detailed` on startup, and the durability-contract test confirming exactly what `append()` alone promises versus what `flush()` additionally guarantees.

## Testing Strategy

Write the crash-injection methods before writing any of the three crash-scenario tests that depend on them, and verify each injection method in isolation (e.g. confirm `inject_crash_after_partial_header_write(2)` actually leaves exactly 2 header bytes on disk with nothing else) before trusting the recovery-side assertions built on top of it — a subtly-wrong crash-injection helper would make every recovery test built on it meaningless regardless of how correct the recovery logic itself is.

## Performance Analysis

Group commit's benefit is fundamentally an amortization argument: fsync's fixed per-call cost (dominated by physical storage flush latency, largely independent of how much data was written) is paid once per batch rather than once per append, so throughput scaling with concurrent append rate should show a clear knee where group-commit's advantage grows as more concurrent appends arrive within each batching window — this shape, not just a single throughput number, is the most informative thing to measure and report.

## Failure Modes

- Checksumming only the payload and not the header, leaving a torn header free to mislead the payload-read step into an incorrect or out-of-bounds read attempt before any checksum has a chance to catch the problem.
- Recovery correctly identifying valid records but returning or using the wrong resume offset for continued appending, corrupting the log on the very next append after a restart — invisible unless a full crash-recover-append-crash-again cycle is specifically tested.
- A group-commit implementation where a slow storage device's fsync latency causes commit batches to grow unboundedly large under sustained high append concurrency, without any documented cap — worth at least noting as a tradeoff even if not fully solved in this project's scope.

## Extensions

- Log segmentation (rolling to a new file past a size threshold) as a natural precursor to the explicitly-out-of-scope compaction/truncation concern.
- Feeding this WAL's durability primitive into [C-1](../../capstones/embeddable-persistent-kv-store/STATEMENT.md)'s persistent key-value store as its recovery mechanism.
