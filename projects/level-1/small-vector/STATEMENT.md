# P-1.3 — `small_vector<T, N>` with Inline Capacity

**Level:** 1 (Focused component) · **Category:** Libraries · **Requires:** Ch01–03 · **Est. effort:** M (8-12h)

## Objective

Build a vector-like container that stores its first `N` elements inline (no heap allocation) and transparently falls back to a heap-allocated buffer only when the element count exceeds `N`, while presenting a `std::vector`-compatible interface for the operations this project requires.

## Functional Requirements

1. `small_vector<T, N>` provides `push_back`, `pop_back`, `size`, `capacity`, `operator[]`, `begin`/`end` (const and non-const), and `clear`.
2. While `size() <= N`, no heap allocation occurs — elements live in an inline buffer that is part of the `small_vector` object itself.
3. When a `push_back` would exceed `N` elements, the container transitions to heap storage, moving (not copying, when the element type permits) all existing elements into the new buffer.
4. Once heap storage has been engaged, growth beyond the current heap capacity must use an amortized growth strategy (e.g. capacity doubling), not a reallocation on every single insertion.
5. The type must correctly support element types that are move-only, non-trivially-copyable, and throw on construction — meaning your inline buffer cannot simply be a `T arr[N]` if `T` has no default constructor, and your transition/growth logic must not leave the container in a corrupted state if a move or copy throws partway through.
6. Iterators obtained before a transition from inline to heap storage (or before a heap reallocation) are correctly documented as invalidated — matching `std::vector`'s own invalidation contract, which this project deliberately mirrors rather than improves on.

## Input

No runtime input format — this is a library component exercised by a GoogleTest suite and a small benchmark/demo program showing the inline-vs-heap transition.

## Output

Correctness demonstrated by GoogleTest assertions on size/capacity/element values/allocation counts (via an instrumented test allocator or a global `operator new`/`delete` counter) at each stage of growth.

## Constraints

- C++20, no exceptions escaping in ways that violate the exception-safety guarantee you choose to implement (see Acceptance Criteria).
- The inline storage must not use placement-`new`-into-raw-bytes tricks that violate strict aliasing or object lifetime rules — this project is a direct, deliberate application of Ch01–02's object-lifetime rules, not an excuse to bypass them.
- `sizeof(small_vector<T,N>)` should be dominated by the inline buffer (`N * sizeof(T)` plus a small fixed overhead for size/capacity/pointer bookkeeping) — document your actual `sizeof` and explain any surprising padding.

## Edge Cases

- `N == 0` (degenerates to always-heap; should still compile and behave correctly, just with no inline benefit).
- Growing past `N`, shrinking back down via `pop_back` below `N` — does the container transition back to inline storage, or stay on the heap once it has moved there? (Both are defensible; `std::vector` itself never shrinks capacity on `pop_back`, and your container should pick one behavior and document it, not leave it unspecified.)
- Copy-constructing or copy-assigning a `small_vector` whose element type is copyable but expensive to copy — does your implementation do the obvious "always copy every element," or does it need to think about capacity at all for a copy?
- Self-assignment (`v = v;`).

## Error Handling

- If an element's move constructor throws during a transition from inline to heap storage (or during heap growth), specify and document the guarantee you provide: is the `small_vector` left in a well-defined-but-possibly-different state (basic guarantee), or do you guarantee the original elements are untouched if the operation fails (strong guarantee)? `std::vector` provides the strong guarantee only when the element type's move constructor is `noexcept` (falling back to copying otherwise) — decide whether you replicate this `move_if_noexcept`-style behavior or document a simpler, weaker guarantee instead.
- Bounds-checking on `operator[]` is intentionally *not* required (matching `std::vector::operator[]`) — but you must provide some bounds-checked accessor (`at()`) that throws on out-of-range access, and document which is which.

## Acceptance Criteria

- Passes a GoogleTest suite covering inline-only use (never allocates), the inline-to-heap transition (allocates exactly once at the transition), heap growth (amortized, not once-per-push), move-only element types, and self-assignment.
- An allocation-counting test proves zero heap allocations occur for any sequence of operations that never exceeds `N` elements.
- Builds cleanly under `/W4 /permissive-`.
- Documented (in your submission notes) exception-safety guarantee, and a test demonstrating it holds under a throwing move/copy constructor (a test-only type that throws on the k-th construction is the standard technique here).

## Testing Requirements

- Allocation-count tests at each of: below-N, exactly-N, N+1 (first heap transition), and multiple-growths-past-heap-capacity.
- A move-only element type test (e.g. wrapping `std::unique_ptr<int>`).
- A throwing-constructor stress test exercising your documented exception-safety guarantee.
- Iterator-invalidation documentation cross-checked against an actual test that intentionally holds an iterator across a transition and documents (via comment, not a runtime assertion, since using an invalidated iterator is UB) why it must not be dereferenced afterward.

## Hints

### Hint 1 — Direction
You need a piece of storage that can hold up to `N` fully-constructed `T` objects without requiring `T` to be default-constructible — which rules out a plain array member `T storage[N]` for any `T` lacking a default constructor. Think about what kind of raw storage lets you control exactly when and how each `T` object is constructed and destroyed within it, independent of the storage's own lifetime.

### Hint 2 — Technique
Consider representing your inline buffer as uninitialized storage (something that reserves the right size and alignment for `N` `T` objects but does not itself construct any of them), and using placement construction/explicit destruction to manage the objects living inside it — this is exactly the "separate the storage's lifetime from the objects' lifetime" idea from Ch01–02. For the inline-vs-heap decision, think about what single piece of state your container needs to track to know, at any point, whether `data()` should currently point into the inline buffer or into a separately allocated one.

### Hint 3 — Implementation
For the transition itself: when growing past `N` (or past current heap capacity), you need to move-construct each existing element into the new storage, destroy the old objects, and only then update your internal "where is my data" pointer/state — think carefully about ordering if any of those moves could throw partway through, since that's exactly the scenario your exception-safety guarantee needs to handle. For deciding move vs. copy during this transition, recall that `std::vector` doesn't always move — it only moves when doing so can't compromise the exception guarantee it promises; the same reasoning is relevant here.

### Hint 4 — Debugging/Design
If your allocation-count test unexpectedly shows an allocation happening below `N` elements, check whether your `push_back` implementation is unconditionally going through a "grow" code path rather than checking against the *current* effective capacity (which starts at `N`, not at some other default). If a throwing-constructor test leaves your container in a state where element count and actual constructed-object count disagree (a common symptom: a destructor later double-destroys or under-destroys), check that you're only counting an element as "constructed" (and thus something your destructor must clean up) at the exact point construction succeeds, not before — a strong-guarantee implementation typically constructs everything in the *new* storage first and only destroys the *old* storage after every new construction has succeeded.
