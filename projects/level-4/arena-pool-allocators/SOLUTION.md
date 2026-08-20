# P-4.4 — Solution

## Reference Architecture

```cpp
class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t initial_chunk_size) { grow(initial_chunk_size); }

    void* allocate(std::size_t size, std::size_t align) {
        auto aligned = align_up(current_, align);
        if (aligned + size > chunk_end_) {
            grow(std::max(size + align, default_growth_size_));
            aligned = align_up(current_, align);
        }
        current_ = aligned + size;
        return reinterpret_cast<void*>(aligned);
    }

    void reset() { chunks_after_first_.clear(); current_ = first_chunk_start_; chunk_end_ = first_chunk_end_; }
private:
    void grow(std::size_t size) {
        auto chunk = std::make_unique<unsigned char[]>(size);
        current_ = reinterpret_cast<std::uintptr_t>(chunk.get());
        chunk_end_ = current_ + size;
        chunks_after_first_.push_back(std::move(chunk)); // old chunks kept alive, never freed until reset
    }
    static std::uintptr_t align_up(std::uintptr_t p, std::size_t a) { return (p + a - 1) & ~(a - 1); }
    std::uintptr_t current_ = 0, chunk_end_ = 0, first_chunk_start_ = 0, first_chunk_end_ = 0;
    std::vector<std::unique_ptr<unsigned char[]>> chunks_after_first_;
};
```

Pool allocator, showing the intrusive free-list technique from Hint 2:

```cpp
class PoolAllocator {
public:
    PoolAllocator(std::size_t block_size, std::size_t block_align)
        : block_size_(std::max(block_size, sizeof(void*))), align_(block_align) {}

    void* allocate() {
        if (free_list_) {
            void* block = free_list_;
            free_list_ = *reinterpret_cast<void**>(free_list_); // pop: read the intrusive next-pointer
            return block;
        }
        return arena_.allocate(block_size_, align_); // fall back to bump allocation for a fresh block
    }

    void deallocate(void* p) {
        *reinterpret_cast<void**>(p) = free_list_; // store this block's own memory as the next-pointer
        free_list_ = p;
    }
private:
    ArenaAllocator arena_{4096}; // backing chunks, reused rather than reimplemented
    std::size_t block_size_, align_;
    void* free_list_ = nullptr;
};
```

The `std::pmr` adapter, showing alignment plumbed through per Hint 3:

```cpp
class ArenaMemoryResource : public std::pmr::memory_resource {
public:
    explicit ArenaMemoryResource(std::size_t initial_size) : arena_(initial_size) {}
private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        return arena_.allocate(bytes, alignment); // alignment forwarded, not hard-coded
    }
    void do_deallocate(void*, std::size_t, std::size_t) override {} // arena: individual free is a no-op by design
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
    ArenaAllocator arena_;
};
```

## Design Rationale

**Why does `ArenaAllocator::grow` keep old chunks alive in a vector rather than deallocating/reallocating a single larger buffer?** Reallocating a single growable buffer (like `std::vector` does) would invalidate every pointer previously handed out from the old buffer — exactly the guarantee the Edge Cases section forbids. Keeping every chunk ever allocated alive (until an explicit `reset()`) means growth only ever *adds* new memory, never moves or frees existing memory, which is what makes "existing pointers stay valid across growth" a true, load-bearing guarantee rather than an accident of implementation.

**Why does the pool allocator store its free-list "next" pointer inside the freed block's own memory rather than in a separate side structure?** A freed block is, by definition, not holding any live data the caller cares about anymore — using that otherwise-wasted memory to store bookkeeping costs nothing extra and requires no separate allocation of its own (which would be self-defeating for an allocator whose entire point is avoiding extra allocation overhead). This intrusive technique is why `deallocate`/`allocate` are both O(1) with no separate data structure to maintain.

**Why does `ArenaMemoryResource::do_deallocate` do nothing rather than asserting or being deleted?** `std::pmr::memory_resource`'s interface requires `do_deallocate` to exist and be callable — containers using this resource will call it during normal operation (e.g. when a `std::pmr::vector` reallocates internally, or is destroyed). Since the arena's whole design deliberately doesn't support individual frees, a no-op is the correct, documented behavior: memory is genuinely reclaimed only via `reset()`, and every individual "free" a container performs is simply absorbed without effect until then.

## Reference Implementation

The above covers both allocators' core allocate/free-list logic and the `std::pmr` adapter shape for the arena. Remaining work for the learner: the `PoolMemoryResource` adapter (mirroring `ArenaMemoryResource` but with `do_deallocate` actually pushing onto the pool's free list, and `do_allocate` validating the requested size/alignment against the pool's fixed configuration per the documented reject-or-fallback policy), the benchmark suite's actual measurement code (with `volatile`/`std::atomic_signal_fence`-style optimizer-defeat techniques per [P-1.4](../../level-1/copy-move-harness/STATEMENT.md)), and the ASan allocator-hooks integration decision from Hint 4.

## Testing Strategy

Test alignment correctness with a deliberately over-aligned type (e.g. `alignas(32) struct Vec4 { float x[4]; };`) rather than only default-aligned primitives — an allocator that happens to pass tests using only naturally-aligned types can still be silently broken for SIMD-style types, which is exactly the gap the over-aligned test cases exist to close.

## Performance Analysis

The arena's `allocate` is O(1) amortized (occasionally O(chunk size) when growth is triggered); `reset()` is O(number of chunks), not O(number of individual allocations) — a critical distinction from the default allocator's O(n) individual-`delete` cost for the same workload, and the primary measured claim this project's benchmark exists to substantiate. The pool's `allocate`/`deallocate` are both O(1) unconditionally once at least one block has been freed and returned to the list.

## Failure Modes

- Reallocating a single growable arena buffer instead of accumulating chunks, silently violating the pointer-stability guarantee under growth.
- Hard-coding a specific alignment value in either allocator instead of honoring the alignment parameter actually passed through `std::pmr::memory_resource::allocate`, breaking for over-aligned types while appearing to work for ordinary ones.
- Treating ASan false positives on legitimately-reused pool memory as a sign of a real bug (or, worse, silently disabling ASan for this code) rather than understanding and addressing the actual allocator-hooks interaction.

## Extensions

- A size-classed pool allocator (several fixed-size pools selected by nearest-fit) generalizing beyond one fixed block size while retaining O(1) allocate/free.
- Feeding this suite's throughput and locality benchmarks directly into [P-5.1](../../level-5/allocator-container-benchmark-harness/STATEMENT.md)'s broader benchmark harness.
