# C-1 — Progressive Hints

Use these in order. This capstone is intentionally the least prescriptive project in the workbook — these hints point at architecture-level decision points, not implementation details, and are deliberately spaced further apart than a regular project's hints.

## Hint 1 — Direction

Before writing any code, produce a one-page written comparison of at least two storage-engine architectures (e.g. an LSM-tree and a B+tree) against the specific requirements this capstone states: write amplification, read amplification, how each behaves when the working set exceeds available memory, and how naturally each supports multi-version snapshot reads. Pick one and write down *why*, in terms of these specific tradeoffs, before starting Phase 1 — this write-up becomes the seed of the required design document, and having it before you code prevents a costly architecture change discovered halfway through Phase 3.

## Hint 2 — Technique

Treat Phase 1 (in-memory-correct data structure with a simple on-disk representation) and Phase 2 (crash-safe durability via a WAL) as separable concerns you can build and test independently before integrating: get an in-memory version of your chosen data structure passing the randomized model-based test against `std::map` first, with the simplest possible "write everything to disk on close" persistence, and only then layer in a proper write-ahead log — reusing [P-5.4](../../level-5/write-ahead-log-crash-recovery/STATEMENT.md)'s record-framing and crash-injection-testing approach directly, rather than re-inventing durability from scratch.

## Hint 3 — Implementation

For Phase 3's "working set larger than memory" requirement, the key design question is: what specifically stays in memory at all times, and what is fetched from disk on demand? A common answer is a small in-memory index (pointers/offsets, not the values themselves) plus a bounded cache of recently-accessed data blocks — [P-4.5](../../level-4/concurrent-sharded-cache/STATEMENT.md)'s memory-budget-based eviction design is directly reusable here for that block cache. For snapshot reads without blocking writers, look into MVCC: tag data with a monotonically increasing sequence number, and give each snapshot reader the sequence number that was current when it started, so it can distinguish "data visible as of my snapshot" from "data written after my snapshot began" without needing a lock that would block writers.

## Hint 4 — Debugging/Design

If you find yourself unable to reach even Phase 3's exit bar within a reasonable timeframe, stop and honestly assess: is the blocker a specific, nameable technical problem (e.g. "I can't get the free-list of reclaimable old MVCC versions right"), or is it that the chosen architecture from Hint 1 turned out to be a worse fit than the alternative you didn't pick? The second case is a legitimate, valuable finding for the design document's honest-accounting section — recognizing and documenting an architecture choice that didn't pan out, with specific reasons, demonstrates more engineering judgment than silently grinding through a design fight that a different architecture would have avoided.
