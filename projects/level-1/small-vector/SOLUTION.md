# P-1.3 — Solution

## Reference Architecture

The core idea: raw uninitialized storage for the inline case, a `union`-free approach using `alignas`/byte buffers plus placement-new, and a single `capacity_` field whose comparison against `N` tells you whether `data()` points inline or to a heap block.

```cpp
template <typename T, std::size_t N>
class small_vector {
    alignas(T) unsigned char inline_buf_[sizeof(T) * N];
    T* data_ = reinterpret_cast<T*>(inline_buf_);
    std::size_t size_ = 0;
    std::size_t capacity_ = N;

public:
    small_vector() = default;

    ~small_vector() {
        std::destroy(data_, data_ + size_);
        if (capacity_ > N) ::operator delete(data_);
    }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    T* begin() noexcept { return data_; }
    T* end() noexcept { return data_ + size_; }
    T& operator[](std::size_t i) noexcept { return data_[i]; }

    void push_back(T value) {
        if (size_ == capacity_) grow(capacity_ == 0 ? 1 : capacity_ * 2);
        ::new (static_cast<void*>(data_ + size_)) T(std::move(value));
        ++size_;
    }

    void pop_back() {
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

private:
    void grow(std::size_t new_cap) {
        T* new_data = static_cast<T*>(::operator new(sizeof(T) * new_cap));
        std::size_t constructed = 0;
        try {
            for (; constructed < size_; ++constructed) {
                ::new (static_cast<void*>(new_data + constructed))
                    T(std::move_if_noexcept(data_[constructed]));
            }
        } catch (...) {
            std::destroy(new_data, new_data + constructed);
            ::operator delete(new_data);
            throw; // original storage untouched — strong guarantee preserved
        }
        std::destroy(data_, data_ + size_);
        if (capacity_ > N) ::operator delete(data_);
        data_ = new_data;
        capacity_ = new_cap;
    }
};
```

## Design Rationale

**Why raw byte storage plus placement-new instead of `T inline_buf_[N]`?** An array member of `T` requires `T` to be default-constructible for the array itself to exist, and it constructs all `N` objects immediately — meaning move/copy assignment into "the third slot" would have to go through assignment rather than construction, which requires yet another set of preconditions on `T`. Raw storage plus placement-new means an unused inline slot holds no `T` object at all, matching what `size_` actually claims. This is the direct, deliberate application of Ch01–02's "object lifetime is distinct from storage lifetime" principle — the storage (`inline_buf_`) always exists for the container's whole lifetime, but individual `T` objects within it are constructed and destroyed on demand.

**Why compare `capacity_ > N` instead of a separate `bool using_heap_` flag?** Because it's derivable: capacity starts at exactly `N` (the inline capacity) and only ever changes when `grow()` allocates a new heap block, at which point it becomes the new heap capacity — which by construction of the growth policy (`capacity_ == 0 ? 1 : capacity_ * 2`, starting from `N`) is always strictly greater than `N` once triggered. A separate flag would be redundant state that could, in principle, get out of sync with reality.

**Why `move_if_noexcept` during growth?** This mirrors `std::vector`'s own behavior: if `T`'s move constructor might throw, moving elements into new storage during growth could leave the container in a state where some elements were moved out of the old storage and then the move throws, losing data with no way to recover the strong guarantee. `move_if_noexcept` selects the copy constructor instead whenever the move constructor isn't `noexcept`, at the cost of an extra copy for such types, in exchange for keeping the strong exception guarantee available. Types with a `noexcept` move constructor get the full performance benefit with no compromise.

**Why does `grow()` construct into new storage before destroying the old, rather than move-then-destroy-in-lockstep?** Constructing everything into `new_data` first, and only destroying `data_`'s contents after every new construction has succeeded, is what makes the `catch` block possible — if any construction throws, `data_` (the original storage) has not been touched at all, satisfying the strong guarantee. Destroying old elements as you go would leave the old storage half-destroyed by the time an exception is caught, forcing a weaker guarantee.

## Reference Implementation

The snippets above cover the performance-critical path (`push_back`, `grow`). Remaining work for a complete submission:
1. Copy constructor/assignment (construct-and-swap is the simplest strong-guarantee-preserving approach: build a temporary copy fully, then swap state — if the copy throws partway through, `*this` is untouched).
2. `at()` with bounds checking throwing `std::out_of_range`, distinct from the unchecked `operator[]`.
3. Handling `N == 0` — `inline_buf_` becomes a zero-size array, which is technically not standard C++ (zero-size arrays are a GCC/Clang extension); a `std::conditional`-based specialization or simply requiring `N >= 1` via a `static_assert` are both reasonable resolutions, and this project intentionally leaves the choice — and the requirement to *notice* the zero-size-array pitfall — to the learner.
4. A `pop_back`-triggered transition-back-to-inline policy decision (this reference implementation, matching `std::vector`, never shrinks capacity — document this explicitly).

## Testing Strategy

Beyond the visible tests: instrument `operator new`/`operator delete` globally (or inject a custom allocator) to get an exact heap-allocation count rather than inferring it indirectly, since indirect inference (e.g. via timing) is unreliable. For the throwing-constructor test, build a small test-only type whose move/copy constructor increments a static counter and throws once that counter reaches a configured value — this is the standard technique for testing exception-safety guarantees and is worth building once, reusably, since later chapters' projects will want it again.

## Performance Analysis

While `size() <= N`, every operation is on stack-resident (or wherever the `small_vector` object itself lives) memory with zero indirection to a separate heap block — this is the entire performance motivation for the type, avoiding an allocation for the common case of small element counts. Once transitioned to heap storage, subsequent behavior is asymptotically identical to `std::vector`'s amortized-O(1) `push_back`. The one-time transition cost is O(current size) moves/copies, same as any `std::vector` reallocation.

## Failure Modes

- A move/copy constructor throwing during growth, left unhandled — corrupts the container (double-destruction or lost elements) unless the construct-into-new-storage-first ordering above is followed correctly.
- `N == 0` compiled without addressing the zero-size-array issue — undefined behavior or a compiler-specific extension silently relied upon.
- Forgetting to destroy previously-constructed elements before overwriting `data_`/`capacity_` in `grow()` on the success path — a memory/resource leak that won't show up in a basic correctness test, only under a per-type construction/destruction counter.

## Extensions

- A `resize()`/`reserve()` public API matching more of `std::vector`'s surface.
- Iterator-debugging support (a debug-build-only generation counter incremented on every reallocation, checked by iterators to detect use of an invalidated iterator) — directly useful preparation for later Ch04/Ch11 material on iterator invalidation.
