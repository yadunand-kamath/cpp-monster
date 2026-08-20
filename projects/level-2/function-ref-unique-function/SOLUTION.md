# P-2.3 — Solution

## Reference Architecture

`function_ref` as exactly two pointers; `unique_function` as an SBO-based owning wrapper reusing the same "operations table generated per-type" idea as `inplace_any`, but with `invoke`/`move`/`destroy` instead of `copy`/`move`/`destroy`.

```cpp
template <typename Sig> class function_ref;

template <typename R, typename... Args>
class function_ref<R(Args...)> {
    void* obj_ = nullptr;
    R (*invoke_)(void*, Args...) = nullptr;

public:
    template <typename F>
    function_ref(F& f) noexcept
        : obj_(const_cast<void*>(static_cast<const void*>(&f))),
          invoke_([](void* obj, Args... args) -> R {
              return (*static_cast<F*>(obj))(std::forward<Args>(args)...);
          }) {}

    R operator()(Args... args) const {
        return invoke_(obj_, std::forward<Args>(args)...);
    }
};
```

```cpp
template <typename Sig, std::size_t InlineSize = 32> class unique_function;

template <typename R, typename... Args, std::size_t InlineSize>
class unique_function<R(Args...), InlineSize> {
    alignas(std::max_align_t) unsigned char buf_[InlineSize];
    void* storage_ = nullptr;         // points into buf_ or into a heap block
    bool heap_ = false;

    struct Ops {
        R (*invoke)(void*, Args...);
        void (*move)(void* dst_buf, void*& dst_storage, void* src_storage, bool src_heap);
        void (*destroy)(void* storage, bool heap);
    };
    const Ops* ops_ = nullptr;

public:
    unique_function() = default;

    template <typename F>
    unique_function(F f) {
        static const Ops ops{
            [](void* obj, Args... args) -> R {
                return (*static_cast<F*>(obj))(std::forward<Args>(args)...);
            },
            [](void* dst_buf, void*& dst_storage, void* src_storage, bool src_heap) {
                if (src_heap) { dst_storage = src_storage; }           // transfer heap pointer
                else { ::new (dst_buf) F(std::move(*static_cast<F*>(src_storage)));
                       dst_storage = dst_buf; }
            },
            [](void* storage, bool heap) {
                static_cast<F*>(storage)->~F();
                if (heap) ::operator delete(storage);
            },
        };
        ops_ = &ops;
        if constexpr (sizeof(F) <= InlineSize) {
            ::new (buf_) F(std::move(f));
            storage_ = buf_;
            heap_ = false;
        } else {
            storage_ = ::operator new(sizeof(F));
            ::new (storage_) F(std::move(f));
            heap_ = true;
        }
    }

    unique_function(unique_function&& other) noexcept {
        if (other.ops_) {
            ops_ = other.ops_;
            heap_ = other.heap_;
            ops_->move(buf_, storage_, other.storage_, other.heap_);
            other.ops_ = nullptr;
            other.storage_ = nullptr;
        }
    }

    unique_function(const unique_function&) = delete;
    unique_function& operator=(const unique_function&) = delete;

    ~unique_function() { if (ops_) ops_->destroy(storage_, heap_); }

    R operator()(Args... args) const {
        return ops_->invoke(storage_, std::forward<Args>(args)...);
    }
};
```

## Design Rationale

**Why does `function_ref` need only two pointers and no vtable struct, while `unique_function` needs a small `Ops` table?** `function_ref` has exactly one operation to type-erase — invocation — since it never owns, copies, or destroys anything; there's nothing else for it to do to the referenced object. `unique_function` owns its callable and must eventually destroy it, and must support moving that ownership, so it genuinely needs (at minimum) invoke, move, and destroy — three operations, which is naturally expressed as a small struct of function pointers rather than three separate raw pointers scattered across the class.

**Why does `unique_function`'s `Ops::move` handle the heap case by just copying the pointer, but the inline case by placement-constructing?** For a heap-allocated callable, "moving" it means transferring ownership of the *same* heap block — the object never needs to relocate in memory, so no move-construction of the contained callable is required at all (this is actually cheaper than the inline case). For an inline callable, the object physically lives inside the `unique_function`'s own storage, which is a different memory address for the destination object — so an actual move-construction of the contained callable into the new inline buffer is unavoidable. This asymmetry is a direct, useful consequence of the same SBO trade-off explored in [P-1.3](../small-vector/STATEMENT.md) and [P-2.2](../inplace-any/STATEMENT.md): heap indirection buys you cheap moves at the cost of an allocation; inline storage buys you no allocation at the cost of a real move on transfer.

**Why does `unique_function`'s template constructor never reference a copy operation for `F`, even implicitly?** Every operation generated (`invoke`, `move`, `destroy`) uses `F`'s move constructor or destructor, never its copy constructor — this is deliberate and is exactly what makes wrapping a move-only capture (like a `unique_ptr`-capturing lambda) compile successfully, whereas `std::function`'s implementation typically requires the target to be copy-constructible (since `std::function` itself must remain copyable), causing a compile error deep in `std::function`'s own internals when instantiated with a move-only callable.

## Reference Implementation

The above covers the structurally interesting parts of both types. Remaining work for a complete submission:
1. `unique_function`'s move-assignment operator (destroy current, then delegate to the same logic as the move constructor).
2. Deciding and documenting empty-call behavior (a null `ops_` check in `operator()` throwing, or a documented precondition-violation/UB choice matching raw-function-pointer-call semantics).
3. The comparative benchmark/compile-failure demonstration against `std::function` for the move-only-capture case.

## Testing Strategy

For `unique_function`'s SBO threshold, test at least three points: comfortably below threshold, exactly at threshold, and above threshold, since boundary-adjacent bugs (off-by-one in the `sizeof(F) <= InlineSize` check) are the most likely place for a subtle correctness gap. Reuse the `Tracked` instrumented type to confirm the move path invokes exactly one move construction (inline case) or zero (heap case, pointer transfer only) — this distinguishes a correct implementation from one that accidentally always move-constructs even when it could just transfer a heap pointer.

## Performance Analysis

`function_ref` construction and invocation should compile down to essentially a function-pointer call with one level of indirection — no allocation, no reference counting, size exactly `2 * sizeof(void*)`. `unique_function`'s inline path avoids allocation entirely for captures under the threshold; its heap path pays one allocation at construction and, critically, zero additional allocations on move (a meaningful advantage over always deep-copying, and the reason the move-transfers-pointer test matters).

## Failure Modes

- A `function_ref` outliving the callable it refers to — a dangling-reference bug with no compile-time or (typically) runtime detection, exactly mirroring `std::string_view`'s well-known danger; this must be documented prominently, not silently left as a footnote.
- `unique_function`'s `Ops::move` forgetting to null out the moved-from object's `storage_`/`ops_`, causing the destructor of the moved-from object to double-destroy or double-free.
- Choosing an `InlineSize` too small for common real-world lambda captures (e.g. capturing two or three `std::string`s by value), causing the "common case" to hit the heap-fallback path far more often than intended — worth measuring against realistic capture sizes, not just the test suite's synthetic ones.

## Extensions

- A `move_only_function`-style alias matching the C++23 standard library addition of exactly this capability, as a point of comparison once available on your toolchain.
- Extending `function_ref` to support const-qualified and ref-qualified call operators, mirroring the overload-set complexity `std::function_ref` proposals (P0792) had to resolve.
