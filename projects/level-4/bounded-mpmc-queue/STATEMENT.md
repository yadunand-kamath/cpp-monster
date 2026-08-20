# P-4.2 — Bounded MPMC Queue (Lock-Based and Lock-Free)

**Level:** 4 (Systems component) · **Category:** Systems · **Requires:** Ch01–11 · **Est. effort:** L (16-24h)

## Objective

Build a fixed-capacity multi-producer multi-consumer queue in two variants — a straightforward lock-based implementation and a lock-free implementation using atomics — that produce identical observable behavior, with the lock-free variant proven correct under ThreadSanitizer and benchmarked against the lock-based one under realistic contention.

## Functional Requirements

1. Both variants support `try_push`/`push` (blocking or bounded-wait) and `try_pop`/`pop`, with a fixed capacity set at construction.
2. Both variants correctly block (not busy-wait indefinitely burning CPU) a producer when full and a consumer when empty, in their blocking-call forms, while still offering non-blocking `try_` variants that fail fast instead.
3. The lock-free variant must use `std::atomic` operations exclusively for the concurrent path (no internal mutex protecting the core push/pop logic) — document the exact memory ordering (`acquire`/`release`/`relaxed`/`seq_cst`) used for every atomic operation and why that ordering, not a stronger one, is sufficient.
4. Both variants must correctly support multiple concurrent producers and multiple concurrent consumers simultaneously (not just single-producer/single-consumer) — this is a harder correctness bar than the SPSC queue that's a common simpler exercise, and the design must genuinely handle it.
5. Provide a way to signal shutdown (e.g. a `close()`) after which producers can no longer push and consumers drain remaining items then receive a clear "queue closed" signal rather than blocking forever.
6. FIFO ordering is preserved among items pushed by a single producer relative to each other (multi-producer interleaving order across different producers is inherently not deterministic and need not be specified beyond that).

## Input

Values of a caller-specified type `T`, pushed and popped concurrently from multiple threads.

## Output

Correct FIFO-per-producer delivery of pushed values to poppers, with no lost, duplicated, or corrupted values under concurrent access.

## Constraints

- C++20, `<atomic>`. The lock-free variant must be verified under ThreadSanitizer with no reported races across a realistic multi-producer multi-consumer stress test.
- The queue's capacity is fixed at construction (no dynamic resizing) — this is a bounded queue by design, not an unbounded one with a soft limit.
- Must not have a correctness dependency on `T`'s move/copy operations being noexcept unless clearly documented as a stated constraint on `T` (and if so, justify why that constraint is necessary for this specific lock-free design, referencing [Ch03](../../../03-value-categories/CONCEPTS.md)'s `move_if_noexcept` material).

## Edge Cases

- Capacity of 1 — degenerates to a single-slot queue; both variants must still function correctly, including under concurrent access.
- A producer and consumer racing on an empty-vs-full boundary transition (queue going from empty to having exactly one item, or from having one free slot to full) — the classic ABA-adjacent boundary where lock-free queue bugs concentrate.
- `close()` called while producers/consumers are actively blocked waiting — all blocked threads must wake and receive the closed signal, not remain blocked forever.
- `close()` called with items still in the queue — consumers must still be able to drain those remaining items before receiving "queue closed," not have them silently discarded.

## Error Handling

- `push` on a closed queue — a clear, immediate, documented result (error/false/exception, consistently chosen) rather than undefined behavior or a silent no-op.
- `pop` on a closed-and-drained queue — a clear "closed, no more items" result distinguishable from "temporarily empty, try again."

## Acceptance Criteria

- Both variants pass a stress test with multiple producer and consumer threads pushing/popping a large number of items with a checksum or count-based verification that nothing was lost, duplicated, or corrupted.
- The lock-free variant is ThreadSanitizer-clean under that same stress test.
- A benchmark comparing both variants' throughput under at least two contention profiles (e.g. 2 producers/2 consumers, and 8 producers/8 consumers on a machine with enough cores) with the results and interpretation written up — the lock-free variant is not assumed faster; it's measured, and a surprising result (e.g. lock-based winning at high contention due to backoff behavior, or roughly tying) is reported honestly rather than adjusted to match an expected narrative.
- Documented memory-ordering justification for every atomic operation in the lock-free variant.

## Testing Requirements

- The capacity-1 edge case, tested under concurrent access specifically (not just single-threaded).
- The empty/full boundary-transition race, specifically targeted with a high-iteration stress test.
- The close-while-blocked and close-with-items-remaining scenarios.
- The full stress test under ThreadSanitizer for the lock-free variant.
- The throughput benchmark under multiple contention profiles.

## Hints

### Hint 1 — Direction
Build and fully validate the lock-based variant first, even though it's not the interesting half of this project — it gives you a correctness reference (its FIFO/no-lost-items behavior is comparatively easy to reason about) that the lock-free variant's stress tests can be checked against, and it isolates "is my test actually detecting the bugs I care about" from "is my lock-free implementation correct," which are two different questions you don't want conflated while debugging.

### Hint 2 — Technique
A ring buffer (fixed-size array plus head/tail indices) is the natural underlying structure for both variants. For the lock-free version, the head and tail indices (or sequence counters, in Vyukov-queue-style designs) become the atomics that producers and consumers coordinate through — a slot's availability for writing or reading is determined by comparing an atomic sequence number against the current head/tail position, using a compare-exchange loop to claim a slot before writing/reading it.

### Hint 3 — Implementation
For the empty/full boundary races specifically, consider a per-slot state (rather than only a global head/tail pair) that a producer/consumer can atomically transition through (e.g. Empty → Writing → Full → Reading → Empty, via compare-exchange), so a slot's own state — not just the global indices — is the source of truth for whether it's currently safe to write or read; this localizes the hardest part of the correctness reasoning to one slot at a time rather than requiring reasoning about the whole ring buffer's global state simultaneously.

### Hint 4 — Debugging/Design
If ThreadSanitizer reports a race specifically around the capacity-1 or boundary-transition tests, check whether you're using `memory_order_relaxed` somewhere a genuine acquire/release pairing is actually required to establish a happens-before relationship between a producer's write into a slot and a consumer's subsequent read of that same slot's data — a lock-free queue's hardest bugs are almost always a memory-ordering gap that lets a consumer observe a slot marked "ready" before the data written into it is actually visible to that consumer's thread, not a logic error in the index arithmetic itself.
