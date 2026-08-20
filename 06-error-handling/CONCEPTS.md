# Chapter 06 — Error Handling and API Failure Design

> Prerequisites: [Chapter 02](../02-lifetime-raii/CONCEPTS.md) (RAII is the precondition for exception safety), [Chapter 03](../03-value-categories/CONCEPTS.md) (`noexcept` and move selection), [Chapter 05](../05-generic-programming/CONCEPTS.md) (generic error propagation, monadic operations).
> This chapter is about *deciding how a function reports failure* and *what the caller is guaranteed when it does* — not just which mechanism (exception vs. code vs. `optional`) to reach for, but the exception-safety contract your code upholds once you've chosen one.

## Crash Course

### Exceptions and Stack Unwinding

Throwing an exception abandons the current execution path and searches outward through enclosing scopes for a matching `catch`. Every stack frame between the `throw` and the `catch` is *unwound*: each local object with automatic storage duration in those frames has its destructor run, in reverse order of construction — this is precisely why RAII (Ch02) is the mechanism that makes exceptions safe to use at all. A resource held by a raw handle with no destructor leaks silently across a throw; a resource held by an RAII wrapper is released automatically as unwinding passes through its scope.

```cpp
void f() {
    std::lock_guard lock(mtx);      // released automatically during unwinding
    std::vector<int> v(1000);       // destroyed automatically during unwinding
    throw std::runtime_error("boom");
}
```

### `noexcept` and Its Effect on Move Selection

`noexcept` on a function is a compile-time-checkable promise that it will not throw. Violating it (throwing from a function marked `noexcept`) calls `std::terminate` immediately — unwinding does not even begin, because the promise was relied upon by callers who may not have set up handling for it.

The practically consequential use of `noexcept` is on move constructors: `std::vector` (and other containers) will only use a type's move constructor during reallocation if that move constructor is `noexcept`. If it isn't, the container falls back to copying during reallocation — silently, with no compile error — specifically because a throwing move mid-reallocation would leave the container in a state it cannot safely unwind from (some elements already moved into new storage, some not, and the old storage partially torn down). This is `move_if_noexcept`'s rule, previewed in Ch03 and formalized here.

### Exception Guarantees

Four (formally three, occasionally elaborated to four) levels describe what a function promises about program state *after* an exception it might throw actually propagates out:

- **No guarantee** — anything, including corrupted invariants, is possible after a throw. Effectively unusable in shared/library code.
- **Basic guarantee** — no resources are leaked and all invariants remain valid, but the object's specific state after the throw is unspecified (it's in *some* valid state, not necessarily the pre-call one).
- **Strong guarantee** — the operation either completes fully or has no visible effect at all (commit-or-rollback semantics), as if it had never been attempted.
- **Nothrow guarantee** — the operation is guaranteed not to throw at all (typically enforced with `noexcept`).

The strong guarantee is usually achieved via the "construct the new state fully in a temporary first, then swap it into place with a nonthrowing operation" pattern (copy-and-swap), because swapping two already-fully-constructed objects can be made `noexcept` even when constructing either one might not be.

### Error Codes and `std::error_code`/`error_category`

An error code is a plain value (often an `enum` or `int`) a function returns instead of throwing. `std::error_code` pairs an integer value with an `error_category` (identifying *which* domain the value belongs to — POSIX `errno`-style, a platform API, or an application-defined category), so that two error codes with the same numeric value from different categories are never confused with each other, and so that a code can be compared against a *portable* condition (`std::errc`) regardless of which underlying category produced it.

### `optional`, `variant`, and `expected` as Error Channels

`std::optional<T>` communicates "a `T`, or nothing" — appropriate when failure carries no information beyond "it didn't work" (e.g., "not found"). `std::variant<T, Error>` can hold either the success value or an error value, but forces the caller to `visit`/`holds_alternative`-check rather than offering purpose-built error-handling ergonomics. `std::expected<T, E>` (C++23) is purpose-built for exactly this: it holds either a `T` or an `E`, offers monadic operations (`.and_then`, `.or_else`, `.transform`) for chaining fallible operations without manual unwrapping at each step, and — unlike a thrown exception — makes the possibility of failure visible in the function's *signature*, at zero throw/unwind cost on the failure path.

```cpp
std::expected<int, std::string> parse_int(std::string_view s);

auto result = parse_int(s).and_then([](int n) { return parse_int_squared(n); });
```

### Error Propagation and API Error-Design

"Error propagation" is the discipline of a function that calls a fallible operation deciding what to do with *that* operation's failure — rethrow, translate into its own error type, handle locally, or propagate unchanged. A library's API error design is the deliberate, stated *policy* for which of these channels (exception vs. code vs. `optional`/`expected`) a given API surface uses, and — critically — that the policy is applied *consistently* across the surface, since a caller who has to remember "this function throws but that one returns an error code, and this third one returns `optional` for the exact same failure mode" cannot write correct, uniform error-handling code against the library.

## Common Misconceptions

1. **"`noexcept` prevents a function from throwing."** No — it's a promise the compiler does not exhaustively verify (it can, in some cases, warn, but in general it cannot prove non-throwing for arbitrary code); if the function throws anyway, the result is `std::terminate`, not a caught exception. `noexcept` is a contract with consequences for violation, not a language-enforced guarantee.

2. **"The basic guarantee means nothing bad happened."** No — the basic guarantee only promises no leaks and no broken invariants; the object's *value* after a caught exception may be different from before the call (e.g., partially modified), and code that assumes "it's back to how it was" under only a basic guarantee is relying on something not actually promised.

3. **"`std::optional` and `std::expected` are interchangeable ways to say 'this might fail.'"** No — `optional<T>` communicates *presence*, discarding any information about *why* something is absent; `expected<T,E>` carries the actual error value, letting a caller inspect, log, or translate the specific failure. Reaching for `optional` when callers genuinely need to know *why* forces them to invent an out-of-band way to recover that information.

4. **"A caught `catch (...)` handles every possible failure safely."** No — `catch (...)` catches the exception, but doesn't tell you *anything* about what failed or whether the program's invariants (or the object being operated on) are even in a guaranteed-valid state afterward; swallowing an exception you don't understand is often worse than letting it propagate to a handler that does.

5. **"Returning an error code is always slower/uglier than throwing, so exceptions are strictly the 'modern' choice."** No — exceptions carry real cost specifically on the *throw* path (stack unwinding, RTTI-based type matching at each candidate `catch`) and, on some ABIs/compilers, a modest cost even on the non-throwing path via exception-table bookkeeping (Ch12's territory). For failure modes that are frequent, expected, and part of normal control flow (e.g., "key not found" in a hot lookup loop), an error-code/`optional`/`expected` channel is frequently both faster and clearer than an exception — the choice is a design decision about *how exceptional the failure actually is*, not a modernity contest.

6. **"If a move constructor is `noexcept(false)`, `vector` will still try to move first and only copy if the move actually throws."** No — the container decides whether to move or copy *before* attempting the operation, purely from the type's `noexcept` specification, not from whether a throw actually occurs at runtime; a non-`noexcept` move constructor causes the container to copy unconditionally during reallocation, even if that specific move would never have thrown in practice.

7. **"`std::error_code`'s numeric value alone is enough to check what happened."** No — the same integer value means different things in different `error_category`s; comparing raw values across categories (or without checking the category at all) risks matching an unrelated error that happens to share the same number. Comparisons should go through `error_code::operator==` (which checks the category too) or against a portable `std::errc` condition via `.default_error_condition()`/category-aware comparison.

## Quick Checks

**06-QC1.** During stack unwinding from a `throw`, in what order are local objects' destructors run relative to the order they were constructed?

**06-QC2.** If a `noexcept` function throws anyway, is the exception caught by an enclosing `catch`, or does something else happen?

**06-QC3.** Why does `std::vector` refuse to use a type's move constructor during reallocation unless that move constructor is marked `noexcept`?

**06-QC4.** What's the difference between the basic guarantee and the strong guarantee, specifically regarding the object's *value* after a caught exception?

**06-QC5.** Why does copy-and-swap achieve the strong guarantee even when the type's own copy constructor might throw?

**06-QC6.** Why can't two `std::error_code`s with the same numeric value but different `error_category`s be safely assumed to mean the same thing?

**06-QC7.** What does `std::expected<T, E>` give a caller that `std::optional<T>` cannot, when the caller needs to know *why* an operation failed?

**06-QC8.** Why might an error-code-based API be faster than an equivalent exception-based API specifically for a *frequent, expected* failure mode?

## Problems

### Level 1 — Recognition

**06-P01.** For a function `void f()` with no `noexcept` specifier at all, is it treated as `noexcept(true)` or `noexcept(false)` by default? Does this differ for a destructor?

**06-P02.** State which exception-guarantee level (no guarantee / basic / strong / nothrow) each of these descriptions matches: (a) "the vector's `push_back` either fully succeeds or the vector is unchanged"; (b) "if this throws, some elements may have been processed and some not, but the container's internal invariants still hold and nothing leaked"; (c) "this function is declared in a way the compiler can rely on to never propagate an exception out of it."

**06-P03.** Given `std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);`, is `ec`'s underlying numeric value alone (without knowing its category) sufficient to know what it means, or must the category also be considered?

**06-P04.** Which of these is/are legal C++: throwing an exception from inside a destructor; throwing an exception from inside a function marked `noexcept`; catching an exception by value when the exception type is polymorphic (has virtual functions)?

### Level 2 — Prediction

**06-P05.**
```cpp
struct Resource {
    Resource() { std::cout << "acquire\n"; }
    ~Resource() { std::cout << "release\n"; }
};
void f() {
    Resource r1;
    Resource r2;
    throw std::runtime_error("fail");
}
int main() {
    try { f(); } catch (...) { std::cout << "caught\n"; }
}
```
Predict the exact output, including the order in which `r1`/`r2`'s destructors run relative to each other and relative to `"caught"` being printed.

**06-P06.**
```cpp
struct Movable {
    Movable() = default;
    Movable(Movable&&) noexcept(false) { /* ... */ }
    Movable(const Movable&) { /* ... */ }
};
std::vector<Movable> v(3);
v.reserve(100);   // forces reallocation
```
During the reallocation triggered by `reserve`, does `vector` move or copy the existing three `Movable` elements into the new storage? Justify from the type's declared `noexcept` specification alone, not from whether a throw would actually occur.

**06-P07.**
```cpp
void log_error() noexcept {
    throw std::runtime_error("logging failed");
}
int main() {
    try { log_error(); } catch (const std::exception& e) { std::cout << "caught: " << e.what(); }
}
```
Predict what actually happens when `main` runs — does `"caught: logging failed"` print, or something else entirely?

**06-P08.**
```cpp
std::optional<int> find_index(const std::vector<int>& v, int target) {
    for (size_t i = 0; i < v.size(); ++i) if (v[i] == target) return i;
    return std::nullopt;
}
auto r = find_index({1,2,3}, 5);
std::cout << r.value_or(-1);
```
Predict the output, and state what information is lost by using `optional<int>` here versus, say, `expected<int, std::string>`.

**06-P09.**
```cpp
class Account {
    int balance_ = 100;
public:
    void withdraw(int amount) {
        balance_ -= amount;             // (1)
        if (balance_ < 0) throw std::runtime_error("overdraft");  // (2)
    }
    int balance() const { return balance_; }
};
Account a;
try { a.withdraw(150); } catch (...) {}
std::cout << a.balance();
```
Predict the output. Does this implementation provide the strong guarantee for `withdraw`? Justify by walking through what `balance_` holds at each numbered line when the exception is thrown.

**06-P10.**
```cpp
std::error_code ec1 = std::make_error_code(std::errc::invalid_argument);
std::error_code ec2{static_cast<int>(std::errc::invalid_argument), std::generic_category()};
std::error_code ec3{static_cast<int>(std::errc::invalid_argument), std::system_category()};
std::cout << (ec1 == ec2) << (ec1 == ec3);
```
Predict the output. Explain why comparing `error_code`s requires matching category, not just matching numeric value, and what (if anything) guarantees `ec1 == ec2` specifically.

**06-P11.**
```cpp
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("division by zero");
    return a / b;
}
auto r = divide(10, 0).and_then([](int x) { return divide(x, 2); });
std::cout << (r.has_value() ? std::to_string(*r) : r.error());
```
Predict the output. Does `.and_then`'s lambda ever actually execute here? Justify from what `.and_then` is documented to do when its receiver already holds an error.

**06-P12.**
```cpp
void process() {
    std::vector<int> v = {1, 2, 3};
    std::unique_ptr<int> p = std::make_unique<int>(42);
    throw std::logic_error("mid-process failure");
}
```
Are `v` and `p`'s resources (heap-allocated buffer, heap-allocated `int`) leaked when `process()` throws? Justify via the unwinding/RAII mechanism, not by inspecting whether a `catch` exists anywhere.

### Level 3 — Implementation

**06-P13.** Write a function `template<typename T> T parse_or_throw(std::string_view s)` (for `T` restricted to `int`/`double` via a simple `if constexpr`/static check, or via overloads) that throws a `std::invalid_argument` with a message naming the exact input string when parsing fails, and returns the parsed value on success. State which exception guarantee this function provides and why (hint: think about whether it has any observable state of its own to leave partially modified).

**06-P14.** Implement a `class TransactionLog` with an `append(std::string_view entry)` method that provides the **strong guarantee**: if appending would exceed a fixed capacity (throwing `std::length_error`), the log's existing contents must be completely unchanged. Use the copy-and-swap (or equivalent commit-or-rollback) pattern explicitly, and explain in a comment which specific operation in your implementation is the one that must be `noexcept` for the guarantee to actually hold.

**06-P15.** Write `std::error_code make_parse_error(int line, int column)` returning a custom `std::error_code` backed by an application-defined `error_category` (implement the category's `name()` and `message(int)`), rather than reusing `std::generic_category()`. Explain what distinguishing a custom category buys you over reusing a generic one (specifically: what a caller comparing against `std::errc` values would incorrectly conclude if your application-specific codes were expressed through `generic_category()` instead).

**06-P16.** Implement `std::expected<double, std::string> safe_sqrt(double x)` (returning an error for negative `x`) and chain it with `.and_then`/`.transform` into a small pipeline that computes `safe_sqrt(x).transform([](double r){ return r * 2; })`, demonstrating both the success path (positive `x`) and the short-circuiting failure path (negative `x`, where `.transform`'s lambda never runs).

**06-P17.** Write a function `template<typename F> auto retry_n(F&& f, int attempts)` that calls a fallible callable `f` (which may throw) up to `attempts` times, returning `f`'s result on the first success and rethrowing the *last* exception seen if every attempt fails. State precisely what guarantee your `retry_n` itself provides about how many times `f`'s *side effects* (if any) may have run by the time it either returns or rethrows.

**06-P18.** Implement a small `noexcept`-correct `swap` free function for a hand-rolled `class Buffer` (owns a `unique_ptr<char[]>` and a `size_t`), and use it inside a copy-and-swap-based copy-assignment operator for `Buffer`. Explain why the `swap` itself being genuinely `noexcept` (not just labeled so) is a structural precondition for copy-assignment's strong guarantee here, tying back to 06-QC5.

**06-P19.** Write `template<typename T, typename E> std::variant<T, E> to_variant(std::expected<T, E> e)` and its inverse `template<typename T, typename E> std::expected<T, E> to_expected(const std::variant<T, E>& v)` (the latter needs a rule for which alternative means "success" — state and justify your convention, e.g. index 0 is always the success type). Demonstrate both round-trip correctly for one success and one failure case, and state one piece of ergonomic capability `expected` has that a raw `variant<T, E>` doesn't (without writing your own wrapper).

**06-P20.** Implement `class ScopedErrorContext` (an RAII type) that, on construction, pushes a string onto a thread-local (or simply global, for this exercise) "error context stack," and on destruction pops it — such that if an exception is thrown while one or more `ScopedErrorContext`s are alive, a handler can inspect the stack (before it unwinds further) to see the nested "where in the operation did this happen" trail. Demonstrate nesting two contexts and an exception thrown inside the innermost one, showing the stack's contents are still available for inspection inside the immediately enclosing `catch` before the outer `ScopedErrorContext` destructs.

### Level 4 — Debugging

**06-P21.** [DEBUG]
```cpp
class FileHandle {
    FILE* f_;
public:
    explicit FileHandle(const char* path) : f_(fopen(path, "r")) {
        if (!f_) throw std::runtime_error("open failed");
    }
    ~FileHandle() { fclose(f_); }
};
void process(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        FileHandle fh(p.c_str());   // may throw for any given path
        // ... use fh ...
    }
}
```
A reviewer claims this leaks file handles when one path in the middle of the list fails to open. Walk through exactly what happens (compile-clean, this is a runtime-reasoning question) and state whether the reviewer is correct, explaining precisely why or why not via the unwinding/RAII mechanism.

**06-P22.** [DEBUG]
```cpp
class Stack {
    std::vector<int> data_;
public:
    void push_all(const std::vector<int>& items) {
        for (int x : items) {
            if (x < 0) throw std::invalid_argument("negative value");
            data_.push_back(x);
        }
    }
};
```
`push_all` is documented as providing the strong guarantee ("either all items are pushed, or none are"). Identify precisely why the implementation as written does *not* actually satisfy that documented guarantee, and fix it using copy-and-swap (or an equivalent commit-or-rollback restructuring) so the documented guarantee becomes true.

**06-P23.** [DEBUG]
```cpp
struct Widget {
    Widget(Widget&&) { /* throws under some rare internal condition */ }
    Widget(const Widget&) = default;
};
std::vector<Widget> v;
v.reserve(10);
for (int i = 0; i < 5; ++i) v.emplace_back();
v.reserve(20);   // triggers reallocation
```
A reviewer expects this to be fast (moves during reallocation) but profiling shows it's copying every element instead. Identify the missing detail in `Widget`'s move constructor's declaration that causes this, and fix it — then state precisely why the "rare internal condition" that might throw makes marking it `noexcept` a genuine correctness question, not just a performance one.

**06-P24.** [DEBUG]
```cpp
std::expected<int, std::string> lookup(const std::map<std::string, int>& m, const std::string& key) {
    auto it = m.find(key);
    return it->second;   // bug
}
```
Identify the bug (there's a missing failure path, not a syntax error) and explain precisely what happens at runtime when `key` isn't present in `m`, then fix it to correctly return an error via `std::unexpected` in that case.

**06-P25.** [DEBUG]
```cpp
void save_config(const Config& cfg) {
    std::ofstream out("config.tmp");
    out << serialize(cfg);
    out.close();
    std::rename("config.tmp", "config.dat");   // may fail; return value ignored
}
```
A reviewer flags that this function's actual failure-reporting policy is inconsistent with itself: `std::ofstream`'s stream operations fail silently (no exception by default, no checked return value used here) while the function's *name* and typical caller expectation imply a reliable save-or-report-failure contract. Identify every point in this function where a failure could occur but currently goes unreported, and restate the function's error policy explicitly (pick either "throws on any failure" or "returns a `std::expected<void, ErrorType>`") and show the corrected implementation for your chosen policy.

**06-P26.** [DEBUG]
```cpp
class Pool {
    std::vector<std::unique_ptr<Widget>> items_;
public:
    Widget* acquire() {
        auto w = std::make_unique<Widget>();   // may throw bad_alloc
        items_.push_back(std::move(w));         // may also throw
        return items_.back().get();
    }
};
```
A reviewer asks: "if `items_.push_back` throws after the `make_unique` succeeded, is the newly-allocated `Widget` leaked?" Answer precisely (walking through what happens to `w`, a local `unique_ptr`, when `push_back` throws partway through), and state whether `acquire` as written already provides the basic guarantee, the strong guarantee, or neither — justify from `Pool::items_`'s state after a caught exception from `push_back`.

**06-P27.** [DEBUG]
```cpp
enum class ParseError { InvalidFormat, OutOfRange };
struct ParseErrorCategory : std::error_category {
    const char* name() const noexcept override { return "parse_error"; }
    std::string message(int ev) const override {
        switch (static_cast<ParseError>(ev)) {
            case ParseError::InvalidFormat: return "invalid format";
            case ParseError::OutOfRange: return "out of range";
        }
        return "unknown";
    }
};
std::error_code make_error_code(ParseError e) {
    static ParseErrorCategory cat;   // local static inside a free function
    return {static_cast<int>(e), cat};
}
```
A reviewer flags a subtle lifetime bug in `make_error_code`'s construction of the returned `error_code`. Identify precisely what's wrong (hint: consider what `std::error_code`'s constructor actually stores — a reference/pointer to the category object, or a copy of it — and what that implies about `cat`'s storage duration given it's a function-local `static`), and state whether it's actually a bug at all once you've confirmed the storage duration, or a common false alarm.

**06-P28.** [DEBUG]
```cpp
template<typename F>
auto call_and_log(F&& f) noexcept {
    try {
        return f();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return decltype(f())();   // default-construct a "safe" fallback
    }
}
```
Identify two distinct problems with this code: one that's a compile error for certain `F`s (not all), and one that's a subtler design/safety problem even for `F`s where it does compile. (Hint for the second: what does marking this `noexcept` actually promise, and is that promise now trivially true and therefore meaningless, or is there a remaining way this function can still violate it?)

### Level 5 — Integration

**06-P29.** Design and implement a small `Result<T, E>` template (your own, not `std::expected` — this exercise is about building the same idea from scratch to internalize it) supporting `.value()` (throws on error access), `.error()` (throws on value access), `.has_value()`, `.map(F)` (transforms the success value, passing through an existing error unchanged), and `.map_error(F)` (transforms the error value, passing through an existing success unchanged). Demonstrate a small pipeline chaining at least two `.map` calls, and state one design decision where you deliberately diverged from (or matched) `std::expected`'s actual behavior, and why.

**06-P30.** Take a function `int legacy_divide(int a, int b)` that currently returns `-1` on division-by-zero (an in-band error value indistinguishable from a legitimate result of `-1`) and redesign it into `std::expected<int, DivideError> safe_divide(int a, int b)` where `DivideError` is a proper enum, **without breaking existing callers of `legacy_divide`** — implement both, with `legacy_divide` implemented *in terms of* `safe_divide` (not duplicated logic), and state precisely what information legitimately gets lost translating `safe_divide`'s result back down into `legacy_divide`'s in-band-error contract.

**06-P31.** Implement a small `class ScopedTransaction` for a hand-rolled in-memory key-value store (`std::map<std::string,std::string>`) that supports `set(key, value)` calls while the transaction is open, and either `commit()` (applies all buffered changes to the store) or, if the `ScopedTransaction` is destroyed without an explicit `commit()` (including via an exception unwinding through its scope), automatically rolls back — no buffered change is ever applied to the underlying store. Demonstrate an exception thrown mid-transaction and show the store is unaffected afterward.

**06-P32.** Design a small logging library's error-reporting policy from scratch: `Logger::write(std::string_view msg)` should never throw (loggers that can throw are notoriously dangerous, since they're often called from other error-handling paths — a throwing logger inside a `catch` block that's itself trying to log the original error is a classic disaster), but a full disk or a broken output stream is a real failure a caller might legitimately want to detect. Design and implement a `noexcept`-total API (no exceptions ever escape `write`) that still surfaces failure information to a caller who wants it, without forcing every caller to handle it. State explicitly which of `expected`, an error callback, a "check `.last_error()` afterward" pattern, or some combination, you chose, and why the "never throw" constraint specifically rules out `expected`'s otherwise-idiomatic use here if the constructing/moving of the error payload itself could throw (or explain why it doesn't, for your chosen `E`).

**06-P33.** Take a hand-rolled `class Matrix` (owns a `std::vector<double>` and dimensions) and implement `operator+` for it providing the **strong guarantee**: if the two matrices' dimensions don't match, the operation throws `std::invalid_argument` and neither operand is modified (this should already hold trivially for a `const`-correct `operator+` returning a new object) — but additionally implement `Matrix::operator+=` providing the strong guarantee too, where a dimension mismatch must leave the left-hand operand completely unchanged, and explain specifically why `operator+=`'s in-place nature makes achieving the strong guarantee a genuinely different (harder) problem than `operator+`'s already-easy case.

### Level 6 — Production

**06-P34.** A code review of a payments-adjacent library flags that three different functions report "insufficient funds" three different ways: one throws `std::runtime_error`, one returns `-1` (an in-band sentinel also used to mean "account not found"), and one returns `bool` via an out-parameter. Propose a single, unified error-reporting policy for the entire library's public surface, covering: which mechanism you'd standardize on and why (tie your answer to whether "insufficient funds" is an *expected, frequent, part-of-normal-flow* condition or a genuinely exceptional one, per this chapter's Common Misconception 5), how you'd migrate the three existing functions to the unified policy without breaking their current callers in one release, and what you'd do about the `-1` sentinel's pre-existing ambiguity (which is a correctness bug independent of the unification effort).

**06-P35.** A production service's crash logs show `std::terminate` being called from inside a destructor, with no exception ever visibly caught anywhere in application code. Explain the most likely root cause (a destructor throwing during stack unwinding that was already in progress from a different exception — "throwing from a destructor during unwinding" is one of the few situations that unconditionally calls `std::terminate`, since the runtime has no defined way to have two simultaneous active exceptions), propose a concrete code-level fix pattern that prevents this class of bug going forward (a policy on what destructors are and aren't allowed to do), and state how you'd audit an existing large codebase for other latent instances of the same bug class without inspecting every destructor by hand (hint: think about what a static-analysis or compiler-warning-level check could flag automatically, e.g. any destructor calling code that isn't itself `noexcept`).

**06-P36.** Your team's public library exposes a function whose current contract is "throws `std::runtime_error` on any I/O failure." A major consuming team, writing latency-sensitive code, asks you to add a non-throwing overload/variant for the same operation, since they've measured that exception handling on their hot path costs more than their latency budget allows even when no exception is actually thrown (Ch12 territory, referenced not owned — a correct one-sentence acknowledgment of this cost model is enough, not a full derivation). Design the non-throwing variant's signature and behavior, decide whether the throwing and non-throwing versions should share an implementation (and if so, which one is implemented in terms of the other, and why that direction rather than the reverse), and state one concrete risk of maintaining two parallel entry points for the same underlying operation going forward.

### Level 7 — Principal Reasoning

**06-P37.** You're the API owner for a widely-used internal library. A team building a new, distributed, high-throughput ingestion service is adopting your library, and they specifically ask you whether your library's error-reporting policy (currently, uniformly, "throws on any failure, no error codes anywhere") is appropriate for their use case, where a single malformed record among millions must be skippable without materially affecting throughput. The requirements you've been given are incomplete: no stated volume of expected malformed-record frequency, no stated tolerance for a rare full-service crash versus a guaranteed-recoverable-per-record failure mode, and no mention of whether their ingestion pipeline is itself exception-safe end-to-end (i.e., whether every intermediate stage already correctly unwinds without leaking or corrupting shared state). Identify at least three concretely missing requirements you would need answered before recommending a change, propose your tentative recommendation *assuming reasonable answers* (a specific stance on whether you'd add a non-throwing variant, change the library's default policy outright, or advise the consuming team to handle it entirely on their side without any library change), and explicitly justify why "just wrap every call in a `try`/`catch` at their call site and keep the library as-is" is not automatically a sufficient answer regardless of how the open questions resolve — tie the justification to a concrete point about exception cost being partly paid on the *non-throwing* path too under some ABIs (Ch12's territory, referenced not owned) and about what a `try`/`catch`-per-record wrapper does and doesn't protect against if an inner stage's exception safety is itself unverified.

**06-P38.** Your team's public library has, for two years, guaranteed the strong exception-safety guarantee on a core `Container::insert` operation — consumers have come to rely on this, some explicitly documenting their own code's correctness in terms of it. A performance investigation finds that achieving the strong guarantee costs a measurable, real overhead (an extra copy in the common, non-throwing case) that a specific class of consumers — those who can independently prove their own element type's operations never throw, and who are on an extremely hot path — would like eliminated. Reason through the tradeoff: propose at least two structurally different ways to resolve this tension (e.g., a separate, explicitly-named "weak guarantee, higher performance" entry point alongside the existing one; a trait-based `if constexpr` fast path that only activates the strong-guarantee machinery when the element type's operations aren't already known-`noexcept`; or declining the request and explaining why the strong guarantee should remain non-negotiable for this API), and for your recommended approach, explicitly identify what future maintenance burden or documentation burden you are taking on, and what you would need to verify (about the element types in question, or about the requesting team's actual usage) before committing to it — this is intentionally open, graded on reasoning quality per this chapter's Level 7 standard, not on matching one "correct" architecture.

## Integration Challenge — 06-IC1

A legacy function `int load_settings(const char* path)` currently reports failure three inconsistent ways depending on *which* failure occurs: it returns `-1` if the file doesn't exist, it throws `std::runtime_error` if the file exists but is malformed, and it silently returns a default-initialized (zeroed) settings-equivalent value — wait, restate: it returns an `int` status code where `0` means success, `-1` means "file not found," and it *also* throws for a malformed file — an inconsistency a caller has no way to anticipate without reading the implementation.

1. State the actual inconsistency precisely: which failure modes are reported via which of the three channels (return-code sentinel, exception, or a silent/undetectable case if you identify one), and why a caller who has only read the function's declared signature cannot reliably write correct error-handling code against it today.
2. Design one unified policy that could replace all three current behaviors, addressing: what specific type you'd return (an `std::expected<Settings, LoadError>` is a reasonable default choice, but justify it rather than assuming it), and why you did or didn't preserve *any* exception-throwing path at all in the new design.
3. Show the corrected `load_settings`'s signature and implementation sketch (you don't need a full working parser — sketch the structure, including where each of the three original failure conditions maps to in the new design).
4. State how you would migrate existing callers of the original three-way-inconsistent function without breaking them all at once in a single release — specifically addressing whether a transitional, deprecated overload of the old signature (implemented in terms of the new one) is a good idea here, and for how long you'd keep it. This is a smaller-scale rehearsal of the same migration reasoning BC-2 (per `PROJECT_ROADMAP.md`) exercises at full library scale.

## Chapter Projects

This chapter feeds directly into:

- **[P-2.4](../PROJECT_ROADMAP.md) Result/Error Propagation Library** — an `expected`-based, monadic, no-exceptions-on-the-hot-path error library draws directly on 06-P16's `.and_then`/`.transform` chaining and 06-P29's hand-rolled `Result<T,E>` design decisions.
- **[P-3.5](../PROJECT_ROADMAP.md) Declarative Command-Line Parser** — the parser's error-surface design (reporting which argument failed, and how) draws on 06-P15's custom `error_category` design and 06-P34's unified-error-policy reasoning.
