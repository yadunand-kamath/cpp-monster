# Chapter 01 — Solutions

> Do not read ahead of the problem you're checking. If you needed to read a solution to finish, mark that problem `◐ Assisted` in [`PROGRESS.md`](../PROGRESS.md), not `☑ Done`.

---

## Quick Check Answers

**01-QC1.** No. `int x;` at *namespace scope* has **static storage duration**, and objects with static storage duration are always **zero-initialized** before any other initialization runs. It's only a block-scope automatic-duration local (`int x;` inside a function) that's left indeterminate.

**01-QC2.** No. `constexpr` means "*can* be evaluated at compile time, and must be evaluable at compile time in a context that requires it" (e.g., an array bound, a `static_assert`, another `constexpr` initializer). Called as an ordinary expression like `f(5)` inside a normal (non-constexpr) function, the compiler is free to evaluate it at compile time or defer it to runtime — both are conforming.

**01-QC3.** Yes, but it's redundant in effect, not forbidden — `constinit const int x = f();` is legal (as of C++20, `const` variables need `constinit` only when you specifically want to force compile-time initialization order guarantees; note also that `constinit` cannot be combined with `constexpr` on the same declaration, since `constexpr` already implies compile-time initialization).

**01-QC4.** Scope: block scope (the name `counter` is not visible outside the function). Storage duration: static (it is initialized once and persists for the entire program, not re-created per call).

**01-QC5.** No. Reference-to-reference doesn't exist as a distinct type — this is exactly what reference collapsing (Ch03) formalizes, but at the Ch01 level, the short answer is: you cannot declare `int& & r`; it's ill-formed. (Reference collapsing becomes relevant only in template/`auto` deduction contexts, covered in Ch03.)

**01-QC6.** `auto x = 5;` deduces `int` — a plain value type, following template-deduction rules. `decltype(auto) x = 5;` also deduces `int` here, because `5` is a prvalue with no reference-ness to preserve; the two only diverge when the initializer is itself a reference expression (see QC7).

**01-QC7.** `decltype(i)` is `int` (the declared type of the named variable). `decltype((i))` is `int&` — parenthesizing turns it from "the declared type of the entity named `i`" into "the type of the *expression* `(i)`," and a parenthesized id-expression naming an lvalue is itself an lvalue expression, so `decltype` yields a reference type. `b`'s type is therefore `int&`.

**01-QC8.** At most **one**. The compiler will not chain two user-defined conversions (e.g., class A → class B → class C) to make an implicit conversion sequence work — you'd need an explicit intermediate cast.

**01-QC9.** No — `explicit` blocks *implicit* conversions and copy-initialization written as `MyType m = 5;`, but `MyType m = MyType(5);` is a direct-initialization of the temporary `MyType(5)` (which explicitly names the type), followed by copy-initializing `m` from that already-constructed `MyType` object — no implicit int→MyType conversion is required at any step. It compiles.

**01-QC10.** An unnamed, compiler-generated closure type unique to that lambda expression — every lambda has its own distinct type, even two lambdas with textually identical bodies. This is why you almost always hold a lambda via `auto` or type-erase it via `std::function`/a template parameter, never by writing out "the type."

**01-QC11.** Not necessarily — `std::function` typically has a small-buffer optimization for sufficiently small, nothrow-move-constructible callables, so a small capturing lambda may be stored inline. But this is implementation-defined behavior, not a standard guarantee, and a lambda with several captures easily exceeds the inline buffer and allocates.

**01-QC12.** Yes, by default. A non-`mutable` lambda's `operator()` is implicitly `const`, which is why value-captured variables can't be modified inside it unless you add `mutable`.

**01-QC13.** `std::strong_ordering` — every member (`int x`, `double y`)... wait, `y` is `double`, so the correct answer is `std::partial_ordering`, not `std::strong_ordering`: the compiler deduces the *weakest* category required by any member's own comparison, and a floating-point member forces `partial_ordering` (since NaN makes floating-point comparison not totally ordered) even though `x` alone would only need `strong_ordering`.

**01-QC14.** No. `<=>` and `==` are separate operations; a type must define (or `= default`) `operator==` explicitly to get it, even when `<=>` is already defined or defaulted.

**01-QC15.** Plausible, not an exaggeration — a compiler observing a code path that can only be reached through UB is permitted to assume that path is never actually taken, and may optimize (including eliminating checks or branches) on that assumption.

**01-QC16.** Implementation-defined — multiple sizes are permitted by the standard, but the implementation must document which one it uses, and that documented choice is then reliable for that specific implementation.

---

## Problem Solutions

### 01-P01

**Approach:** Match each declaration to the initialization category by its syntax, then apply the determinacy rule for automatic-duration scalars vs. class types.

**Reference Solution:**
| Declaration | Category | Determinate? |
|---|---|---|
| `int a;` | default-initialization | No — indeterminate value (UB to read) |
| `int b{};` | list-initialization (empty braces) | Yes — zero-initialized, then value `0` |
| `int c(7);` | direct-initialization | Yes — value `7` |
| `int d = 7;` | copy-initialization | Yes — value `7` |
| `std::string s;` | default-initialization | Yes — `std::string`'s default constructor runs; empty string. Determinate because default-init of a *class type* calls its default constructor, unlike a scalar. |

**Why It Works:** Default-initialization means different things for scalar types (no initializer, indeterminate) versus class types (the default constructor runs). This is the root of the "why is my int garbage but my string fine" confusion.

**C++ Considerations:** reading `a` before assigning it is undefined behavior, not merely "unspecified" — a compiler is permitted to assume it never happens and optimize accordingly.

---

### 01-P02

**Approach:** Storage duration is a property of *where and how* a variable is declared, independent of scope.

**Reference Solution:**
- `x` — automatic (destroyed at end of block).
- `y` (`static int y;`) — static (persists for program lifetime, despite block scope).
- `z` (`thread_local int z;`) — thread (one instance per thread, persists for the thread's lifetime).
- `*p` (from `new int`) — dynamic (you control destruction via `delete`).
- `g` (namespace scope) — static.

**Why It Works:** Storage duration and scope are orthogonal axes; `y` is the canonical example that a name can be scope-invisible outside its block while its storage persists across the entire program.

---

### 01-P03

**Approach:** Aggregate-initialization requires the type to be an aggregate: no user-declared constructors, no private/protected non-static data members (base-class restrictions relaxed since C++17 don't matter here since none of these have bases).

**Reference Solution:**
- `A a{1, 2};` — **legal**, `A` is an aggregate (no user constructors, all public members).
- `B b{1};` — **legal but not aggregate-initialization** — `B` has a user-declared constructor, so `{1}` is a call to `B(int)`, not aggregate-init. (It compiles, but for a different reason than it looks like.)
- `C c{1, 2};` — **illegal**. `C` has a private member (`x`), disqualifying it from being an aggregate, and it has no constructor that would make `{1, 2}` a valid constructor call either. Compile error.

**Common Wrong Approaches:** Assuming "it has braces, so it's aggregate-init" — the compiler decides that based on the type's shape, not the syntax at the call site.

---

### 01-P04

**Approach:** Read `const` right-to-left relative to `*`.

**Reference Solution:**
- `const int* p` — pointer to const int. Protects the **pointee** (`*p` can't be assigned through `p`); `p` itself can be reseated.
- `int* const p` — const pointer to int. Protects the **pointer itself** (`p` can't be reseated); `*p` can be freely modified.
- `const int* const p` — const pointer to const int. Protects **both**.

---

### 01-P05

**Approach:** Apply `auto` deduction rules (strip references/top-level cv-qualifiers by default) to each.

**Reference Solution:**
- `x` — `int` (deduced from literal `5`).
- `y` (`auto& y = x;`) — `int&` (explicit `&` in the declaration is preserved; it's binding to `x`).
- `z` (`const auto z = x;`) — `const int` (the explicit `const` is preserved; the deduced base type is `int`).

---

### 01-P06

**Approach:** Track `x`'s storage duration relative to when the reference is used.

**Reference Solution:** Well-formed (compiles), but returning a reference to a local automatic-duration variable is a **dangling reference** the instant the function returns — `x`'s storage no longer exists. `r`'s "relationship to `x`" after the call is undefined behavior to use at all; most compilers will also emit a warning (`-Wreturn-local-addr` / similar) precisely because this is almost always a bug.

**Why It Works:** the reference doesn't extend `x`'s lifetime — lifetime extension only applies to a reference bound directly to a temporary at the point of initialization, not to an arbitrary local returned by reference.

**C++ Considerations:** this is the canonical "compiles fine, UB at runtime" trap this chapter is building intuition against, ahead of Ch02's formal lifetime treatment.

---

### 01-P07

**Approach:** Distinguish aggregate default-initialization from zero-initialization.

**Reference Solution:** No. `Point p;` at block scope is **default-initialization** of an aggregate with scalar members, which for scalar members with no initializer means indeterminate value — same rule as `int a;`. Only `Point p{};` is guaranteed to zero-initialize `p.x` and `p.y`.

**Common Wrong Approaches:** Assuming structs "zero themselves out" by default the way some other languages do; C++ scalar members follow the same determinacy rule as standalone scalars.

---

### 01-P08

**Approach:** `char` is a scalar type; determine which conversion — `char`→`int` or `char`→`double` — ranks higher in overload resolution.

**Reference Solution:** `f(int)` is selected. `char` → `int` is an integral **promotion**, which ranks above `char` → `double`, a standard (floating) conversion. Promotions are preferred over conversions in overload resolution.

---

### 01-P09

**Approach:** Distinguish list-initialization's narrowing check from direct/copy-initialization, which permit narrowing silently.

**Reference Solution:**
1. `int a{3.14};` — **compile error**. List-initialization disallows narrowing conversions (`double`→`int` loses the fractional part), and the standard requires this to be diagnosed.
2. `int b = 3.14;` — compiles, `b == 3` (truncated). Copy-initialization permits narrowing; it's just a silent conversion.
3. `int c(3.14);` — compiles, `c == 3` (truncated). Direct-initialization, same as (2), also permits narrowing.

**Why It Works:** the brace form is deliberately stricter — it's one of the concrete safety benefits of preferring `{}`-init in new code, and exactly why this chapter emphasizes noticing which form is in use.

---

### 01-P10 [DEBUG]

**Approach:** Once a class has *any* user-declared constructor, it is no longer an aggregate, so brace-init calls a constructor instead of aggregate-initializing members positionally.

**Reference Solution:** `Widget(int id)` takes exactly one argument; `{1, 2}` doesn't match any constructor (`Widget` has no two-argument or initializer-list constructor), so the call is ill-formed — compile error, not a silent aggregate-init of two members that don't exist as public data anyway.

**Fix:** either add a matching constructor (`Widget(int, int)`), or, if aggregate-style positional init is actually wanted, remove the user constructor and rely on `Widget{1}` (single aggregate member init, if `id_` were public) — but note that adding *any* constructor is a permanent, one-way trade of aggregate-ness for validation/encapsulation, which is exactly the subject of 01-P39 and 01-P45.

---

### 01-P11

**Approach:** Distinguish "must be compile-time-evaluated" contexts from "may be either."

**Reference Solution:** Line (1), `int arr[square(3)];`, is an array-bound context — the standard **requires** a constant expression there, so `square(3)` must be evaluated at compile time; if it couldn't be (e.g., if the argument weren't a constant expression), this line would fail to compile. Line (2), `int y = square(x);` with `x` a runtime variable, is an ordinary expression context — `square` is *permitted* to run at either compile time or runtime, but since `x` isn't a constant expression, it necessarily runs at runtime here.

**Why It Works:** `constexpr` is a capability, not a mandate, except in the specific grammatical contexts (array bounds, `static_assert`, template arguments, other `constexpr`/`consteval` contexts) that themselves require a constant expression.

---

### 01-P12

**Approach:** `consteval` functions are "immediate functions" — *every* call must produce a constant expression, unconditionally.

**Reference Solution:** Does **not** compile. `triple(runtime_value)` cannot be evaluated at compile time because `runtime_value` is a runtime parameter with no constant value — and `consteval`, unlike `constexpr`, offers no fallback to runtime evaluation. This is a hard compile error, not a warning.

**Why It Works:** this is precisely the semantic difference from `constexpr` that QC2 tests — `constexpr` degrades gracefully to runtime; `consteval` does not degrade at all.

---

### 01-P13

**Approach:** Compare against the static-initialization-order fiasco that `static int global_counter = compute_initial_value();` would risk.

**Reference Solution:** A plain `static int global_counter = compute_initial_value();`, if `compute_initial_value()` were *not* guaranteed to run before `global_counter` is first used from another translation unit's static initializer, would risk the classic **static initialization order fiasco** — the relative order of dynamic initialization across TUs is unspecified. `constinit` forces the compiler to prove `compute_initial_value()` is evaluable as a constant expression and perform the initialization at compile time (or reject the program), eliminating the ordering hazard entirely — while still leaving `global_counter` mutable at runtime (unlike `constexpr`, which would make it `const`).

**Why It Works:** `constinit` is specifically about *initialization timing*, decoupled from mutability — the two properties `const`/`constexpr` bundle together, `constinit` separates.

---

### 01-P14 [DEBUG]

**Approach:** Compare the scope of the outer `total` against the inner, loop-body-scoped `total`.

**Reference Solution:** The `int total = total + i;` inside the loop body **declares a new, separate `total`** that shadows the outer one for the remainder of the block — and because its own initializer (`total + i`) refers to itself before it's initialized, it reads its own indeterminate value. This is undefined behavior, and even where it "happens to run," the outer `total` is never modified — the printed value is `0`.

**Fix:** drop the `int` — `total = total + i;` (or `total += i;`) to assign to the outer variable instead of shadowing it.

**Common Wrong Approaches:** assuming the inner `total` refers to the outer one because they share a name; scope resolution doesn't work that way — the new declaration takes over the name for the rest of its scope starting at the point of declaration, and its own initializer is inside that scope.

---

### 01-P15

**Approach:** Function-local `static` — apply the "block scope, static storage duration" split from the Crash Course.

**Reference Solution:** Scope: block (visible only inside `lookup`). Storage duration: static — it is constructed exactly once, the first time control passes through its declaration, and persists across all subsequent calls. It is **not** re-constructed on every call; the map's contents accumulate across calls unless explicitly cleared.

**C++ Considerations:** function-local statics with dynamic initialization are guaranteed thread-safe initialization since C++11 (no two threads can race to construct it), but concurrent *use* of the map itself after construction still needs external synchronization (Ch11) — construction safety is not the same as use safety.

---

### 01-P16

**Approach:** `r` and `p` are both aliases for `x`'s storage; track writes through either.

**Reference Solution:** After `r = 20;`: `x == 20`, `r == 20` (same object), `*p == 20` (same object). After `*p = 30;`: `x == 30`, `r == 30`, `*p == 30`. All three names/expressions refer to the same `int` object throughout.

---

### 01-P17 [DEBUG]

**Approach:** References must be bound at the point of declaration — there is no such thing as a "declare now, bind later" reference.

**Reference Solution:** Does **not** compile. `int& r;` with no initializer is ill-formed — a reference must be initialized (bound) in its own declaration; `r = x;` on the next line would be an *assignment through* an already-bound reference, not a binding, and since `r` was never validly declared, the program never reaches that point.

**Fix:** `int& r = x;` — bind at declaration.

---

### 01-P18 [DEBUG]

**Approach:** `thread_local` gives each thread its own independent instance of `c`.

**Reference Solution:** Each thread gets its **own separate `c`**, so `counter()` is safe in the sense that there's no data race between threads — but it is *not* a shared, cross-thread counter the way the name might suggest; each thread sees its own sequence starting from `0`, independent of every other thread's calls. If the intent was a single global shared counter, this is a logic bug (wrong tool), not a data race.

**C++ Considerations:** the reference returned is safe to use within the calling thread (the `thread_local` object outlives the function call, for the thread's lifetime), but storing that reference and handing it to a *different* thread would be a bug — it wouldn't refer to that other thread's instance.

---

### 01-P19

**Approach:** Apply `auto`/`auto&`/`auto&&` deduction to each initializer's actual value category and cv-qualification.

**Reference Solution:**
- `a` (`auto a = s;`) — `std::string` (plain value; `const` is stripped, it's a copy).
- `b` (`auto& b = s;`) — `const std::string&` (the explicit `&` is added to the deduced base type, and since `s` is `const`, that const is preserved because it's not a *top-level* qualifier being stripped here — `auto` deduction for `T&` keeps the referred-to type's own cv-qualification).
- `c` (`auto&& c = std::string("temp");`) — `std::string&&` bound directly to the temporary (a forwarding-reference-shaped declaration binding to an rvalue deduces as an rvalue reference here, since this is a plain variable declaration, not a template parameter — reference collapsing rules proper are a Ch03 topic, but `auto&&` on a prvalue behaves intuitively as "rvalue reference to it").

---

### 01-P20

**Approach:** Apply the "declared type" vs "type of the parenthesized expression" vs "type of an arithmetic expression" distinctions.

**Reference Solution:**
- `decltype(i)` — `int` (declared type of the named entity).
- `decltype((i))` — `int&` (parenthesized lvalue expression).
- `decltype(i + 0)` — `int` (the result of `operator+` on two `int`s is a prvalue `int`; arithmetic expressions are not lvalues).

---

### 01-P21

**Approach:** Rank the conversions `double`→`int` and `double`→`long` against each other.

**Reference Solution:** **Ambiguous** — both `double`→`int` and `double`→`long` are standard conversions of the same rank (neither is a promotion, since neither `int` nor `long` is a "promoted" type relative to `double`; both are equally-ranked narrowing conversions), so neither overload is strictly better than the other. Compile error: ambiguous call.

**Common Wrong Approaches:** assuming `long` "sounds bigger, so it should be preferred" — overload resolution ranks by *conversion category*, not by target-type size.

---

### 01-P22

**Approach:** `explicit` blocks implicit conversions but not direct-initialization or explicit casts.

**Reference Solution:**
1. `move(5.0);` — **does not compile**. `5.0` (a `double`) would need an implicit `double`→`Meters` conversion, which `explicit` blocks.
2. `move(Meters(5.0));` — **compiles**. `Meters(5.0)` is a direct-initialization of a temporary `Meters`, not an implicit conversion; the resulting `Meters` object is then passed directly (or moved, once Ch03's rules apply).
3. `move((Meters)5.0);` — **compiles**. A functional/C-style explicit cast is exactly the mechanism `explicit` is designed to still allow.

---

### 01-P23 [DEBUG]

**Approach:** Track capture-by-value's copy-at-creation-time semantics combined with the default `const`-ness of a non-`mutable` lambda's `operator()`.

**Reference Solution:** This **does not compile** as written, because `total += x;` attempts to modify the by-value-captured `total`, but the lambda's `operator()` is implicitly `const` (no `mutable`), so the captured copy is const-qualified inside the call operator. Even if it did compile (with `mutable` added), each call to `make_accumulator()` produces a *fresh* closure with its *own* copy of `total = 0` — that's actually the correct, intended behavior for independent accumulators; the bug being tested here is purely the missing `mutable`.

**Fix:**
```cpp
auto make_accumulator() {
    int total = 0;
    return [total](int x) mutable { total += x; return total; };
}
```

**Common Wrong Approaches:** assuming the bug is about lifetime/dangling — `total` is captured by *value*, so there's no dangling-reference issue here at all; the bug is purely the missing `mutable`.

---

### 01-P24

**Approach:** Use `explicit` on the converting constructor to block implicit `double`→`Meters` conversions while still allowing arithmetic between two `Meters`.

**Reference Solution:**
```cpp
class Meters {
public:
    explicit constexpr Meters(double v) : v_(v) {}
    constexpr double value() const { return v_; }
    friend constexpr Meters operator+(Meters a, Meters b) {
        return Meters(a.v_ + b.v_);
    }
    friend constexpr Meters operator*(Meters a, double scalar) {
        return Meters(a.v_ * scalar);
    }
private:
    double v_;
};
```

**Explanation:** the constructor taking `double` is marked `explicit`, so `Meters m = 5.0;` is rejected, but `Meters(5.0)` (direct) still works — exactly the pattern from 01-P22.

**Why It Works:** `explicit` removes the constructor from consideration during implicit conversion sequences and copy-initialization, but not from direct-initialization, which is what `operator+`'s internal `Meters(...)` calls use.

**C++ Considerations:** `operator*(Meters, double)` deliberately does *not* have a `double * Meters` counterpart in this minimal version — that asymmetry is intentional to keep the example focused; a complete unit library would add it.

---

### 01-P25

**Approach:** Use a `constexpr` constructor with a check that works both at compile time (via a `constexpr`-context-triggered contract violation) and at runtime (via a runtime assertion mechanism).

**Reference Solution:**
```cpp
class RangeChecked {
public:
    constexpr explicit RangeChecked(int v) : v_(check(v)) {}
    constexpr int value() const { return v_; }
private:
    static constexpr int check(int v) {
        return (v < 0 || v > 100)
            ? throw std::out_of_range("RangeChecked: value out of [0,100]")
            : v;
    }
    int v_;
};
```

**Explanation:** a `throw` expression inside a `constexpr` function is itself fine to *write*; what matters is whether it's actually *reached* during constant evaluation. If `check(v)` is called with an out-of-range constant `v` in a constant-expression context (e.g., `constexpr RangeChecked r(500);`), the compiler must reject the program at compile time, because throwing is not a valid constant-expression operation — this surfaces as a compile error, which is exactly the desired "check fires at compile time" behavior. At runtime, with a genuinely runtime-determined `v`, the same `throw` becomes an ordinary runtime exception.

**Why It Works:** the standard requires that if a `constexpr` function is actually evaluated at compile time and that evaluation would throw, the program is ill-formed — this is the mechanism, not a special "compile-time assert" feature.

**C++ Considerations:** this class is not `noexcept`; constructing it with an out-of-range runtime value throws — callers must be prepared for that, a preview of Ch06's exception-guarantee formalization.

---

### 01-P26

**Approach:** Determine why a free function is required for symmetric operand handling with stream types.

**Reference Solution:**
```cpp
struct Point { int x, y; };

std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << '(' << p.x << ", " << p.y << ')';
}
// This must be a free function, not a Point member, because the left-hand
// operand of << here is std::ostream, not Point — a member operator<< would
// need to be a member of std::ostream, which we can't (and shouldn't) modify.
```

**Why It Works:** for a binary operator, a member-function overload's *first* (left) operand is always the implicit `this`, so `os << p` could only be written as a `Point` member if `Point` were the left operand — it isn't. This is the general principle 01-P41 also tests.

---

### 01-P27

**Approach:** A generic lambda's parameters use `auto`, making the closure type's `operator()` an implicit template.

**Reference Solution:**
```cpp
auto max_of = [](const auto& a, const auto& b) {
    return (a > b) ? a : b;
};
```

**Explanation:** the compiler generates, effectively, a closure type whose `operator()` is:
```cpp
template <typename T, typename U>
auto operator()(const T& a, const U& b) const { return (a > b) ? a : b; }
```
one template instantiation per distinct pair of argument types used at call sites.

**C++ Considerations:** if `a` and `b` are different types (e.g., `int` and `double`), the ternary's common type applies the usual arithmetic conversions to the return — worth being deliberate about if precision matters.

---

### 01-P28

**Approach:** Build the array with a `constexpr` function whose loop is itself constant-evaluable, then use it in an array-bound context to confirm compile-time evaluation.

**Reference Solution:**
```cpp
constexpr std::array<int, 10> make_lookup() {
    std::array<int, 10> result{};
    for (int i = 0; i < 10; ++i) result[i] = i * i;
    return result;
}
constexpr auto squares = make_lookup();
int arr[squares[3]];  // uses squares[3] == 9 as a compile-time constant
```

**Why It Works:** since C++14, `constexpr` functions may contain loops and local mutable state, as long as every step is itself constant-evaluable — `std::array` (unlike `std::vector`) has no dynamic allocation, so it's usable in a `constexpr` context.

---

### 01-P29 [DEBUG]

**Approach:** Identify the concurrency hazard in a function-local static cache accessed without synchronization.

**Reference Solution:** The **construction** of `cache` itself is thread-safe (guaranteed since C++11 for function-local statics). The bug is that every subsequent **use** of `cache` — `cache.find(key)` and `cache[key] = result` — is an unsynchronized read/write to a shared `std::unordered_map` from potentially multiple threads, which is a **data race** if `expensive_compute` is ever called concurrently from more than one thread. `std::unordered_map` provides no thread-safety guarantee for concurrent modification.

**C++ Considerations:** this is flagged, not fixed, here deliberately — the fix (a mutex, or a concurrent map design) requires Ch11's synchronization primitives; this problem's job is purely to train you to *notice* the hazard from the shape of the code (shared mutable static + no visible lock).

---

### 01-P30

**Approach:** Confirm `std::function`'s type erasure accepts any of the three callable shapes with a matching signature.

**Reference Solution:**
```cpp
int free_fn(int x) { return x + 1; }
struct Functor { int operator()(int x) const { return x * 2; } };

std::function<int(int)> f = free_fn;
f = [](int x) { return x - 1; };
f = Functor{};
```

**Why It Works:** `std::function<R(Args...)>` type-erases the *call signature*, not the concrete callable type — anything invocable as `R(Args...)` can be stored, regardless of whether it's a function pointer, closure, or functor.

---

### 01-P31

**Approach:** `std::invoke` unifies the call syntax across ordinary callables and pointers-to-member.

**Reference Solution:**
```cpp
template <typename F, typename... Args>
auto call_any(F&& f, Args&&... args) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

int free_fn(int x) { return x; }
struct S { int method(int x) const { return x * 2; } };

call_any(free_fn, 5);
call_any(&S::method, S{}, 5);   // pointer-to-member — plain f(args...) can't do this
call_any([](int x) { return x + 1; }, 5);
```

**Why It Works:** `f(args...)` syntax works for function pointers, lambdas, and functors directly, but a pointer-to-member-function like `&S::method` requires the `(obj.*ptr)(args...)` or `(obj->*ptr)(args...)` syntax instead — `std::invoke` detects which shape it received and dispatches to the correct call form uniformly.

---

### 01-P32

**Approach:** Construct a call whose argument type is equally convertible to both overload parameter types, then disambiguate via an explicit cast or a differently-named function instead of adding a third overload.

**Reference Solution:**
```cpp
int clamp(int v) { /* ... */ return v; }
double clamp(double v) { /* ... */ return v; }

// clamp(5L);  // ambiguous: long -> int and long -> double are both standard conversions of equal rank
clamp(static_cast<int>(5L));  // disambiguated via explicit cast — no third overload added
```

**Common Wrong Approaches:** adding `long clamp(long)` — this "solves" this one call site but multiplies the combinatorial overload set for every future numeric type; an explicit cast at the ambiguous call site is usually the more maintainable fix.

---

### 01-P33 [DEBUG]

**Approach:** Recognize this is not actually compiler divergence in standard behavior — check whether the premise holds.

**Reference Solution:** This is a **trick premise** — per the standard, `f(3.0)` here is genuinely **ambiguous** on every conforming compiler: `double`→`int` and `double`→`float` are both standard floating-to-integral/floating-to-floating conversions of equal rank, neither a promotion. If you observed different behavior on two compilers, the likely real cause is a *third* overload present in one translation unit but not the other (e.g., via a differently-configured header), or a nonstandard extension/warning-as-behavior-change flag — not a genuine standard-mandated divergence. The lesson: verify the actual overload set in scope before concluding "the standard is ambiguous here" when observed behavior differs.

**Why It Works:** this problem exists specifically to train the "standard guarantee vs. observed behavior" distinction called out in the Notes throughout `CONCEPT_INDEX.md` — don't trust an observed difference until you've ruled out a mundane cause.

---

### 01-P34

**Approach:** Use a `constexpr` bit-trick or loop-based check, callable from both a compile-time and runtime context.

**Reference Solution:**
```cpp
constexpr bool is_power_of_two(unsigned n) {
    return n != 0 && (n & (n - 1)) == 0;
}

static_assert(is_power_of_two(16));
int arr[is_power_of_two(8) ? 8 : 1];  // compile-time use

int check_input(unsigned n) {         // runtime use, same function
    return is_power_of_two(n) ? 1 : 0;
}
```

---

### 01-P35

**Approach:** Show the reference-capture-then-outlive pattern, then contrast with value-capture.

**Reference Solution:**
```cpp
// DANGLING PATTERN (do not run for real — illustrative only):
// std::function<int()> make_dangling() {
//     int local = 42;
//     return [&local]() { return local; };  // captures local BY REFERENCE
// }   // <- local's storage ends here; the returned lambda now holds a dangling reference
// auto f = make_dangling();
// f();  // UB: reads a destroyed automatic-duration int

// FIX — capture by value instead:
auto make_safe() {
    int local = 42;
    return [local]() { return local; };  // copies local at creation time; safe to outlive the original
}
```

**C++ Considerations:** this is the lambda-specific instance of the exact same dangling-reference hazard as 01-P06 — a lambda capturing by reference is, structurally, just another reference whose lifetime must not outlive its referent.

---

### 01-P36 [DEBUG]

**Approach:** Trace both branches of the conditional to determine whether `x` is definitely initialized before `return x;`.

**Reference Solution:** If `some_runtime_condition()` returns `false`, `x` is never assigned, and `return x;` reads an **indeterminate value** — undefined behavior. `-Wall` off doesn't change this; it only changes whether the compiler *warns* about it (most compilers do warn on this specific pattern, "variable may be used uninitialized," but it's not required to).

**Fix:** initialize `x` with a default (`int x = 0;`) or restructure so every path assigns before use.

**Common Wrong Approaches:** assuming "it compiled without a warning flag, so it's fine" — a missing diagnostic is not a correctness guarantee; the standard does not require this UB to be diagnosed at all.

---

### 01-P37 [DEBUG]

**Approach:** Check whether `t + u` (a `std::string` plus an `int`) is even a valid expression in the first place.

**Reference Solution:** `decltype(t + u)` itself fails to form, because `std::string + int` has no matching `operator+` overload (`std::string` supports `+` with another `std::string`, a `const char*`, or a `char`, but not a bare `int`). The error isn't really about `decltype`'s deduction *rules* — it's that the expression inside `decltype(...)` is ill-formed on its own terms, and `decltype` cannot deduce a type from an expression that doesn't type-check. This is not SFINAE-suppressible in this context (it's not in an unevaluated context that participates in overload deduction the way a function template default would) — it's a hard error at the point of instantiation of `add`.

**Why It Works:** `decltype(expr)` requires `expr` to be well-formed; it inspects the type of a valid expression, it doesn't independently validate or coerce mismatched operand types.

---

### 01-P38 [DEBUG]

**Approach:** Both `a` and `b` are function parameters — automatic-duration locals — regardless of which the ternary selects.

**Reference Solution:** The bug: `a` and `b` are both **parameters with automatic storage duration**, local to the call. Returning `a > b ? a : b` returns a reference to *whichever parameter* the ternary selected — but both parameters' storage ends when `larger` returns, exactly like 01-P06. There is no version of this signature (`int& larger(int a, int b)`) that can safely return a reference to a by-value parameter; the caller ends up with a dangling reference regardless of which branch was taken.

**Fix:** either return by value (`int larger(int a, int b)`), or take the parameters by reference and return one of *those* references (`int& larger(int& a, int& b)`), which is safe because now the referents are the *caller's* objects, not local copies.

---

### 01-P39

**Approach:** Enumerate every category of call site affected by the aggregate → constructor transition, not just the ones that fail to compile.

**Reference Solution:** `Config c{3, 1000};` still compiles in both versions, and produces the same observable member values — but the *meaning* of the line changes completely: in the "before" version it's aggregate-initialization (direct positional member init, no code runs beyond default member semantics); in the "after" version it's a call to `Config(int, int)`, meaning any validation logic in that constructor now genuinely executes on every such call site, including ones the code's author may not have re-reviewed. Additional silently-affected categories, beyond the one shown: (1) any code that previously used designated-initializer-style or partial aggregate init (`Config c{.retries = 3};`, C++20) stops compiling entirely, since that syntax is aggregate-only; (2) any code relying on `Config` being *trivially copyable*/an aggregate for `memcpy`-based serialization or `std::is_aggregate_v<Config>` compile-time checks silently changes answer; (3) `std::vector<Config>` construction via aggregate-style brace-init in-place (e.g. `vec.push_back({3, 1000})`) still compiles but now goes through the constructor instead of aggregate-init, meaning validation now runs where it silently didn't before.

**Why It Works:** this is the general lesson behind the Ch01 note on aggregates — adding a constructor is not merely "adding a feature," it is a **breaking change to the type's initialization semantics** across every call site, whether or not any of them fail to compile.

---

### 01-P40

**Approach:** `push_back` takes its argument by (effectively) reference to a `const Wrapper&` or `Wrapper&&`, requiring an implicit conversion from `int`; `emplace_back` forwards its arguments directly to `Wrapper`'s constructor.

**Reference Solution:** `vec.push_back(5);` — **does not compile**, because `Wrapper`'s constructor is `explicit`, and `push_back` would need an *implicit* `int`→`Wrapper` conversion to construct the temporary it inserts — exactly what `explicit` forbids. `vec.emplace_back(5);` — **compiles**, because `emplace_back` constructs the `Wrapper` in-place by forwarding `5` directly to `Wrapper`'s constructor as a direct-initialization, which `explicit` does not block.

**Why It Works:** this is the container-library-level version of the direct-vs-implicit distinction from 01-P22 — `emplace_back`'s entire design point is to avoid requiring an implicit conversion or an extra temporary+move, and this is a direct, visible consequence of that design for `explicit` constructors specifically.

---

### 01-P41

**Approach:** Construct (in the general form the problem asks for) a call requiring *two* chained user-defined conversions, and show it fails, to isolate the "at most one" rule.

**Reference Solution:** The single-conversion case (`f(a)` calling `void f(int)`, using `A::operator int()`) **does** compile — one user-defined conversion (`A`→`int`) is allowed. The general principle this demonstrates: if instead there were a function `void g(SomeOtherClass)` and neither `A` nor `int` converts *directly* to `SomeOtherClass` — requiring, say, `A`→`int` (user-defined) followed by `int`→`SomeOtherClass` (a second user-defined conversion, if `SomeOtherClass` had a converting constructor from `int`) — the call would be rejected, because an implicit conversion sequence may contain **at most one** user-defined conversion. The minimal fix in that shape of problem is always the same: make one of the two conversions explicit at the call site (an explicit intermediate cast), rather than relying on the compiler to chain them.

**Why It Works:** this is QC8's rule in a concrete design scenario — it's the same principle 01-P37's std::string+int failure gestures at from a different angle (there, no conversion path existed at all; here, a path exists but is one hop too long to be implicit).

---

### 01-P42

**Approach:** Apply named-factory-function + explicit-conversion-operator patterns to block accidental bare-numeric construction and accidental implicit degrees-to-double conversion.

**Reference Solution:**
```cpp
class Angle {
public:
    static constexpr Angle from_degrees(double d) { return Angle(d); }
    static constexpr Angle from_radians(double r) { return Angle(r * 180.0 / 3.14159265358979323846); }

    constexpr Angle operator+(Angle other) const { return Angle(degrees_ + other.degrees_); }
    explicit constexpr operator double() const { return degrees_; }

private:
    explicit constexpr Angle(double degrees) : degrees_(degrees) {}
    double degrees_;
};
```

**Design Rationale:** the constructor is `private` and only reachable via the named factories, so `Angle a(90.0);` from outside the class is impossible — a caller must write `Angle::from_degrees(90.0)`, which makes the *unit* explicit in the call site itself rather than relying on convention/comments (a bare-number constructor can't distinguish degrees from radians, a classic real-world unit bug). The conversion-to-`double` operator is `explicit` for the same reason as 01-P22/24 — an implicit `Angle`→`double` would let an `Angle` silently participate in arbitrary arithmetic contexts expecting a raw number, defeating the entire point of the strong-typedef pattern; requiring `static_cast<double>(a)` keeps every such use visible and deliberate.

---

### 01-P43

**Approach:** Use a non-type template parameter `N`, with a `constexpr` function usable both in a `static_assert` and at an ordinary runtime call site.

**Reference Solution:**
```cpp
template <std::size_t N>
constexpr std::array<unsigned long long, N> fibonacci() {
    std::array<unsigned long long, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = (i < 2) ? i : result[i-1] + result[i-2];
    }
    return result;
}

static_assert(fibonacci<10>()[9] == 34);  // compile-time use

unsigned long long nth_fib_runtime(int n) {  // runtime use, same function template
    auto table = fibonacci<20>();
    return table[n];
}
```

**Why It Works (constexpr vs consteval):** `constexpr` is correct here specifically *because* the function needs to serve both a `static_assert` (compile-time-required context) and an ordinary runtime call (`nth_fib_runtime`, called with a runtime `n` used only as an index, not as `N`). `consteval` would make the *entire function* an immediate function, rejecting the runtime call path outright — it can't "sometimes" be a runtime function. `constexpr` is the tool specifically for "usable in either context," which is the stated requirement.

---

### 01-P44 [DEBUG]

**Approach:** Trace the chain: string literal → `std::string` (implicit, via `std::string`'s non-explicit converting constructor from `const char*`) → `Logger` (implicit, via `Logger`'s non-explicit constructor taking `std::string`) → pass-by-value copy/move into `configure`.

**Reference Solution:** `"app"` is a `const char*` string literal. `Logger`'s constructor takes a `std::string`, and `std::string` has a non-`explicit` converting constructor from `const char*`, so `"app"` implicitly converts to a temporary `std::string`. `Logger`'s own constructor is also not `explicit`, so that temporary `std::string` implicitly converts again to construct a temporary `Logger` — **wait**, this is exactly two chained user-defined conversions (`const char*`→`std::string` is technically a standard library user-defined conversion, and `std::string`→`Logger` is a second one), which per the "at most one user-defined conversion" rule (QC8, 01-P41) should be **rejected**... except it isn't, because `configure(Logger logger)`'s parameter type is `Logger`, and the *argument* expression is the string literal directly — the compiler performs exactly **one** user-defined conversion to reach `Logger` from the argument's type. The confusion is thinking of `const char*`→`std::string` as a separate hop in the *same* conversion sequence; it isn't — the relevant sequence is "argument type → parameter type" as a whole, and `Logger`'s constructor accepting `std::string` means the compiler looks for something convertible to `std::string` as *part of* constructing the `Logger`, which is a constructor-argument conversion, not a second link in the *implicit conversion sequence to the parameter*. Only one user-defined conversion (the `Logger` constructor call itself) is being used to satisfy the `configure(Logger)` parameter.

**The one change that breaks this call site while breaking almost nothing else:** mark `Logger`'s constructor `explicit`. `configure("app");` would then fail to compile (no implicit `const char*`/`std::string`→`Logger` conversion), while every call site that already explicitly constructs a `Logger` (`configure(Logger("app"))`, `Logger l("app"); configure(l);`) continues to work unchanged — this is the same "the `explicit` fix has a large, containable blast radius" pattern from 01-P24 and 01-P42, applied to a diagnostic scenario instead of a from-scratch design.

**C++ Considerations:** this problem is intentionally the hardest conversion-tracing problem in the chapter — if you found the "wait" moment confusing, revisit 01-P41: the "one user-defined conversion" rule bounds a *single* implicit conversion sequence from an argument to a *parameter type*, not the total count of constructor calls that might occur while building up nested objects.

---

### 01-P45

**Approach:** Enumerate categories of affected call sites by initialization/overload-resolution mechanism, mirroring 01-P39 but at production/review scale, then propose a migration path.

**Reference Solution (review comment):**

> This change breaks or silently alters more than the direct compile failures suggest. (1) Any aggregate brace-init call site (`Rect r{3, 4};`) either fails to compile (if argument types don't match a constructor) or, worse, silently switches from positional aggregate-init to a constructor call with the same apparent syntax and values — meaning any validation logic added later to that constructor will start running at every one of those call sites without their authors reviewing it, exactly as in 01-P39. (2) Any code doing `r.width` / `r.height` as direct member access breaks outright, needing conversion to `r.width()` / `r.height()` — at least this category is a clean compile error, not silent. (3) Any code relying on `Rect` being a `std::is_aggregate_v` type, being trivially copyable for `memcpy`/binary-serialization purposes, or being usable with designated initializers (`Rect{.width = 3}`) breaks or changes behavior, and some of these failure modes (e.g., a `memcpy`-based serializer that now copies past private-implementation-detail assumptions) may not fail to compile at all — they'll just be silently wrong at whatever point `Rect`'s implementation details next change. (4) Any generic/template code instantiated over `Rect` that assumed aggregate status (e.g., structured-bindings-based decomposition, `auto [w, h] = rect;`) breaks, since structured bindings on a non-aggregate class type require public accessible members or a tuple-like protocol, neither of which this new `Rect` provides. A safer migration path: keep `Rect` an aggregate, and add validation via a separate factory function (`Rect make_validated_rect(int, int)`) or a `constexpr` invariant-check helper called explicitly at construction sites that need it — following the same "factory function over converting constructor" pattern used in 01-P42 — so existing call sites keep working unless they specifically opt into validation, rather than every call site silently and non-optionally acquiring new runtime behavior.

**Why It Works:** this problem deliberately has no single "correct" code fix — it's a design-judgment problem (L6), and the rubric is whether the review identifies the *categories* of breakage (compile-fail, silent-behavior-change, and silently-wrong-later) rather than just the first one anyone notices.

---

### 01-P46

**Reference Solution:** Output: `1 0`. `a < b` compares `major` first (both `1`, equal), then `minor` (`2 < 3`, true) — so `a < b` is `true`, printed as `1`. `a == b` is `false` (printed `0`) since `minor` differs. `Version` here has *only* a defaulted `operator<=>` and no `operator==` written or defaulted — but `a == b` still compiles, because when a class defines `<=>` (defaulted or not) and no usable `operator==`, C++20 lets the compiler rewrite `a == b` as `(a <=> b) == 0`. This is a real, standard-specified rewrite rule, distinct from the point Misconception 8 warns about: the rewrite means `==` *works* off the back of `<=>` alone, but it is still a separate mechanism from actually *declaring* `operator==` — a type that suppresses or can't use the rewrite (e.g., one wanting a cheaper, non-`<=>`-derived equality check) must still write or default `==` explicitly, exactly as Misconception 8 states. Comparison-category deduced for `Version`: `std::strong_ordering`, since every member (`int`) supports strong ordering.

**Explanation:** The nuance worth holding onto here: "defaulting `<=>` doesn't give you `==`" (Misconception 8) is about *authoring* — you haven't declared an `==` of your own — not about whether `a == b` happens to compile at a call site, which the `<=>`-based rewrite can still make work.

---

### 01-P47

**Reference Solution:** `Reading`'s defaulted `<=>` deduces `std::partial_ordering`, not `std::strong_ordering`, because `value` is a `double` — floating-point comparison is not a total order (NaN is unordered relative to every value, including itself), so the compiler cannot claim the stronger guarantee for any type containing a floating-point member, regardless of what that specific instance's value happens to be. `r1 < r2` evaluates to `false` — comparing against NaN, every relational operator (`<`, `<=`, `>`, `>=`) yields `false` (NaN is unordered, not "less than everything" or "greater than everything"), which is precisely what `std::partial_ordering::unordered` represents.

**Explanation:** A caller who assumed `r1 < r2` being `false` means `r1 >= r2` would be wrong — with `partial_ordering`, `!(a < b)` does **not** imply `a >= b` the way it does under a total order; both `r1 < r2` and `r1 >= r2` can be simultaneously false when the comparison is genuinely unordered.

---

### 01-P48 [DEBUG]

**Reference Solution:** This is **undefined behavior**, not "garbage integer, but harmless" — reading an uninitialized local (`y`, when `x <= 0`) has no defined value and no defined behavior at the read itself. The colleague's framing understates the danger in a specific, examinable way: a compiler observing that `y` is read without a guaranteed prior write on the `x <= 0` path is permitted to assume that path either never executes, or that the entire function's observable behavior on that path is unconstrained — in practice, this can mean the compiler eliminates the `if` check as dead code (if it can prove UB is the only way to reach a later branch), reorders or removes seemingly-unrelated code that shares a basic block with the read, or produces results that vary by optimization level, compiler version, or surrounding code changes with no connection to `y` itself. "It's fine, we're just printing a weird number" assumes the *only* consequence is a bad value in one variable — the UB section's core point is that no such bound on scope exists.

**Explanation:** This is the "Undefined, Unspecified, and Implementation-Defined Behavior" crash-course section's central claim made concrete: fix is to initialize `y` (e.g., `int y = 0;` or restructure to avoid the possibility of an unassigned path entirely, such as returning early or requiring `x > 0` as a precondition).

---

### 01-P49

**Reference Solution:** The review comment's claim is **wrong about the category** — argument evaluation order for `f(a(), b())` (outside the specific sequenced cases C++17 carved out, like initializer lists) is **unspecified behavior**, not implementation-defined: the standard permits multiple orders and does not require the implementation to *document* which one it consistently uses, and critically, a conforming compiler is not obligated to pick the same order every time (across different call sites, optimization levels, or even different calls to the same expression pattern) the way implementation-defined behavior requires. "Document which compiler we target" is exactly the correct mitigation *for implementation-defined* behavior, but it does not fix reliance on unspecified behavior, because there may be no single documented answer to point to at all, and no guarantee the same build produces the same order everywhere.

**What should actually happen:** verify whether the code's correctness genuinely depends on a specific evaluation order; if so, that's a latent bug regardless of what any specific compiler currently does, and the fix is to remove the order-dependency (e.g., sequence the two calls into separate statements with an explicit, guaranteed order) rather than to document a compiler and treat the risk as contained.

---

## Integration Challenge Solution — 01-IC1

**Reference Solution:**
```cpp
class Meters {
public:
    static constexpr Meters make(double v) { return Meters(v); }
    constexpr Meters operator+(Meters other) const { return Meters(v_ + other.v_); }
    constexpr double raw() const { return v_; }
private:
    explicit constexpr Meters(double v) : v_(v) {}
    double v_;
};

class Feet {
public:
    static constexpr Feet make(double v) { return Feet(v); }
    constexpr Feet operator+(Feet other) const { return Feet(v_ + other.v_); }
    constexpr double raw() const { return v_; }
private:
    explicit constexpr Feet(double v) : v_(v) {}
    double v_;
};

// Explicit, named conversion — NOT an implicit conversion operator on either
// type. An implicit Meters<->double or Meters<->Feet conversion would let a
// bare double or a Feet value silently participate anywhere a Meters is
// expected, defeating the whole point of keeping the units distinct (the
// same reasoning as 01-P42's explicit conversion-to-double operator).
constexpr Feet to_feet(Meters m) {
    return Feet::make(m.raw() * 3.28084);
}

static_assert((Meters::make(2.0) + Meters::make(3.0)).raw() == 5.0);
```

**Design Rationale:** both unit types use the "private constructor + named `static` factory" pattern from 01-P42 rather than a public converting constructor, so `Meters(5)` isn't directly callable from outside — you must go through `Meters::make(5)`, which is no more typing but makes accidental bare-numeric construction impossible to do by accident. `operator+` is restricted to same-type operands only (there's no `Meters + Feet` overload), so unit confusion is caught at compile time rather than needing a runtime check. `to_feet` is a free function with an ordinary (non-`explicit`, non-operator) name specifically so that converting between units is always a visible, greppable call in source, never something that happens implicitly as a side effect of an assignment or function-call conversion.

**C++ Considerations:** every operation here is `constexpr`, so the entire integration challenge — construction, addition, and the `static_assert` — is fully compile-time-evaluable, which is what makes the `static_assert` line valid as a compile-time proof rather than a runtime check.
