# P-2.2 — Solution

## Reference Architecture

An aligned raw byte buffer, plus a per-instance pointer to a small "operations table" generated once per contained type by a template function, with `if constexpr` branching to skip the table entirely for trivially-copyable types.

```cpp
template <std::size_t N, std::size_t Align = alignof(std::max_align_t)>
class inplace_any {
    alignas(Align) unsigned char buf_[N];
    struct Ops {
        void (*copy)(void* dst, const void* src);
        void (*move)(void* dst, void* src);
        void (*destroy)(void* obj);
        const std::type_info* type;
    };
    const Ops* ops_ = nullptr; // nullptr means "empty" or "trivial fast path with no ops needed"
    bool holds_trivial_ = false;
    const std::type_info* trivial_type_ = nullptr;

public:
    inplace_any() = default;

    template <typename T>
    inplace_any(T value) {
        static_assert(sizeof(T) <= N, "T does not fit in inplace_any<N>'s inline buffer");
        static_assert(alignof(T) <= Align, "T's alignment exceeds inplace_any's supported alignment");
        construct<T>(std::move(value));
    }

    ~inplace_any() { reset(); }

    inplace_any(const inplace_any& other) {
        if (other.ops_) { ops_ = other.ops_; ops_->copy(buf_, other.buf_); }
        else if (other.holds_trivial_) { std::memcpy(buf_, other.buf_, N); holds_trivial_ = true; trivial_type_ = other.trivial_type_; }
    }

    inplace_any& operator=(inplace_any&& other) noexcept {
        if (this != &other) {
            reset();
            if (other.ops_) { ops_ = other.ops_; ops_->move(buf_, other.buf_); other.reset(); }
            else if (other.holds_trivial_) { std::memcpy(buf_, other.buf_, N); holds_trivial_ = true; trivial_type_ = other.trivial_type_; other.holds_trivial_ = false; }
        }
        return *this;
    }

    template <typename T>
    inplace_any& operator=(T value) {
        reset();
        construct<T>(std::move(value));
        return *this;
    }

    bool has_value() const noexcept { return ops_ != nullptr || holds_trivial_; }

    template <typename T>
    T& get() {
        if (current_type() != &typeid(T)) throw std::bad_cast{};
        return *reinterpret_cast<T*>(buf_);
    }

private:
    const std::type_info* current_type() const noexcept {
        return ops_ ? ops_->type : trivial_type_;
    }

    template <typename T>
    void construct(T value) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(buf_, &value, sizeof(T));
            holds_trivial_ = true;
            trivial_type_ = &typeid(T);
        } else {
            static const Ops ops{
                +[](void* dst, const void* src) { ::new (dst) T(*static_cast<const T*>(src)); },
                +[](void* dst, void* src) { ::new (dst) T(std::move(*static_cast<T*>(src))); },
                +[](void* obj) { static_cast<T*>(obj)->~T(); },
                &typeid(T),
            };
            ::new (buf_) T(std::move(value));
            ops_ = &ops;
        }
    }

    void reset() {
        if (ops_) { ops_->destroy(buf_); ops_ = nullptr; }
        holds_trivial_ = false;
        trivial_type_ = nullptr;
    }
};
```

## Design Rationale

**Why a `static const Ops` local to the template function rather than a member of `inplace_any` itself?** Each instantiation of `construct<T>` needs exactly one `Ops` table shared across every `inplace_any` instance that ever holds that specific `T` — it's per-type data, not per-instance data, so a function-local `static` (one per template instantiation, initialized once, shared thereafter) is the natural fit and avoids constructing a fresh table on every single `inplace_any` construction.

**Why skip the `Ops` table entirely for trivially-copyable types rather than just giving them a trivial `Ops` whose `copy`/`move` are `memcpy` and whose `destroy` is a no-op?** Either would be *correct*, but going through a function pointer at all — even one that just does a `memcpy` — still pays for an indirect call the compiler generally cannot inline, whereas branching at compile time via `if constexpr` and calling `std::memcpy` directly lets the compiler potentially inline or vectorize it. This is the actual mechanism behind the "verified, not just claimed" fast-path requirement: the trivial path's copy/move/destroy are ordinary direct calls with no vtable-style indirection anywhere in the generated code.

**Why use `typeid`/`std::type_info` for the type check rather than a hand-rolled type-id scheme?** `typeid` gives correct, unique identity per type across translation units for free, at the cost of a small amount of RTTI machinery already present in most builds (and explicitly documented here as a deliberate, acknowledged cost rather than an accidental one) — a hand-rolled scheme (e.g. a counter incremented per distinct `T` the template is instantiated with) is a legitimate alternative if avoiding RTTI entirely is a hard constraint, but introduces its own correctness subtleties around ODR and cross-translation-unit consistency that `typeid` avoids by construction.

## Reference Implementation

The above is close to complete. Remaining work:
1. A move-constructor mirroring the copy-constructor's structure using `move` instead of `copy`.
2. Deciding the oversized-type policy definitively — the `static_assert`s above already implement compile-time rejection; if choosing heap-fallback instead, replace them with a runtime branch that heap-allocates and adjusts every operation (`get`, `reset`, copy/move) to check "inline or heap" first.
3. `type()` accessor exposing `current_type()` publicly.

## Testing Strategy

Reuse (or lightly extend) the `Tracked` instrumented type from [P-1.4](../copy-move-harness/STATEMENT.md) to verify exactly one construction and one destruction per logical value across copy/move/reassignment sequences — this catches the class of bug where reassignment forgets to destroy the previously-held value, or where a copy accidentally invokes a move (or vice versa) due to a forwarding mistake in the `Ops` table's function bodies.

## Performance Analysis

The trivial fast path should show measurably lower overhead per copy/move/destroy than the non-trivial dispatch path in a microbenchmark, since the trivial path is a direct `memcpy` (often inlined) while the non-trivial path pays for at least one indirect call per operation. The magnitude of the difference will vary by compiler and optimization level — report the actual measured numbers rather than a predicted one, and note that at high optimization levels a smart compiler devirtualizing a small, non-polymorphic call through a `static const` function pointer table is unlikely, since the whole point of the mechanism is runtime-selected behavior.

## Failure Modes

- Forgetting to call `reset()` before constructing a new value during reassignment — destroys nothing, "constructs" over live bytes, corrupting or leaking the previous value.
- An over-aligned type whose actual required alignment exceeds the `Align` template parameter's default, used without specifying a stricter `Align` explicitly — either a compile-time alignment mismatch (best case) or silent misalignment (if your `static_assert` doesn't check `alignof(T) <= Align`).
- Mixing up `copy`/`move` operations in the `Ops` table (e.g. always calling the copy constructor even from a move context) — subtle, since it still compiles and often still "works" for types that don't observably differ in copy vs. move behavior, but fails the instrumented-type tests.

## Extensions

- A small-buffer-optimized `std::function`-like callable wrapper is a very close cousin of this project — implementing `unique_function` (Ch05, P-2.3) will feel almost identical in structure.
- Exposing a `visit`-style API accepting a set of overloads keyed by possible held types, similar to `std::variant`'s `std::visit`, as a more ergonomic alternative to the type-checked `get<T>()`.
