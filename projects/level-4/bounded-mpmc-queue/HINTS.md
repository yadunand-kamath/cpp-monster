# P-4.2 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch11's memory-ordering material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Build the lock-based variant to completion and full confidence first. A mutex-protected ring buffer with two condition variables (one signaled on push for waiting consumers, one signaled on pop for waiting producers) is entirely sufficient and gives you a working, comparatively-easy-to-trust reference implementation — both for correctness comparison and as the "known good" baseline your throughput benchmark will compare the lock-free variant against.

## Hint 2 — Technique

For the lock-free variant, think in terms of a fixed-size array of slots, each with its own atomic state, rather than reasoning about the queue's state as one global thing. A common, well-proven shape (as in Dmitry Vyukov's bounded MPMC queue design) gives each slot a sequence number that producers and consumers compare against their own monotonically-increasing position counters via compare-exchange — a producer claims a slot only when that slot's sequence number indicates it's currently empty and it's this producer's turn; a consumer claims a slot only when the sequence indicates data is present and it's this consumer's turn.

## Hint 3 — Implementation

The trickiest correctness property is making sure a consumer that observes "this slot has data" via an atomic load also reliably observes the *actual data written into that slot*, not a stale or partially-written value — this requires an acquire load on the signal that data is ready, paired with a release store by the producer after writing the data (not before). Get this ordering backwards (or use `relaxed` where `acquire`/`release` is needed) and you'll get real, if intermittent, data corruption that a single-threaded test will never catch and that even a moderately-loaded stress test may only occasionally reveal — which is exactly why ThreadSanitizer, not just a passing stress test, is required for this project's correctness bar.

## Hint 4 — Debugging/Design

If your stress test occasionally observes a duplicated or corrupted value (rather than a clean crash), suspect a memory-ordering gap over a logic bug in the index/sequence arithmetic — lock-free queues built with correct compare-exchange claim logic but insufficient ordering tend to "mostly work," passing light testing while still being genuinely broken, which is a much more dangerous failure shape than an outright crash. Systematically downgrading each atomic operation's ordering one at a time and re-running under ThreadSanitizer (per the Hidden Tests' memory-ordering-downgrade experiment) is a good way to build real confidence about which orderings are load-bearing versus merely conservative.
