# C-3 — Progressive Hints

Use these in order. This capstone is the most Ch09-dependent project in the workbook — if a hint here references a platform mechanism you don't recognize, that's a signal to revisit Ch09's paired section before continuing, not to search for it fresh.

## Hint 1 — Direction

Write down, before coding, the exact shape of one trace event (what fields it needs: timestamp, thread id, event kind, name/address, nesting depth or parent link) — this shape is shared by both instrumented and sampled events, and settling it early prevents a costly reshape later when Phase 2's sampler needs to feed the same storage format Phase 1's instrumented tracer already committed to.

## Hint 2 — Technique

Give every thread its own ring buffer, written only by that thread, with no cross-thread writes ever — combining/draining across threads happens only at export/analysis time, reading each thread's buffer independently. This sidesteps the need for any synchronization at all on the hot recording path (Hint 2 in the STATEMENT.md), at the cost of needing a merge step later that reconstructs a single time-ordered view across threads using each event's timestamp.

## Hint 3 — Implementation

For DWARF (Linux) or PDB (Windows) symbolization, look for a well-scoped existing library to handle the binary debug-info format parsing itself rather than writing a DWARF/PDB parser from scratch (per the STATEMENT.md's constraint) — your own code's job is the capture pipeline, the address-to-symbol lookup orchestration, and the caching layer around that lookup, not reimplementing a debug-info-format parser that already exists and is genuinely a distinct, large undertaking on its own.

## Hint 4 — Debugging/Design

If Phase 2's Windows stack-walking (`StackWalk64` or equivalent) intermittently produces truncated or wrong call stacks while Linux's mechanism is reliable (or vice versa), suspect a frame-pointer-omission or unwind-info mismatch before suspecting your walking code — both platforms' stack walkers depend on the target binary having been built with correct unwind metadata (frame pointers preserved, or accurate `.pdata`/`.xdata`/CFI records), and an optimized release build without that metadata will produce exactly this kind of intermittent, hard-to-explain stack-walking failure regardless of how correct your walker's own logic is.
