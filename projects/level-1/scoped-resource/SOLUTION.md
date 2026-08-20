# P-1.2 — Solution

## Reference Architecture

All three types share one shape: a stored resource/action plus a boolean-ish "armed" flag, a deleted copy pair, a move constructor/assignment that transfers and disarms the source, and a destructor that fires the cleanup only if still armed.

```cpp
class scoped_file {
    FILE* fp_ = nullptr;
    std::function<void()> on_close_; // fake-close hook for tests; real code would call fclose

public:
    scoped_file() = default;
    scoped_file(FILE* fp, std::function<void()> on_close = {})
        : fp_(fp), on_close_(std::move(on_close)) {}

    scoped_file(const scoped_file&) = delete;
    scoped_file& operator=(const scoped_file&) = delete;

    scoped_file(scoped_file&& other) noexcept
        : fp_(other.fp_), on_close_(std::move(other.on_close_)) {
        other.fp_ = nullptr;
    }

    scoped_file& operator=(scoped_file&& other) noexcept {
        if (this != &other) {
            close_now();                       // release whatever *this* currently holds
            fp_ = other.fp_;
            on_close_ = std::move(other.on_close_);
            other.fp_ = nullptr;
        }
        return *this;
    }

    ~scoped_file() { close_now(); }

    FILE* release() noexcept {
        FILE* f = fp_;
        fp_ = nullptr;
        on_close_ = nullptr;
        return f;
    }

private:
    void close_now() noexcept {
        if (fp_) {
            std::fclose(fp_);
            if (on_close_) on_close_();
        }
        fp_ = nullptr;
    }
};
```

`scope_exit` is the fully generic version of the same shape, parameterized on any no-arg invocable (typically type-erased via `std::function<void()>` for simplicity, or templated directly on the callable type to avoid the allocation — see Design Rationale):

```cpp
template <typename F>
class scope_exit {
    F action_;
    bool armed_ = true;
public:
    explicit scope_exit(F action) : action_(std::move(action)) {}
    scope_exit(scope_exit&& other) noexcept
        : action_(std::move(other.action_)), armed_(other.armed_) {
        other.armed_ = false;
    }
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit& operator=(scope_exit&&) = delete; // deleted, not just move-only-by-default; see rationale

    void dismiss() noexcept { armed_ = false; }

    ~scope_exit() noexcept {
        if (armed_) action_();
    }
};
```

`scoped_timer` follows the same disarm-on-move pattern with a `std::chrono::steady_clock::time_point` start and a reporting callback invoked from the destructor.

## Design Rationale

**Why does the destructor need no special-casing for exception-unwinding vs. normal exit?** A destructor is invoked identically by the language whether control reaches the end of scope normally or via unwinding — `~scope_exit()` has no way to distinguish the two cases (short of calling `std::uncaught_exceptions()`, which none of these types need to do), and that's the point: RAII gives you the exception-safety property *because* you don't have to write separate cleanup logic for the exceptional path. This is the core reason RAII is preferred over manual `try`/`catch`/cleanup blocks.

**Why does move-assignment need to close the target's existing resource first?** Move assignment is really "destroy what I currently hold, then take what the source holds." Skipping the first half is the single most common bug in hand-written RAII wrappers — it silently leaks whatever `*this` used to own, because nothing else will ever call cleanup on it. Routing this through the same `close_now()` the destructor uses (rather than duplicating the logic) also means there's only one place that can get the "is this armed" check wrong.

**Why is `scope_exit`'s move-assignment deleted rather than implemented?** `scope_exit` is meant to be a scope guard constructed once and left alone until it goes out of scope — it's move-*constructible* (needed so it can be returned from a factory function or passed into a container by value), but move-*assigning* into an already-armed guard raises the same "what happens to the target's pending action" question as `scoped_file`, with a less obviously correct answer (should the target's pending action run immediately before being overwritten, or just get discarded silently?). Deleting it sidesteps an ambiguous design question this project doesn't need to answer; a real production utility (like `std::experimental::scope_exit` proposals) may choose to support it, at the cost of specifying that behavior explicitly.

**Why is self-move-assignment safety non-trivial here?** In `scoped_file::operator=(scoped_file&& other)`, if `this == &other`, calling `close_now()` on `*this` also destroys the resource `other` (i.e. the same object) is about to be "moved from" — the `if (this != &other)` guard exists specifically to make self-move-assignment a no-op rather than a use-after-close.

## Reference Implementation

The snippets above are representative, not a complete header. Remaining work for a full submission:
1. `scoped_timer`'s destructor logic (start-time capture in the constructor, `steady_clock::now() - start_` delta computed and handed to the callback in the destructor — structurally identical to `scoped_file`'s "fire callback once, guarded by an armed flag").
2. Deciding and documenting the cleanup-failure channel (e.g. `fclose` returning nonzero) — a thread-local last-error slot or a caller-supplied error callback are both defensible; the key requirement is that the destructor itself never throws.
3. Wiring the `std::function`-vs-template-parameter choice for `scope_exit` and documenting the allocation trade-off either way.

## Testing Strategy

Beyond the visible GoogleTest suite: run the full suite under ASan specifically targeting the move-assignment and self-move-assignment paths, since those are exactly the paths a correct-looking-but-subtly-wrong implementation will pass on the "happy path" tests but fail under a sanitizer. For the exception-unwinding test, verify the guarded action runs *exactly once* — not zero times (guard didn't fire) and not more than once (destructor ran twice, which would itself indicate a double-destruction bug elsewhere).

## Performance Analysis

All three types are stack-allocated, single-ownership wrappers with no heap allocation in their own right — the only possible allocation is inside `std::function` if the captured lambda's state exceeds its small-buffer-optimization threshold (implementation-defined, typically a couple of pointers' worth). Templating `scope_exit` on the concrete callable type instead of type-erasing removes this cost entirely at the price of a less convenient type name (deduced via CTAD in practice) and no ability to store heterogeneous guards in a single container — a real trade-off worth stating explicitly rather than defaulting to `std::function` out of habit.

## Failure Modes

- A resource cleanup call that fails (e.g. `fclose` erroring) with no failure-reporting path wired up — silently swallowed unless you built the documented error channel.
- A destructor that lets a cleanup exception propagate during unwinding from another exception — `std::terminate`, immediately and unrecoverably.
- A move-assignment that forgets to release the target's prior resource — a resource leak that will not show up in ordinary correctness tests, only under an allocation/handle-count check or ASan-style tracking.

## Extensions

- A `unique_resource<Handle, Deleter>` generalization (as proposed for the standard library, `std::experimental::unique_resource`) that subsumes `scoped_file` as a single instantiation rather than a bespoke type.
- Extending `scope_exit` with `scope_fail`/`scope_success` variants that only run their action depending on whether the scope is exiting via an exception (using `std::uncaught_exceptions()` to detect this) — directly useful in transactional code that needs "roll back only if something went wrong."
