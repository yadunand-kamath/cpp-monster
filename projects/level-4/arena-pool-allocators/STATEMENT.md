# P-4.4 — Arena & Pool Allocator Suite

**Level:** 4 (Systems component) · **Category:** Systems · **Requires:** Ch01–07,12 · **Est. effort:** M (10-16h)

## Objective

Build a small suite of custom allocators — a bump/arena allocator and a free-list pool allocator — plus `std::pmr`-compatible adapters so both can be dropped into standard containers, with measured allocation-throughput and locality benefits over the default allocator for their respective target workloads.

## Functional Requirements

1. A bump/arena allocator: allocates from a large pre-reserved (or growable-in-chunks) block by simply advancing a pointer, supports resetting the entire arena at once (freeing everything allocated from it in O(1) or O(chunk count)), and does not support freeing individual allocations (documented as a deliberate limitation, not an oversight).
2. A free-list pool allocator: allocates and frees individual fixed-size (or size-classed) blocks efficiently, reusing freed blocks for subsequent allocations of matching size, backed by chunks obtained from the system allocator or from an arena.
3. Both allocators must correctly honor alignment requirements for arbitrary types (including over-aligned types, e.g. SIMD vector types with 32-byte alignment).
4. `std::pmr::memory_resource`-derived adapters for both, so `std::pmr::vector<T>`, `std::pmr::map<K,V>`, etc. can use them directly without modification to those container types.
5. A benchmark suite comparing allocation/deallocation throughput and, where measurable, cache-locality effects (e.g. via a workload sensitive to pointer-chasing locality) against the default global allocator, for a workload shaped to each allocator's intended use case (many same-sized short-lived allocations for the pool; many allocations freed all-at-once for the arena).
6. Both allocators must be usable safely within the workbook's existing sanitizer presets (ASan in particular) — document any sanitizer interaction issues (e.g. ASan's own allocator poisoning potentially needing to be told about custom pool reuse via its allocator-hooks API, if you choose to integrate with it) or explicitly document choosing not to.

## Input

Allocation size and alignment requests, routed either directly or via the `std::pmr` adapter from a standard container.

## Output

Correctly aligned, correctly sized memory blocks; for the pool allocator, blocks returned to the free list on deallocation for reuse.

## Constraints

- C++20, `<memory_resource>`. No use of the default global `new`/`delete` inside the hot allocation/deallocation path of either custom allocator (the whole point is avoiding that path) — the system allocator may still be used to obtain each allocator's backing chunks.
- Must correctly reject (or gracefully fall back, if that's the documented policy) an allocation request the pool allocator's fixed size class cannot satisfy, rather than silently returning an incorrectly-sized block.
- Benchmarks must control for measurement noise appropriately (per [P-1.4](../../level-1/copy-move-harness/STATEMENT.md)'s optimizer-defeat lessons and general sound-benchmarking practice) — no benchmark numbers presented without believable methodology.

## Edge Cases

- An allocation request larger than the pool's configured block size — documented reject-or-fallback behavior, not silent corruption.
- Over-aligned types (e.g. `alignas(32)`) requested through the `std::pmr` adapter — must produce correctly-aligned memory, not merely default-aligned memory that happens to work by accident on some platforms.
- The arena allocator's chunk growing (if implemented as growable rather than fixed-size) while existing pointers into earlier chunks remain valid and must **not** be invalidated by later growth (unlike, e.g., `std::vector`'s reallocation) — document this guarantee explicitly since it materially affects what the arena is safe to be used for.
- Resetting an arena while a `std::pmr` container still holding pointers into it exists — undefined by design (the container would hold dangling pointers), but the documentation must be explicit and loud about this danger given how easy it is to misuse.

## Error Handling

- Out-of-memory when growing an arena/pool chunk — a clear, documented failure mode (exception, per `std::pmr::memory_resource`'s contract of throwing `std::bad_alloc` from `do_allocate` on failure) rather than undefined behavior.
- A deallocation call on the pool allocator for a pointer that didn't originate from it — out of scope to fully defend against (matching real-world allocator behavior), but document this as an explicit, understood constraint on correct usage rather than silence.

## Acceptance Criteria

- Both allocators pass correctness tests including the over-aligned-type case, run cleanly under ASan.
- The `std::pmr::vector`/`std::pmr::map` integration is demonstrated working with both allocators as the backing memory resource.
- The benchmark suite shows measured throughput numbers (not estimates) for both allocators against the default allocator on their target workloads, with methodology described (iteration counts, warm-up, how compiler optimization was prevented from eliding the work).
- The arena's O(1)-reset behavior is demonstrated and measured against equivalent individual-deallocation cost using the default allocator.

## Testing Requirements

- Correctness tests for both allocators: basic allocate/deallocate round-trip, alignment correctness (including over-aligned types), and the pool allocator's fixed-size-class boundary behavior.
- The arena's chunk-growth-does-not-invalidate-existing-pointers guarantee, explicitly tested.
- ASan-clean runs for both allocators' test suites.
- The `std::pmr` container integration tests.
- The benchmark suite, run and its results captured with methodology documented.

## Hints

### Hint 1 — Direction
Both allocators are, at their core, much simpler than the general-purpose allocator they're replacing — that simplicity (no per-allocation bookkeeping to support arbitrary-size individual frees, in the arena's case; no need to search for a best-fit block, in the pool's case, since it only ever deals with one fixed size) is the entire source of their performance advantage for their specific intended workloads. Design each one to exploit that restricted use case fully rather than accidentally re-implementing general-purpose allocation logic.

### Hint 2 — Technique
The bump allocator's `allocate(size, align)` is essentially: round the current bump pointer up to satisfy `align`, check there's enough remaining space in the current chunk (allocate a new chunk if not), return the current pointer, then advance it past the allocated size — no search, no bookkeeping beyond the current position. The pool allocator's `deallocate` for a fixed block size is essentially: treat the freed block's own memory as storage for an intrusive linked-list "next free block" pointer, and push it onto the head of a free list; `allocate` pops from that free list if non-empty, or falls back to bump-allocating a fresh block from its backing chunk otherwise.

### Hint 3 — Implementation
For alignment correctness with arbitrary (including over-aligned) types through the `std::pmr` adapter, remember that `memory_resource::allocate(size, alignment)` receives the *actual* required alignment from the calling container — your `do_allocate` override must actually honor that parameter (rounding your bump pointer up to it, or ensuring your pool's fixed block size/alignment matches or exceeds it) rather than assuming a fixed, hard-coded alignment sufficient only for the types you happened to test with first.

### Hint 4 — Debugging/Design
If ASan reports issues specifically around your pool allocator's freed-and-reused blocks (e.g. a false "use after free" on memory your pool has legitimately reused), investigate ASan's allocator-hooks API (`__asan_poison_memory_region`/`__asan_unpoison_memory_region` or the more targeted quarantine/redzone interaction points) — a custom allocator that recycles memory without telling ASan about the recycling can trigger ASan reporting on memory that is, from your allocator's perspective, legitimately back in active use; document whichever choice you make (integrate with ASan's hooks, or explicitly note the known limitation) rather than silently working around it in a way that could mask a genuine bug.
