# P-4.4 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch12's allocator/memory-model material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Each allocator in this suite earns its performance by deliberately doing *less* than a general-purpose allocator — the arena gives up individual deallocation entirely in exchange for trivial O(1) bulk allocation and reset; the pool gives up variable block sizing in exchange for a trivial, search-free free-list. Resist the urge to generalize either one beyond its stated restricted use case — that restriction is the feature, not a limitation to work around.

## Hint 2 — Technique

The arena's `allocate` is just pointer arithmetic: round the current position up to the requested alignment, verify the rounded position plus the requested size still fits in the current chunk (if not, obtain a new chunk — from the system allocator — and continue from its start), then return the (now-aligned) position and advance past it. The pool's freed blocks can store their own "next free" pointer inside the freed memory itself (since it's not in use for anything else at that moment) — this intrusive free-list technique needs no separate bookkeeping structure at all.

## Hint 3 — Implementation

`std::pmr::memory_resource::do_allocate(std::size_t bytes, std::size_t alignment)` receives the alignment the calling container actually needs — plumb that value all the way through to your bump-pointer rounding (for the arena) or validate it against your pool's fixed block alignment (for the pool, rejecting or falling back per your documented policy if the pool's configured alignment can't satisfy a stricter request) rather than assuming a alignment value that happened to work for whichever type you first tested with.

## Hint 4 — Debugging/Design

If ASan reports a "use-after-free" or "heap-buffer-overflow" specifically on memory your own pool allocator has legitimately reused, this is very likely ASan's own allocator instrumentation reacting to memory it doesn't know has been recycled through your custom free list rather than through the system allocator's own free — look at ASan's manual poisoning API (`__asan_poison_memory_region`/`__asan_unpoison_memory_region`) to correctly mark blocks as available again after your pool reuses them, and document explicitly whichever approach (integrate with these hooks, or note the known interaction as a documented limitation) you settle on.
