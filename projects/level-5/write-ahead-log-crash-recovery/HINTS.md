# P-5.4 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch09's file I/O/durability material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Nail down the on-disk record format on paper before writing any file I/O code: a fixed-width header (magic bytes or a checksum over the header itself, payload length, payload checksum) followed by the payload bytes. Every other requirement in this project — recovery, crash injection, group commit — is built on top of this format being unambiguous and self-describing enough that "is this a complete, valid record" can always be answered by looking only at the bytes present so far.

## Hint 2 — Technique

Structure your internal write path as separately-callable steps (`write_header(...)`, `write_payload(...)`, `fsync_file()`) rather than one monolithic `append` that does all three inline — this isn't just good structure, it's what lets your crash-injection harness call a prefix of those steps and stop, faithfully producing each of the three required torn-write scenarios without needing to actually kill a process.

## Hint 3 — Implementation

A simple, correct group-commit design: `append()` acquires a lock, appends the record's bytes to an in-memory pending buffer, records which "batch generation" this call's data belongs to, and waits on a condition variable. Whichever thread is first to notice the buffer has pending data (either the appending thread itself, checking after a short deliberate delay, or a dedicated background thread) performs the actual file write plus one fsync for the whole accumulated buffer, then increments the batch generation counter and notifies all waiters — each waiter, once woken, checks whether its own batch generation has been committed yet before returning.

## Hint 4 — Debugging/Design

If your crash-injection tests pass individually but a combined "crash, recover, append more, crash again" cycle produces a corrupted or gapped log, check whether your recovery logic correctly identifies the exact byte offset where valid data ends (not just "how many valid records" but "at what file offset does the next append need to start writing") — a recovery that returns the right *records* but the wrong *resume offset* will silently corrupt the log on the very next append after recovery, which only becomes visible once you test the full crash-recover-append-crash-again cycle rather than each crash scenario in isolation.
