# P-2.4 — Solution

## Reference Architecture

`Result<T,E>` as a discriminated union over `T` and `E` (structurally close to `inplace_any` from [P-2.2](../inplace-any/STATEMENT.md), but closed over exactly two known types rather than open-ended), with `map`/`and_then`/`or_else` as member functions branching on the discriminant.

```cpp
template <typename T, typename E>
class Result {
    union { T value_; E error_; };
    bool has_value_;

public:
    Result(T value) : value_(std::move(value)), has_value_(true) {}
    Result(E error) : error_(std::move(error)), has_value_(false) {}

    Result(const Result&) = delete; // omitted for brevity; a real impl needs copy/move handling
                                     // the active union member explicitly (see Design Rationale)

    ~Result() { if (has_value_) value_.~T(); else error_.~E(); }

    bool has_value() const noexcept { return has_value_; }
    explicit operator bool() const noexcept { return has_value_; }

    const T& value() const { assert(has_value_); return value_; }
    const E& error() const { assert(!has_value_); return error_; }

    template <typename F>
    auto map(F&& f) const -> Result<std::invoke_result_t<F, T>, E> {
        if (has_value_) return Result<std::invoke_result_t<F, T>, E>(f(value_));
        return Result<std::invoke_result_t<F, T>, E>(error_);
    }

    template <typename F> // F: T -> Result<U, E>
    auto and_then(F&& f) const -> std::invoke_result_t<F, T> {
        if (!has_value_) return std::invoke_result_t<F, T>(error_);
        return f(value_);
    }

    template <typename F> // F: E -> Result<T, E2>
    auto or_else(F&& f) const -> std::invoke_result_t<F, E> {
        if (has_value_) return std::invoke_result_t<F, E>(value_);
        return f(error_);
    }
};
```

`Result<void, E>` as a partial specialization dropping the `T`-related storage and accessors entirely:

```cpp
template <typename E>
class Result<void, E> {
    std::optional<E> error_; // empty optional == success

public:
    static Result success() { return Result{}; }
    Result(E error) : error_(std::move(error)) {}

    bool has_value() const noexcept { return !error_.has_value(); }
    const E& error() const { assert(error_.has_value()); return *error_; }

private:
    Result() = default;
};
```

## Design Rationale

**Why a raw `union` rather than reusing `inplace_any` from P-2.2 directly?** `Result<T,E>` knows exactly which two types it can hold at compile time — there's no need for the runtime type-tag/dispatch machinery `inplace_any` needs to support an open set of types. A plain `union` plus a `bool` discriminant is simpler, smaller, and lets the compiler know statically which destructor/constructor path applies, rather than going through a function-pointer table for a fixed two-way choice. This is a case where the more general tool from an earlier project would be over-engineering for a narrower, statically-known problem.

**Why does copy/move construction need to branch on `has_value_` explicitly, rather than being defaulted?** A `union` member's special member functions are not automatically invoked by the compiler for the currently-active member — the compiler has no way to know which member is "active" (that's exactly what your separate `has_value_` flag tracks), so copy/move construction, assignment, and destruction must all manually check `has_value_` and placement-construct/destroy the correct member explicitly. This is a direct, hands-on application of Ch01–02's "storage lifetime is distinct from object lifetime" principle in a context (a hand-rolled sum type) where the language doesn't do the bookkeeping for you the way it does for a `std::variant`.

**Why does `and_then` check `!has_value_` and return immediately, while `map` always proceeds to call `f`?** `and_then`'s whole purpose is chaining a fallible operation — if the input is already an error, calling `f` would be pointless (and, if `f` has side effects, actively wrong, since it would run a stage's logic on data that was never validly produced). `map`, in the reference implementation above, actually mirrors this same check (`if (has_value_)`) — the key distinction from `and_then` isn't in *whether* to check, but in what happens to the *result*: `map`'s `f` returns a plain `U`, which the reference implementation wraps in a `Result` for the caller, while `and_then`'s `f` already returns a `Result`, which is passed through unmodified. Getting these two conflated (e.g. implementing `map` in a way that requires `f` to return a `Result`, or `and_then` in a way that double-wraps) is the most common structural bug in a from-scratch monadic-Result implementation.

## Reference Implementation

The above is close to complete for `Result<T,E>`'s core; remaining work:
1. Copy and move constructors/assignment operators, each explicitly branching on `has_value_` to construct/destroy the correct active union member.
2. The demonstration pipeline itself: four pipeline stages (`parse`, `validate`, `transform`, `persist`) each returning a `Result<StageOutput, PipelineError>`, chained via `and_then`, where `PipelineError` is a single `enum class` (or small tagged struct) covering all four stages' distinct failure kinds — the simpler, common-error-type approach documented as the deliberate choice for this project (per-stage distinct error types with a unification step is a valid but more complex alternative, left as an extension).
3. The exceptions-based parallel pipeline for the comparison write-up, and the write-up itself citing specific lines from both versions.

## Testing Strategy

The "later stages never invoked" tests are the most valuable in this suite — they're the concrete proof that short-circuiting actually works, as opposed to merely "the final result happens to look like an error," which could occur even with an implementation that wastefully runs every stage and discards results. Instrument each pipeline stage with a call counter to make this observable directly rather than inferring it.

## Performance Analysis

`Result<T,E>`'s size is `max(sizeof(T), sizeof(E)) + alignment padding + sizeof(bool)` — smaller than a solution that stored both a `std::optional<T>` and a `std::optional<E>` side by side, since only one is ever live. All operations are direct branches and function calls with no heap allocation, no exceptions thrown on the expected-failure path, and no dynamic dispatch — the entire error-propagation mechanism is exception-free by construction, which is the concrete performance and control-flow-clarity argument for this style over exceptions for expected, frequent failure modes.

## Failure Modes

- Forgetting to explicitly branch on `has_value_` in copy/move/destroy, silently invoking the wrong union member's special member function (or none at all) — undefined behavior that may not manifest as an obvious crash.
- `and_then` implemented to always call `f`, relying on `f` itself to check whether it received valid input — this defeats the short-circuiting contract and pushes a responsibility onto every caller that the library should be handling centrally.
- Error types that are unstructured strings, making `or_else`-based recovery logic unable to distinguish error kinds without string parsing — exactly the anti-pattern this project's Error Handling section warns against.

## Extensions

- A `Result<T,E>` supporting per-stage distinct error types unified via an explicit `std::variant`-based combined error type, rather than requiring one common `E` across the whole chain — a meaningfully harder but more flexible design.
- `operator|` overloads providing a pipe-based chaining syntax (`parse(input) | validate | transform | persist`) as a more ergonomic alternative to nested `.and_then(...)` calls, directly foreshadowing Ch13's ranges-pipeline composability.
