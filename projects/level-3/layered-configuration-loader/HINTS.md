# P-3.6 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch11's atomics/synchronization material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Keep "reading a source" and "merging sources" as two entirely unrelated pieces of code. Each source-reader's only job is to produce a small map of the keys *that source actually sets* — nothing more. The merge step then never needs to know or care whether a given map came from a file, an environment scan, or a CLI parse result; it just folds several such maps together by precedence. This separation is exactly why the Constraints section can require the merge logic to be tested without any real file or environment.

## Hint 2 — Technique

Because "a key is absent from this source's map" already and unambiguously means "unset, fall through," you don't need `std::optional`-wrapped values inside each source's map — a key's mere presence in the map is the signal. The merge is then just: for each declared schema key, walk the layers from lowest to highest precedence, remembering the last (highest-precedence) layer that actually contains the key.

## Hint 3 — Implementation

For the atomic hot-reload publish, build the entire new configuration (re-read file, re-merge with cached env/CLI/defaults, re-validate) as a fresh, fully-formed object before touching anything readers can see — only if that validation succeeds do you publish it, via a single atomic pointer swap (`std::atomic<std::shared_ptr<const Config>>::store`, or `std::atomic_exchange` on a raw pointer with appropriate lifetime management) so any concurrent reader's `load()` always returns either the complete old configuration or the complete new one.

## Hint 4 — Debugging/Design

If ThreadSanitizer flags a race (or you observe inconsistent field values under concurrent reads without a sanitizer catching it, which is worse), look for any code path that updates the *current* configuration's fields in place rather than swapping in a wholly new object — this includes a config struct that reload logic mutates member-by-member even under a mutex, since a reader that took a reference to that same struct before the mutex was acquired can still observe a torn mix of field values as the reload writes proceed. The fix is always the same: never mutate the object readers can see; publish a new one atomically instead.
