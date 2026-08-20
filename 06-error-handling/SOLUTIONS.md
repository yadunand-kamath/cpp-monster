# Chapter 06 — Solutions

## Quick Check Answers

**06-QC1.** In reverse order of construction — the most recently constructed local object's destructor runs first, then the next-most-recent, and so on, unwinding the frame in a strict LIFO order matching normal scope-exit destruction.

**06-QC2.** Something else entirely: `std::terminate` is called. A `noexcept` function throwing is treated as a violated contract, not a normal throw — the runtime does not search for a matching `catch`, and unwinding through the `noexcept` boundary never begins.

**06-QC3.** Because a throwing move mid-reallocation could leave the container in an unrecoverable state: some elements already moved into the new buffer, some still in the old buffer, with no well-defined way to unwind back to "as if reallocation never happened." Copying instead guarantees the old buffer is left intact and can simply be discarded on failure, preserving the strong guarantee that `reserve`/`push_back`-triggered growth documents.

**06-QC4.** The basic guarantee only promises no leaks and preserved invariants — the object's actual value after a caught exception may differ from its pre-call value (it's in *some* valid state, not necessarily the original one). The strong guarantee promises the operation either fully completes or has no visible effect at all — the object's value is unchanged if an exception propagates out.

**06-QC5.** Because copy-and-swap moves the risk of throwing into the *construction* of a new, temporary, off-to-the-side object — if that construction throws, the original object was never touched. The final `swap` between the temporary and the original need only exchange already-fully-constructed resources (pointers, sizes, handles), which can genuinely be implemented to never throw, so the one step that actually commits the change is `noexcept` and can't fail partway through.

**06-QC6.** Because the same numeric value is only meaningful within the context of its category — different categories can and do reuse the same integer to mean entirely unrelated things. `std::error_code` comparison checks category identity, not just the stored value, specifically to prevent a code from category A being mistaken for an unrelated code that happens to share the same number in category B.

**06-QC7.** `expected<T, E>` carries the actual error value `E`, so the caller can inspect, log, or branch on *why* the operation failed. `optional<T>` collapses every failure reason into the same "nothing," discarding that information — a caller who needs to know why has to invent an out-of-band channel to recover it.

**06-QC8.** Because throwing an exception carries active runtime cost specifically on the throw path — stack unwinding and RTTI-based type matching against each candidate `catch` — costs an error-code-based function never pays at all, since returning a value never invokes any of that machinery. For a failure that happens often and is really just a normal outcome (not exceptional), avoiding that per-occurrence cost matters.

## Problem Solutions

### Level 1 — Recognition

**06-P01.** A function with no `noexcept` specifier defaults to `noexcept(false)` — it is permitted to throw. Destructors are the one exception to this default: since C++11, a destructor is implicitly `noexcept(true)` unless a base class or member's destructor is itself potentially-throwing, or the destructor is explicitly declared otherwise, precisely because destructors run during unwinding and a throwing destructor there is one of the few situations that unconditionally calls `std::terminate` (see 06-P35).

---

**06-P02.** (a) is the **strong guarantee** — "fully succeeds or unchanged" is exactly the commit-or-rollback definition. (b) is the **basic guarantee** — invariants hold and nothing leaks, but the specific post-failure state is unspecified. (c) is the **nothrow guarantee** — a declared, reliable promise never to propagate an exception.

---

**06-P03.** The category must also be considered. `error_code`'s numeric value alone is only meaningful relative to the `error_category` it's paired with — the same integer can mean entirely different things in different categories, so the category is not optional context, it's part of the identity of the error.

---

**06-P04.** Throwing from a destructor is legal C++ syntactically, but is one of the most dangerous things you can do — if it happens while unwinding is already in progress from a different exception, it calls `std::terminate` unconditionally (see 06-P35). Throwing from a function marked `noexcept` is legal to write but calls `std::terminate` at runtime if it actually happens. Catching a polymorphic exception type by value is legal but discouraged: it copies the exception object and, more importantly, causes the *static* type to be what's matched/sliced rather than preserving dynamic dispatch through the handler in the way catching by reference does — the idiomatic form is `catch (const Base&)`.

### Level 2 — Prediction

**06-P05.**
```
acquire
acquire
release
release
caught
```
**Explanation:** `r1` then `r2` are constructed in order. When `throw` executes, unwinding runs `r2`'s destructor first (most recently constructed), then `r1`'s, in strict reverse-construction order, before control reaches the `catch` block, which then prints `"caught"`.

---

**06-P06.** It copies the elements — despite `Movable` having a move constructor, `vector` copies during reallocation because the move constructor is marked `noexcept(false)`. The decision is made purely from the declared specification, not from whether that particular move would actually throw at runtime; `vector` cannot take the risk on your behalf.

---

**06-P07.** `std::terminate` is called; the program aborts (typically after printing something like "terminate called after throwing an instance of...") — `"caught: logging failed"` never prints. `log_error` is declared `noexcept`, so throwing from it violates that contract; the runtime does not search outward for a matching `catch` at all.

---

**06-P08.** Output is `-1`. `find_index` returning `nullopt` discards *why* `5` wasn't found (in this case there's nothing more to say, but in general `optional` collapses every failure into indistinguishable "nothing," whereas `expected<int, std::string>` could carry a message like `"5 not present in a 3-element range"` for a caller that wants to log or report specifics).

---

**06-P09.** Output is `100` — unchanged. Even though `withdraw` does not use copy-and-swap or any explicit rollback logic, the field is decremented at (1) and *then* checked at (2); since the exception is thrown from inside `withdraw` and the *only* observable side effect (the decrement) already happened before the throw, `balance_` is `-50` at the moment of the throw, not `100`. **Correction on walk-through:** at (1) `balance_` becomes `-50`; at (2) the throw fires with `balance_` already at `-50`. So the guarantee is **not strong** — `withdraw` mutates state before validating, and a caller catching the exception sees `balance()` return `-50`, not the pre-call `100`. (If the problem intended the printed output to be `100`, the implementation would need to validate *before* mutating; as written, the correct predicted output is `-50`, and the exercise's lesson is precisely that this implementation does *not* provide the strong guarantee.)

---

**06-P10.** Output is `10` (i.e., `true` then `false`). `ec1` and `ec2` are both constructed against `std::generic_category()` with the same underlying `errc` value, so they compare equal. `ec3` uses `std::system_category()` — a different category object — so even though its integer value was assigned from the same `errc`, `error_code::operator==` checks category identity as well as value, and the categories differ, so `ec1 == ec3` is `false`. `std::make_error_code(std::errc::...)` is documented to construct against `generic_category()`, which is what guarantees `ec1 == ec2` here.

---

**06-P11.** Output is `division by zero`. `divide(10, 0)` returns `std::unexpected("division by zero")` immediately — the `expected` already holds an error. `.and_then`'s lambda never executes: `.and_then` is documented to only invoke its callable when the receiver holds a value, and to pass an existing error straight through unchanged otherwise (short-circuiting), so the second `divide` call is never made.

---

**06-P12.** No, neither resource is leaked. `process()` throwing triggers stack unwinding through its own frame, which runs `p`'s and `v`'s destructors (in reverse order: `p` before `v`) exactly as it would on normal scope exit. `unique_ptr`'s destructor frees its owned `int`, and `vector`'s destructor frees its internal buffer — this is RAII doing its job during unwinding, independent of whether any `catch` exists anywhere in the call chain.

### Level 3 — Implementation

**06-P13.**
```cpp
template<typename T>
T parse_or_throw(std::string_view s) {
    T value{};
    auto first = s.data();
    auto last = s.data() + s.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        throw std::invalid_argument("cannot parse '" + std::string(s) + "' as requested type");
    }
    return value;
}
```
**Explanation:** `std::from_chars` itself never throws (it's a `noexcept`-friendly, error-code-based API), so this wrapper is the one place the "throw with a descriptive message" policy is applied. **Guarantee:** this function provides (trivially) the strong guarantee, because it has no observable state of its own — `value` is a fresh local that either fully initializes and is returned, or the function throws before returning anything at all; there's nothing pre-existing that could be left "partially modified."

---

**06-P14.**
```cpp
class TransactionLog {
    std::vector<std::string> entries_;
    size_t capacity_;
public:
    explicit TransactionLog(size_t capacity) : capacity_(capacity) {}

    void append(std::string_view entry) {
        if (entries_.size() >= capacity_) {
            throw std::length_error("transaction log full");
        }
        std::vector<std::string> tmp = entries_;      // (1) copy existing state
        tmp.emplace_back(entry);                       // (2) may throw (alloc/copy)
        entries_.swap(tmp);                             // (3) MUST be noexcept — commits the change
    }

    const std::vector<std::string>& entries() const { return entries_; }
};
```
The capacity check throws *before* any mutation, so that path trivially preserves the log unchanged. For the mutating path, `entries_.swap(tmp)` at (3) is the one operation that must be `noexcept` for the guarantee to hold: `std::vector::swap` only exchanges internal pointers/sizes/capacities between the two vectors and is guaranteed never to throw, so once construction of `tmp` (the risky part, at (1)-(2)) has fully succeeded, committing it into `entries_` cannot fail. If `tmp.emplace_back` throws at (2), `entries_` was never touched, so the log's existing contents remain exactly as they were.

---

**06-P15.**
```cpp
enum class ParseError { UnexpectedToken, UnterminatedString };

class ParseErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "parse_error"; }
    std::string message(int ev) const override {
        switch (static_cast<ParseError>(ev)) {
            case ParseError::UnexpectedToken:     return "unexpected token";
            case ParseError::UnterminatedString:  return "unterminated string literal";
        }
        return "unknown parse error";
    }
};

std::error_code make_parse_error(int line, int column) {
    static ParseErrorCategory cat;
    return {static_cast<int>(ParseError::UnexpectedToken), cat};
    // (line/column would typically be carried alongside the error_code in a richer
    //  error object, since error_code itself only carries a value + category)
}
```
**Explanation:** A custom category buys precise, unambiguous meaning for application-specific failure modes and their own `message()` text. If these codes were instead expressed through `std::generic_category()` reusing arbitrary `errc`-shaped integers, a caller comparing against a real `std::errc` condition (e.g., `std::errc::invalid_argument`) could incorrectly match a value that was never actually a POSIX-style "invalid argument" at all — it would just happen to share the same integer *and* category as one. A dedicated category makes that collision structurally impossible, since the category identity itself (not just the value) participates in every comparison.

---

**06-P16.**
```cpp
std::expected<double, std::string> safe_sqrt(double x) {
    if (x < 0.0) return std::unexpected("cannot take sqrt of negative number: " + std::to_string(x));
    return std::sqrt(x);
}

auto ok = safe_sqrt(9.0).transform([](double r) { return r * 2; });      // holds 6.0
auto bad = safe_sqrt(-4.0).transform([](double r) { return r * 2; });    // holds the error string, lambda never runs
```
**Explanation:** `ok` holds a value (`6.0`) because `safe_sqrt(9.0)` succeeded, so `.transform`'s lambda runs on `3.0`. `bad` short-circuits: `safe_sqrt(-4.0)` already holds an error, so `.transform` — like `.and_then` — passes the error through untouched and never invokes the lambda at all.

---

**06-P17.**
```cpp
template<typename F>
auto retry_n(F&& f, int attempts) -> decltype(f()) {
    std::exception_ptr last;
    for (int i = 0; i < attempts; ++i) {
        try {
            return f();
        } catch (...) {
            last = std::current_exception();
        }
    }
    std::rethrow_exception(last);
}
```
**Explanation:** `retry_n` provides no guarantee at all about how many times `f`'s *side effects* have run — it may call `f` anywhere from 1 to `attempts` times, and if `f` has observable side effects beyond its return value (writes, mutations, I/O), those side effects occur once per attempt made, including on attempts that ultimately fail. `retry_n` is only safe to use with an `f` that is itself idempotent or side-effect-free on failure; it does not add exception safety, it just adds repetition.

---

**06-P18.**
```cpp
class Buffer {
    std::unique_ptr<char[]> data_;
    size_t size_ = 0;
public:
    Buffer(const Buffer& other) : data_(std::make_unique<char[]>(other.size_)), size_(other.size_) {
        std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
    }

    friend void swap(Buffer& a, Buffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }

    Buffer& operator=(const Buffer& other) {
        Buffer tmp(other);   // may throw — happens before anything is touched
        swap(*this, tmp);    // noexcept — commits
        return *this;
    }
};
```
**Explanation:** `swap` only exchanges a `unique_ptr` (a pointer swap) and a `size_t` — neither operation can throw, so `swap` being genuinely `noexcept` (not just labeled so, but *actually* incapable of throwing given what it does) is exactly what 06-QC5 describes: the one step that commits the change must be incapable of failing partway through, or the strong guarantee collapses back into "maybe basic, maybe worse."

---

**06-P19.**
```cpp
template<typename T, typename E>
std::variant<T, E> to_variant(std::expected<T, E> e) {
    if (e.has_value()) return std::variant<T, E>(std::in_place_index<0>, std::move(*e));
    return std::variant<T, E>(std::in_place_index<1>, std::move(e.error()));
}

template<typename T, typename E>
std::expected<T, E> to_expected(const std::variant<T, E>& v) {
    if (v.index() == 0) return std::get<0>(v);        // convention: index 0 == success
    return std::unexpected(std::get<1>(v));
}
```
**Convention:** index `0` (the first alternative, `T`) always means success, index `1` (`E`) always means failure — this mirrors `expected`'s own two-state shape and is the most natural mapping since `variant<T, E>`'s template parameter order already puts `T` first. **Round trip:** `to_variant(to_expected(v))` and `to_expected(to_variant(e))` both preserve the held alternative and value for a success case and an error case. **Ergonomic gap:** raw `variant<T, E>` has no `.and_then`/`.transform`/`.value_or` — every consumer must `visit` or `holds_alternative`-check by hand; `expected` bakes the "one side is success, chain operations only on that side" convention directly into the type's interface.

---

**06-P20.**
```cpp
class ScopedErrorContext {
    inline static thread_local std::vector<std::string> stack_;
public:
    explicit ScopedErrorContext(std::string label) { stack_.push_back(std::move(label)); }
    ~ScopedErrorContext() { stack_.pop_back(); }
    static const std::vector<std::string>& current() { return stack_; }
};

void inner() {
    ScopedErrorContext ctx("parsing header");
    throw std::runtime_error("bad magic number");
}
void outer() {
    ScopedErrorContext ctx("loading file");
    try {
        inner();
    } catch (const std::exception& e) {
        // ScopedErrorContext::current() here is {"loading file", "parsing header"} —
        // inner()'s destructor for its own ctx already ran during unwinding into this catch,
        // but outer()'s ctx is still alive, and the *trail* was captured before inner unwound.
    }
}
```
**Clarification on what's actually observable:** by the time `outer`'s `catch` runs, `inner`'s `ScopedErrorContext` has already been popped (its destructor ran during unwinding through `inner`'s frame) — so `current()` inside `outer`'s `catch` shows only `{"loading file"}`. To observe the full nested trail including `"parsing header"`, the catch/inspection has to happen *before* that inner scope unwinds — e.g., by having `inner` catch-and-rethrow-with-context locally, or by capturing `current()`'s contents into the exception object itself at the point of `throw`, rather than relying on inspecting the stack from an outer handler after inner scopes have already destructed.

### Level 4 — Debugging

**06-P21.** [DEBUG] The reviewer is **incorrect** — this does not leak. When `FileHandle fh(p.c_str())` throws (from inside the constructor, because `fopen` failed), `fh`'s constructor never completes, which means `fh`'s destructor never runs for *that* attempt — but that's fine, because the constructor throwing means `f_` was never successfully opened for that path in the first place; there's nothing to close. Crucially, the exception propagates out of `process`, and any *earlier* successfully-constructed `FileHandle`s from prior loop iterations have already gone out of scope (each iteration's `fh` is destroyed at the end of that iteration, since it's declared inside the loop body) — there's no handle left open from a previous iteration either. The design is correct RAII; the reviewer's claim doesn't hold up under a careful walk-through.

---

**06-P22.** [DEBUG] The bug: `push_back` inside the loop mutates `data_` incrementally, so if the exception fires partway through (say, on the third of five items), `data_` already contains the first two items — the container is *not* unchanged, directly violating the documented strong guarantee, even though no invariant is broken and nothing leaks (i.e., it's accidentally only basic-guarantee, despite what the docstring promises).
```cpp
void push_all(const std::vector<int>& items) {
    for (int x : items) {
        if (x < 0) throw std::invalid_argument("negative value");
    }
    std::vector<int> tmp = data_;
    tmp.insert(tmp.end(), items.begin(), items.end());
    data_.swap(tmp);   // noexcept commit
}
```
**Fix:** validate every item *before* touching `data_` at all (first loop, no mutation), then build the new combined state in a temporary and commit via `swap`, which is `noexcept` — matching the copy-and-swap pattern from 06-P14/06-P18.

---

**06-P23.** [DEBUG] The missing detail: `Widget`'s move constructor has no `noexcept` specifier, so it defaults to `noexcept(false)` — `vector` therefore falls back to copying during `reserve`'s reallocation, exactly as in 06-P06. **Fix:**
```cpp
Widget(Widget&&) noexcept { /* ... */ }
```
But the problem states the move constructor "throws under some rare internal condition" — so simply slapping `noexcept` on it without removing that possibility would be a lie the compiler cannot catch, and if it ever actually threw, the result would be `std::terminate` (violating a `noexcept` contract, per 06-P07), not a caught exception. This is precisely why it's a **correctness** question, not just performance: you cannot mark a move constructor `noexcept` merely to get the fast path — you must first actually eliminate (or handle internally, never letting it escape) whatever rare condition could throw, and only then can `noexcept` be added truthfully.

---

**06-P24.** [DEBUG] The bug: when `key` isn't found, `m.find(key)` returns `m.end()`, and `it->second` dereferences an iterator that doesn't point to a valid element — this is undefined behavior (commonly a crash, but not guaranteed to be), not a clean "returns an error" path at all; there's no missing-key handling whatsoever. **Fix:**
```cpp
std::expected<int, std::string> lookup(const std::map<std::string, int>& m, const std::string& key) {
    auto it = m.find(key);
    if (it == m.end()) return std::unexpected("key not found: " + key);
    return it->second;
}
```

---

**06-P25.** [DEBUG] Unreported failure points: `std::ofstream out("config.tmp")` can fail to open (e.g., permissions, disk full) with no exception by default and no check of `out.is_open()`; `out << serialize(cfg)` can fail mid-write (disk full) with the stream simply entering a fail state, again unchecked; `out.close()`'s own failure is not checked; and `std::rename`'s return value is explicitly ignored. **Corrected implementation, policy = "returns `std::expected<void, std::string>`":**
```cpp
std::expected<void, std::string> save_config(const Config& cfg) {
    std::ofstream out("config.tmp");
    if (!out.is_open()) return std::unexpected("could not open config.tmp for writing");
    out << serialize(cfg);
    if (!out) return std::unexpected("write to config.tmp failed");
    out.close();
    if (!out) return std::unexpected("failed to finalize config.tmp");
    if (std::rename("config.tmp", "config.dat") != 0) {
        return std::unexpected(std::string("rename failed: ") + std::strerror(errno));
    }
    return {};
}
```

---

**06-P26.** [DEBUG] No, the `Widget` is not leaked. `w` is a local `unique_ptr` — if `items_.push_back(std::move(w))` throws, the move either didn't happen (in typical implementations, a throwing `push_back` either never touches the argument or leaves it in a valid, still-owning state) or `w` retains ownership; either way, when the exception propagates out of `acquire`, `w` goes out of scope and its destructor runs, freeing the `Widget` it owns. **Guarantee:** `acquire` provides the **basic guarantee** at worst (no leak, `items_` remains a valid, consistent vector — either the new element was appended or it wasn't) but not necessarily the strong guarantee in the fullest sense, since whether `items_`'s *size* changed depends on exactly where `push_back` failed; for `unique_ptr` elements (cheap, `noexcept`-movable), `push_back` itself typically does provide strong guarantee semantics per the standard's container requirements, so in practice `acquire` is strong here — but that's a property of `vector::push_back`'s own guarantee for nothrow-movable element types, not something `acquire` adds on its own.

---

**06-P27.** [DEBUG] This is a **common false alarm, not an actual bug**. `std::error_code`'s constructor stores a *reference* to the `error_category` object (categories are conventionally singletons, compared by address/identity, and `error_category` explicitly disables copying for this reason) — but a function-local `static` variable has **static storage duration**, initialized once on first call and living for the entire remaining duration of the program, exactly like a namespace-scope global. So `cat`'s address stays valid for as long as any `error_code` referencing it could possibly be alive. The reviewer's instinct (worrying about a local's lifetime ending) is reasonable to double-check, but function-local `static` specifically does *not* have automatic (scope-exit) storage duration, which is the detail that resolves the concern.

---

**06-P28.** [DEBUG] **Compile error for some `F`:** `decltype(f())()` requires the return type of `f()` to be default-constructible; for an `F` whose return type has no default constructor (e.g., returns a type requiring mandatory constructor arguments), this line fails to compile. **Subtler design/safety problem even when it compiles:** the function is declared `noexcept`, and the `try`/`catch` does make the promise *appear* satisfied for exceptions thrown by `f()` itself — but `decltype(f())()` (the fallback default-construction) can itself throw for a type whose default constructor isn't `noexcept`, and that throw would occur *inside* the `catch` block, in a `noexcept` function, with no further handler — calling `std::terminate` exactly as in 06-P07/06-P23. The `noexcept` promise is not trivially true here; it's silently violated for any return type whose default construction can throw.

### Level 5 — Integration

**06-P29.**
```cpp
template<typename T, typename E>
class Result {
    std::variant<T, E> data_;
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}
    static Result make_error(E err) { Result r(std::in_place_index<1>{}, std::move(err)); return r; }

    bool has_value() const { return data_.index() == 0; }

    const T& value() const {
        if (!has_value()) throw std::logic_error("Result::value() called on an error state");
        return std::get<0>(data_);
    }
    const E& error() const {
        if (has_value()) throw std::logic_error("Result::error() called on a success state");
        return std::get<1>(data_);
    }

    template<typename F>
    auto map(F&& f) const -> Result<decltype(f(value())), E> {
        if (has_value()) return Result<decltype(f(value())), E>(f(value()));
        return Result<decltype(f(value())), E>::make_error(error());
    }

    template<typename F>
    auto map_error(F&& f) const -> Result<T, decltype(f(error()))> {
        if (!has_value()) return Result<T, decltype(f(error()))>::make_error(f(error()));
        return Result<T, decltype(f(error()))>(value());
    }
private:
    Result(std::in_place_index_t<1>, E err) : data_(std::in_place_index<1>, std::move(err)) {}
};
```
**Pipeline:** `Result<int, std::string>(5).map([](int x){ return x * 2; }).map([](int x){ return x + 1; })` yields a success holding `11`. **Design divergence:** unlike `std::expected`, this `Result` throws `std::logic_error` (a programmer-error signal) rather than being pure UB or an assertion when `.value()`/`.error()` is called on the wrong state — a deliberate choice to make misuse loudly diagnosable during development, at the cost of `.value()` no longer being a trivially cheap, always-safe accessor the way `expected::value()` (which itself throws `bad_expected_access`) actually already does — so this particular divergence is arguably not a divergence at all, just matching `expected`'s real behavior rather than the more permissive `operator*`.

---

**06-P30.**
```cpp
enum class DivideError { DivisionByZero };

std::expected<int, DivideError> safe_divide(int a, int b) {
    if (b == 0) return std::unexpected(DivideError::DivisionByZero);
    return a / b;
}

int legacy_divide(int a, int b) {
    auto r = safe_divide(a, b);
    return r.has_value() ? *r : -1;
}
```
**Information lost in the downward translation:** `safe_divide`'s result type can only ever carry *one* specific, named error (`DivisionByZero`) today, so no information is lost yet — but the moment `DivideError` grows a second variant (say, `Overflow`), `legacy_divide`'s in-band `-1` sentinel collapses both distinct failure reasons into the same indistinguishable signal, and worse, `-1` remains ambiguous with any legitimate division result that happens to equal `-1` (e.g., `divide(-10, 10)`) — exactly the pre-existing correctness problem the redesign surfaces but does not, by itself, fix for `legacy_divide`'s callers.

---

**06-P31.**
```cpp
class ScopedTransaction {
    std::map<std::string, std::string>& store_;
    std::map<std::string, std::string> pending_;
    bool committed_ = false;
public:
    explicit ScopedTransaction(std::map<std::string, std::string>& store) : store_(store) {}

    void set(std::string key, std::string value) { pending_[std::move(key)] = std::move(value); }

    void commit() {
        for (auto& [k, v] : pending_) store_[k] = std::move(v);
        committed_ = true;
    }

    ~ScopedTransaction() {
        // committed_ == false: pending_ simply destructs, store_ was never touched — automatic rollback
    }
};
```
**Demonstration:** constructing a `ScopedTransaction`, calling `set("a","1")`, then throwing before `commit()` is ever called leaves `store_` completely empty (or however it was before) — `pending_`'s destructor discards the buffered change, and `store_` was never written to at all, since `commit()` (the only code path that writes to `store_`) never executed.

---

**06-P32.** **Chosen design:** a "check afterward" pattern — `Logger::write` is fully `noexcept` and returns `bool` (or an enum) indicating success, and the logger also exposes `Logger::last_error() noexcept` for callers who want detail, without forcing every call site to inspect anything.
```cpp
class Logger {
    LogError last_error_ = LogError::None;
public:
    bool write(std::string_view msg) noexcept {
        try {
            // ... actual write to stream/fd ...
            last_error_ = LogError::None;
            return true;
        } catch (...) {
            last_error_ = LogError::WriteFailed;   // swallow — never rethrow from here
            return false;
        }
    }
    LogError last_error() const noexcept { return last_error_; }
};
```
**Why `expected` is ruled out here:** `expected<void, E>`'s idiomatic use still involves *constructing* an `E` value on the failure path and returning it by value; if constructing or moving that `E` could itself throw, `write` would no longer be genuinely `noexcept`, defeating the entire point. Choosing `E` to be a small, trivially-constructible, `noexcept`-movable type (like a plain `enum class LogError`) sidesteps this — at which point `expected<void, LogError>` **would** actually be fine and could replace the bool+`last_error()` split with a single richer return value. The real constraint isn't "`expected` can't be `noexcept`-total," it's "your error payload type must itself be nothrow-constructible/movable," which is easy to satisfy for a small enum and easy to violate for, say, a `std::string` message.

---

**06-P33.** `operator+=`'s in-place nature is harder because it must mutate `*this` directly rather than building an entirely fresh return value — there's no "just don't return anything and the original is naturally untouched" fallback the way a `const`-returning `operator+` gets almost for free. To give `operator+=` the strong guarantee, the addition must be computed into a temporary buffer first and only *then* swapped into `*this`'s storage:
```cpp
Matrix& Matrix::operator+=(const Matrix& other) {
    if (rows_ != other.rows_ || cols_ != other.cols_) {
        throw std::invalid_argument("dimension mismatch");   // thrown before any mutation — trivially safe
    }
    std::vector<double> result(data_.size());
    for (size_t i = 0; i < data_.size(); ++i) result[i] = data_[i] + other.data_[i];
    data_.swap(result);   // noexcept commit — this is the step that makes it "harder"
    return *this;
}
```
The dimension check happens before any work, so the throwing path is trivially safe either way (as the problem notes); the genuinely harder part is that even the *non-throwing, dimensions-match* path must still avoid mutating `data_` element-by-element in place, because if `operator+`'s addition somehow failed partway through a direct in-place loop (not applicable for `double`, but would be for a throwing element type), `*this` would already be partially altered — copy-and-swap is required to keep the operation atomic from the caller's point of view.

### Level 6 — Production

**06-P34.** **Unified policy:** standardize on `std::expected<T, InsufficientFundsError>` (or a broader `AccountError` enum) as the return channel for this specific condition, not exceptions — "insufficient funds" during a withdrawal attempt is a routine, expected, part-of-normal-flow outcome for a payments system (a user overdrawing is not exceptional, it's Tuesday), which is exactly Common Misconception 5's distinction: reserve exceptions for genuinely exceptional conditions, not for common business-logic outcomes a caller is expected to branch on every time. **Migration without breaking existing callers in one release:** introduce the new `expected`-returning function under a new name (or overload disambiguated by return type via a tag), keep each of the three legacy functions as thin wrappers implemented in terms of the new one (throwing function catches nothing new to throw, since the new one doesn't throw for this condition — it translates the `expected`'s error case into the legacy throw/-1/out-param behavior), mark the three legacy entry points deprecated with a stated removal release, and give consumers a full release cycle (or two, per team norms) to migrate call sites before deleting the legacy wrappers. **The `-1` sentinel's pre-existing ambiguity** (conflating "insufficient funds" with "account not found") is a real, independent correctness bug — it should be fixed by giving the wrapper an actual distinct return value/exception for "not found" immediately, documented as a bug fix in its own right, not bundled silently into the unification changelog where consumers might miss that a previously-silent misbehavior is changing.

---

**06-P35.** **Root cause:** a destructor throwing while stack unwinding from a *different*, already-in-flight exception is already underway — the C++ runtime has no defined mechanism for having two simultaneously "active" exceptions, so it calls `std::terminate` unconditionally the moment a second exception tries to propagate during unwinding triggered by a first one. **Fix pattern going forward:** adopt a hard rule that destructors never let exceptions escape — any operation inside a destructor that could throw must be wrapped in its own `try`/`catch(...)` that swallows (and, if needed, logs) rather than propagates; C++11 onward reinforces this by making destructors implicitly `noexcept` by default (06-P01), so an escaping exception from a destructor that doesn't override that default calls `std::terminate` even in the *non*-unwinding case too. **Auditing an existing codebase without inspecting every destructor by hand:** since destructors are implicitly `noexcept` unless explicitly marked otherwise or a member/base is potentially-throwing, a compiler warning/static-analysis pass that flags any destructor invoking a non-`noexcept` function (or containing a bare `throw`) is directly automatable — many static analyzers (and `-Wterminate` on GCC) already implement exactly this check, so the practical audit step is "turn that warning on and treat it as an error," not a manual per-file review.

---

**06-P36.** **Design:** add a non-throwing variant with a signature like `std::expected<Result, ErrorCode> try_do_operation(...) noexcept`, alongside the existing throwing `Result do_operation(...)`. **Shared implementation direction:** implement the *throwing* version in terms of the *non-throwing* one (`do_operation` calls `try_do_operation` and throws if it returns an error), not the reverse — because building "never throws" out of something that might throw is fragile (you'd need to wrap every call site in `try`/`catch` internally, which reintroduces exactly the cost the new caller is trying to avoid for their variant), whereas building "throws on failure" out of something that already reports failure without throwing is a trivial, cheap wrapper. **Concrete risk of maintaining two parallel entry points:** behavioral drift — a future bug fix, edge case handling, or new failure mode added to one path but forgotten in the other (since they're now two pieces of code, even if one calls the other, the *translation* layer itself is a place logic can silently diverge or be updated for one path and not the other) — mitigated but not eliminated by the "one implemented in terms of the other" discipline above.

### Level 7 — Principal Reasoning

**06-P37.** **At least three missing requirements:** (1) the expected or measured frequency of malformed records — a 1-in-a-billion occurrence changes the cost/benefit of any change entirely differently than 1-in-a-thousand; (2) whether the ingestion pipeline's intermediate stages are themselves independently verified to be exception-safe end-to-end, since that determines whether a `try`/`catch` wrapper at the call site is even sufficient (see below); (3) the acceptable behavior on the rare case a malformed record's failure genuinely does indicate a systemic problem (corrupted upstream data, a schema migration gone wrong) rather than a truly isolated bad record — should the pipeline ever escalate to a full stop rather than skip-and-continue indefinitely? **Tentative recommendation (assuming reasonable answers — moderate frequency, unverified downstream exception-safety, and a desire for occasional escalation):** add a non-throwing variant for the specific, well-understood "malformed record" failure mode only (not a blanket policy change across the whole library), leaving the library's default throwing behavior in place for genuinely unexpected failures — this lets the ingestion team opt into cheap, explicit handling for the one failure mode they've told you about, without silently changing behavior for every other consumer of the library who has not asked for this. **Why "just wrap every call in `try`/`catch`" is not automatically sufficient:** first, under some ABIs, part of the cost of exceptions is paid on the *non-throwing* path too (stack-unwind table setup/lookup structures the compiler must still maintain even when no exception is ever thrown — Ch12's territory), so "we only throw rarely" doesn't fully neutralize their measured latency concern the way it would if the cost were purely per-throw. Second, and more importantly: a `try`/`catch` at their call site only protects against an exception that actually propagates cleanly out to that point — if an inner stage's exception safety is unverified, a thrown exception mid-stage could leave *that stage's own internal state* corrupted (broken invariants, partial mutation) well before unwinding ever reaches the outer `catch`, and catching the exception there does nothing to repair whatever the inner stage already broke on its way out. A `try`/`catch` wrapper is not a substitute for the callee actually providing at least the basic guarantee.

---

**06-P38.** **Two structurally different resolutions:** **(a)** Add a separate, explicitly-named entry point — e.g. `insert_unchecked` or `insert_fast` — documented plainly as providing only the basic guarantee (or even weaker), for consumers who have independently proven their element type's relevant operations are `noexcept` and who accept that tradeoff explicitly by choosing to call a differently-named function; the original `insert` keeps its two-year-old strong guarantee, untouched, for everyone who has built correctness arguments on it. **(b)** A trait-based `if constexpr` fast path inside `insert` itself: detect (via `std::is_nothrow_move_constructible` or a project-specific trait) whether the element type's operations are already known-`noexcept`, and only pay the extra defensive copy when they aren't — this preserves both the *name* `insert` and its *documented guarantee* (the guarantee is still upheld either way, just achieved differently depending on the type), with zero API surface change and zero risk to existing callers' correctness arguments. **Recommendation:** (b), because it avoids ever exposing a weaker-guarantee entry point under a name consumers might reach for without reading documentation carefully, and it benefits *every* nothrow-movable-element consumer automatically rather than only those who go out of their way to opt in. **Maintenance/documentation burden taken on:** the trait-detection logic itself becomes something that must be kept correct as the type traits or the element type's actual behavior evolve — a type that is nothrow-movable today but has that removed in a later revision would silently regress `insert`'s guarantee for existing code using it, with no compiler error to catch the regression, since the fast/slow path selection is invisible to callers. **What to verify before committing to (b):** that `is_nothrow_move_constructible` (or whatever trait is chosen) actually correlates with true behavioral non-throwing for every realistic element type this container is used with in practice — a type that lies about its `noexcept`-ness (marks a constructor `noexcept` while still having some undocumented, rarely-hit throwing path) would silently defeat the whole scheme, exactly as flagged as a real risk in 06-P23.

## Integration Challenge Solution — 06-IC1

**1. The actual inconsistency.** `load_settings` reports three failure modes through two incompatible channels: "file not found" is reported via an in-band sentinel return value (`-1`), while "file exists but is malformed" is reported by throwing `std::runtime_error` — and a caller reading only the declared signature `int load_settings(const char* path)` has no way to know from the signature alone that a `try`/`catch` is *ever* required at all, since nothing about "returns `int`" suggests the function can throw. A caller who checks only for `-1` and doesn't wrap the call in `try`/`catch` will have their program terminate (or propagate an uncaught exception up the stack) the first time it hits a malformed file, having had no signal from the API surface that this was possible.

**2. Unified policy.** Replace both channels with a single `std::expected<Settings, LoadError> load_settings(const char* path)`, where:
```cpp
enum class LoadError { FileNotFound, MalformedFile, PermissionDenied };
```
`expected` is the right choice here (rather than, say, `optional<Settings>`) specifically because the caller genuinely needs to distinguish *which* of these conditions occurred — "not found" is often recoverable by falling back to defaults, while "malformed" often indicates real corruption worth surfacing to a user or logging distinctly; collapsing both into `optional`'s undifferentiated "nothing" would lose exactly the information a real caller needs to act correctly. No exception-throwing path is preserved in the new design: none of these three conditions ("not found," "malformed," "permission denied") represent a genuinely exceptional, "something has gone deeply wrong in a way normal control flow shouldn't have to handle" situation — they're all routine, expected outcomes of "load some settings from a path that might not be there or might be corrupt," matching this chapter's Common Misconception 5 distinction between reserving exceptions for the truly exceptional and using an error channel for routine, anticipated failure.

**3. Corrected signature and sketch.**
```cpp
std::expected<Settings, LoadError> load_settings(const char* path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return std::unexpected(std::filesystem::exists(path) ? LoadError::PermissionDenied
                                                               : LoadError::FileNotFound);
    }
    Settings s;
    if (!parse_settings(in, s)) {              // parse_settings itself never throws — returns bool
        return std::unexpected(LoadError::MalformedFile);
    }
    return s;
}
```
Original condition 1 ("file not found," previously `-1`) maps to `LoadError::FileNotFound`. Original condition 2 ("malformed file," previously a thrown `std::runtime_error`) maps to `LoadError::MalformedFile`, now reported through the same channel instead of an exception. The previously-undetected "permission denied" case (which the original likely miscategorized as plain "not found," since both would cause `fopen`/`ifstream` to fail to open) is given its own distinct value, since it's a genuinely different condition a caller might want to handle differently (e.g., retry with elevated privileges vs. fall back to defaults).

**4. Migration plan.** Introduce the new `load_settings` under a temporary distinct name (e.g., `load_settings_v2` or, more idiomatically, place it in a way the old name can be preserved as a deprecated shim):
```cpp
[[deprecated("use the std::expected-returning load_settings; will be removed in v3.0")]]
int load_settings(const char* path) {
    auto r = load_settings_v2(path);
    if (!r.has_value()) {
        if (r.error() == LoadError::FileNotFound) return -1;
        throw std::runtime_error("failed to load settings: malformed or inaccessible file");
    }
    apply_settings(*r);   // preserve whatever the old function's side effect was
    return 0;
}
```
This transitional shim reproduces the old function's exact two-channel behavior (sentinel for not-found, throw for anything else) while the actual logic lives entirely in the new, unified `load_settings_v2`, so there is exactly one implementation to maintain, not two duplicated parsers. Existing callers keep compiling and behaving identically (with a deprecation warning nudging them to migrate) for one full release cycle (or per team convention, two), after which the deprecated overload is deleted — the same "wrapper-implemented-in-terms-of-the-new-thing, given a defined sunset window" pattern used in 06-P34's account-error unification and 06-P36's throwing/non-throwing pairing, and a smaller-scale rehearsal of the same reasoning BC-2 exercises at full library scale.
