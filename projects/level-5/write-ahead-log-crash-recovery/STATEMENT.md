# P-5.4 — Write-Ahead Log with Crash Recovery

**Level:** 5 (Production-style) · **Category:** Storage · **Requires:** Ch01–09,11 · **Est. effort:** L (16-24h)

## Objective

Build a write-ahead log (WAL) — the durability primitive underneath every real database and storage engine — with CRC-checked record framing, group commit for throughput, and a crash-injection test harness that proves recovery actually works against real torn/partial writes, not just against clean shutdowns.

This project is structured in phases. Each phase has its own exit bar; don't move on until the current one is met.

## Phase 1 — Framing and Clean-Shutdown Correctness

Design the on-disk record format and get append/recover working correctly across a clean shutdown (no crash yet). Exit bar: append N records, close cleanly, recover, and confirm all N read back correctly and in order.

## Phase 2 — Crash Injection Harness

Build the mechanism to simulate a crash at a precise point in the write path (mid-header, mid-payload, post-payload-pre-fsync). Exit bar: the harness can deterministically reproduce each of those three torn states on demand, verified by inspecting the resulting file's raw bytes.

## Phase 3 — Recovery Against Torn Writes

Make recovery correctly detect and stop at each torn-write scenario rather than misinterpreting partial data. Exit bar: all three crash-injection scenarios from Phase 2 recover to the documented, correct outcome with zero silent corruption.

## Phase 4 — Group Commit

Add batched fsync for concurrent appends and measure its throughput effect. Exit bar: a benchmark (via P-5.1's harness) shows a measurable throughput improvement with group commit enabled vs. one-fsync-per-append under concurrent load.

## Phase 5 — Contract Verification

Nail down the exact append→durability contract and prove the boundary is exact, not fuzzy, including under concurrent group-committed appends. Exit bar: a test demonstrates that a record acknowledged as committed always survives a crash, and a record not yet acknowledged may legitimately be lost — with no ambiguity about which concurrently-appended records landed in which commit batch.

## Functional Requirements

1. `append(record) -> LogPosition`: appends a record to the log, returning a position that can later be used to read it back; durability semantics (does `append` guarantee the record is on disk before returning, or is that a separate `flush`/`sync` call) must be explicit and documented.
2. CRC-checked record framing: each record is framed with enough metadata (length, checksum) to detect corruption and to detect a torn write (a record that was only partially written before a crash) during recovery.
3. Group commit: multiple concurrent `append` calls arriving close together in time are batched into a single `fsync`/`FlushFileBuffers` call rather than each incurring its own — trading a small amount of added latency per append for substantially higher throughput under concurrent load, a design decision that must be measured, not just claimed.
4. Recovery: on startup, scan the log file from the beginning, replaying valid records and correctly detecting and stopping at the first sign of corruption or a torn write (the tail of the log, if a crash occurred mid-write) rather than treating a torn record as if it were valid data or as if it silently ended the log earlier than it needs to.
5. A crash-injection test harness: a mechanism to simulate a crash at various precise points during a write operation (e.g. after writing N bytes of a record but before completing it, or after writing but before the corresponding fsync) and verify that recovery afterward correctly handles exactly that torn state.
6. Log truncation/compaction is out of scope for this project's core requirement (a real system would need it, but it's a distinct concern) — document this scope boundary explicitly rather than silently ignoring it.

## Input

Records (opaque byte sequences, or a documented structured type) to append; on recovery, the existing log file possibly left in a torn state by a prior crash.

## Output

A durable, appendable log file; on recovery, the fully recovered sequence of valid records plus a clear indication of where the log's valid tail ended (if a torn write was detected and truncated away during recovery).

## Constraints

- C++20. Recovery correctness must be demonstrated against actual simulated torn writes (via the crash-injection harness), not merely against a log file that was cleanly closed — a WAL that only works after clean shutdown provides none of the value a real WAL exists for.
- Group commit's throughput benefit must be measured (concurrent-append throughput with group commit enabled vs. disabled/one-fsync-per-append) using [P-5.1](../allocator-container-benchmark-harness/STATEMENT.md)'s benchmark harness or an equivalent sound methodology.
- CRC (or an equivalent checksum) must actually be verified during recovery, not merely computed and stored — a corrupted record whose checksum doesn't match its content must be detected and treated as the end of valid data.

## Edge Cases

- A crash precisely between writing a record's length/checksum header and writing its payload — recovery must detect this as an incomplete record and stop there, not attempt to interpret whatever partial payload bytes happen to be present.
- A crash precisely after the payload is fully written but before the corresponding fsync commits it to durable storage — depending on the documented durability contract, this record may or may not be considered committed; the behavior must match what was promised by `append`'s documented guarantee, and be tested against that specific promise.
- A record whose CRC happens to accidentally match despite corrupted content (a checksum false negative) — acknowledged as a fundamentally unavoidable probabilistic limitation of any checksum (documented, not solved), while confirming the chosen checksum's collision probability is appropriate for this use case.
- Multiple records appended concurrently by different threads, batched into one group-commit fsync — the returned `LogPosition` for each concurrently-appended record must correctly and unambiguously identify that specific record's location, with no ambiguity about which records were included in which commit batch.
- The log file growing across a process restart — recovery must correctly resume appending after the recovered valid tail, not overwrite it or leave a gap.

## Error Handling

- The log file is missing entirely at startup — treated as a fresh, empty log (a valid initial state), not an error.
- The log file exists but its very first bytes are already corrupted (not even a valid first record) — recovery reports an empty valid sequence and a clear indication of where corruption began, rather than crashing.

## Acceptance Criteria

- The crash-injection harness demonstrates at least three distinct torn-write scenarios (mid-header, mid-payload, post-payload-pre-fsync) each recovering to the documented, correct outcome.
- Group commit's throughput benefit is measured and reported using sound benchmarking methodology, showing a measurable improvement under concurrent append load compared to one-fsync-per-append.
- A CRC mismatch on an otherwise-well-formed-looking record is correctly detected and treated as corruption during recovery.
- The append→durability contract is explicitly documented and a test exists verifying that contract specifically (e.g. if `append` promises durability only after an explicit `flush()`, a crash between `append` and `flush` must be shown to correctly lose that record on recovery, matching the promise).

## Testing Requirements

- Basic correctness: append several records, close cleanly, recover, confirm all records read back correctly and in order.
- The three crash-injection scenarios from Acceptance Criteria, each as its own test.
- The CRC-mismatch-detected-as-corruption test.
- The group-commit throughput benchmark.
- The missing-log-file and corrupted-first-bytes startup edge cases.
- A concurrent-append correctness test confirming `LogPosition`s returned under concurrent group-committed appends are all individually valid and correctly ordered.

## Hints

### Hint 1 — Direction
Design the on-disk record format before writing any code: a fixed-size header (record length, checksum, perhaps a magic number distinguishing "start of a real record" from "uninitialized/leftover bytes") followed by the payload. Recovery is then just "repeatedly try to parse one record's header-then-payload from the current file position; stop at the first point that doesn't parse cleanly," which is a satisfyingly simple loop once the format itself is solid.

### Hint 2 — Technique
To simulate a crash at a precise point without needing to actually crash the process, structure your write path so that each conceptual step (write header, write payload, fsync) is a separately callable, injectable operation — a crash-injection test can then call exactly the first two steps and simply stop, closing the file handle without ever calling the third, faithfully reproducing "the OS has these bytes on disk, but the fsync never happened" without needing real process-kill machinery.

### Hint 3 — Implementation
For group commit, a reasonable design is: an `append` call places its record's bytes into a shared pending-write buffer and blocks on a condition variable; a dedicated writer thread (or the next `append` call to notice the buffer is non-empty and take responsibility) periodically (or triggered by buffer size / a short time window) writes the whole accumulated buffer in one write call, performs one fsync, and then wakes all the blocked `append` callers whose data was included in that batch — the tricky, testable part is correctly tracking which specific callers' data made it into which specific committed batch.

### Hint 4 — Debugging/Design
If your recovery logic doesn't correctly stop at a torn record and instead continues trying to parse further into the file (potentially "recovering" garbage as if it were valid data), check whether your header format includes anything that makes a genuinely torn/partial header distinguishable from coincidentally-plausible-looking leftover bytes — a checksum over the header itself (not just the payload) is one robust way to ensure a torn write is detected at the header-parsing stage rather than only being caught later when the payload's own checksum fails, which could otherwise let a malformed length field drive an incorrect read attempt in between.
