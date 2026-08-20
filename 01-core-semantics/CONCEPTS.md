# Chapter 01 — Core Semantics: Initialization, Const-ness, and Name Binding

> Scope: how a name acquires a type, a value, and a meaning — before any resource management enters the picture. Solutions live in the sibling file [`SOLUTIONS.md`](SOLUTIONS.md) — don't open it until you've attempted a problem.

**Prerequisites:** none — this is the entry chapter. Assumes basic C++ syntax fluency (you can already write a function, a class, a loop).

**Concepts owned here:** initialization (default/copy/direct/list/aggregate/narrowing), `const`, `constexpr`, `consteval`, `constinit`, scope, storage duration, references, pointers, `auto`, `decltype`, `decltype(auto)`, overload resolution, implicit conversions, explicit conversions, operator overloading, lambdas, function objects, `std::function`, `std::invoke`.

**Referenced but not owned here:** *lifetime* (Ch02 — this chapter owns *storage duration*, which is when memory exists; lifetime, when an *object* exists, is a deliberately separate cut), move semantics (Ch03), templates (Ch05).

---

## Crash Course

Read this once, in order. It's orientation, not the content — the problems are the content.

### Initialization

C++ has more than one way to give a variable its starting value, and they are not interchangeable:

```cpp
int a;          // default-init: indeterminate value for a local of scalar type
int b{};         // list-init with empty braces: zero-init
int c(5);        // direct-init
int d = 5;       // copy-init
int e{5};        // list-init (direct-list-init)
int f = {5};     // copy-list-init
Point p{1, 2};   // aggregate-init, if Point has no user-declared constructors
```

`int a;` at block scope leaves `a` with an indeterminate value — reading it before writing is undefined behavior. `int a{};` zero-initializes. This single distinction is responsible for a large fraction of "works on my machine" bugs, because an indeterminate value often happens to be zero in a debug build and garbage in a release build.

List-initialization (`{}`) additionally forbids **narrowing conversions** at compile time in most contexts: `int x{3.14};` is an error, but `int x = 3.14;` silently truncates to `3`.

Aggregate initialization applies only to aggregates (roughly: no user-declared constructors, no private/protected non-static data members, no virtual functions, no base classes prior to C++17 — C++17 relaxed the base-class restriction). The moment a class gets even one user-declared constructor, `{1, 2}` stops being aggregate-init and becomes a call to that constructor (or a compile error if none matches).

### `const`, `constexpr`, `consteval`, `constinit`

These four answer different questions and are frequently confused:

- `const` — "this name cannot be used to modify the object." Says nothing about *when* the value is known.
- `constexpr` — "this *can* be evaluated at compile time, and must be if used where a compile-time constant is required." A `constexpr` function can still run at runtime if called with non-constant arguments.
- `consteval` — "this *must* be evaluated at compile time, on every call, or it's a compile error." C++20. An "immediate function."
- `constinit` — "this variable's *initialization* must happen at compile time, but the variable itself is not const." C++20. Solves static-initialization-order problems for non-const globals without granting `constexpr`'s power (or restrictions).

`const` and `constexpr` compose (`constexpr` implies `const` for objects), but `constexpr` and `consteval` are mutually exclusive on the same declaration, and `constinit` explicitly does *not* imply `const`.

### Scope vs. Storage Duration

Two axes that are easy to conflate:

- **Scope** is about *name visibility* — where in the source text can you write this identifier and have it resolve to this entity. Block scope, namespace scope, class scope, function-parameter scope.
- **Storage duration** is about *how long the object's storage exists* — automatic (block-scoped locals, freed at block exit), static (globals, namespace-scope, `static` locals — exists for the whole program), thread (`thread_local` — one instance per thread), dynamic (`new`/`malloc` — you control it).

A `static` local variable has **block scope** (you can't name it outside the function) but **static storage duration** (it survives across calls). This combination is exactly why `static` locals are useful for lazy-initialized singletons.

### References and Pointers

A reference is not a pointer with different syntax. A reference must be bound at initialization and cannot be rebound; there is no "null reference" in well-formed code (you can construct one through UB, but that's a bug, not a feature). A pointer can be reseated, can be null, and its own address is observable (`&p`) separately from what it points at.

Reference binding to a temporary extends that temporary's lifetime to the reference's — but only in specific cases (a direct binding at the point of declaration), not transitively through a returned reference or a stored member.

### `auto`, `decltype`, `decltype(auto)`

- `auto x = expr;` deduces `x`'s type using the same rules as template argument deduction — it **strips top-level references and cv-qualifiers** unless you write `auto&`, `const auto&`, or `auto&&`.
- `decltype(expr)` inspects the *declared type* of an expression without evaluating it, and — critically — `decltype((x))` (double parens) differs from `decltype(x)`: the parenthesized form treats the parenthesized expression as an lvalue expression, yielding a reference type if `x` is an lvalue, even though `x` alone might just be a plain variable name.
- `decltype(auto)` (C++14) uses `auto` syntax but `decltype` deduction rules — it's the only way to write "deduce this, but preserve reference-ness exactly," which plain `auto` cannot do.

### Overload Resolution and Conversions

When you call `f(x)` and multiple overloads of `f` exist, the compiler ranks candidate functions by how "good" the conversion from `x`'s type to each parameter type is: exact match > promotion > standard conversion > user-defined conversion. At most **one** user-defined conversion may participate in a conversion sequence — the compiler will not chain two implicit user-defined conversions to make a call work.

`explicit` on a constructor or conversion operator removes it from consideration for *implicit* conversions and copy-initialization, while still allowing direct-initialization and explicit casts. `explicit(bool)` (C++20) lets that decision depend on a compile-time condition.

### Operator Overloading

Operators are just functions with funny names and special call syntax. They participate in the *same* overload resolution as any other function. The choice between a member function (`T::operator+`) and a free function (`operator+(T, T)`) matters when you need implicit conversions to apply to the *left*-hand operand too — a member function can't have its own `this` implicitly converted, but a free function's first parameter can.

### The Three-Way Comparison Operator (`<=>`, C++20)

`operator<=>` ("spaceship") lets a type define ordering once and get `<`, `<=`, `>`, `>=` synthesized automatically by the compiler from that single definition, instead of writing four (or six, counting `==`/`!=`) separate operator overloads by hand.

```cpp
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;   // memberwise, lexicographic
    bool operator==(const Point&) const = default;     // == is not implied by <=>; write it too (or default it)
};
```

`= default` on `<=>` generates a memberwise, lexicographic comparison (compare `x` first; only if equal, compare `y`) — the same order the members are declared in. The compiler picks the **narrowest correct return type** automatically for a defaulted `<=>` (`std::strong_ordering` if every member's comparison is a strong ordering, `std::partial_ordering` if any member is a floating-point type, since NaN comparisons are not totally ordered). A hand-written (non-defaulted) `<=>` must return one of `std::strong_ordering`, `std::weak_ordering`, or `std::partial_ordering` explicitly, and must actually accurately reflect which category applies — claiming `strong_ordering` for a type that in fact has NaN-like incomparable values would be a genuine correctness bug, not just a style choice, since callers may rely on the strong-ordering guarantee (that inequivalent-implies-distinguishable) to justify using it as, say, a map key.

Defining `<=>` alone does **not** give you `==`/`!=` — those still need their own definition (or their own `= default`), because equality can sometimes be implemented more efficiently than "compare via `<=>` and check for the equal case" (e.g., comparing containers' sizes first before any element-wise work), and the language doesn't assume the two are always the same operation.

### Lambdas and Function Objects

A lambda expression is syntactic sugar for an unnamed class with `operator()` (a *closure type*) — everything a lambda does, a hand-written functor with member captures does too, just with more typing. Capture-by-value copies at the point the lambda is created, not when it's called; capture-by-reference binds to the referent, so a lambda that outlives its captured reference's lifetime is a dangling-reference bug waiting to happen. `mutable` on a lambda allows its `operator()` to modify value-captured members — without it, `operator()` is `const` by default.

`std::function<R(Args...)>` type-erases *any* callable with a matching signature — a function pointer, a lambda, a functor — behind one concrete type, at the cost of (usually) a heap allocation for anything too large for its small-buffer optimization, and the loss of inlining opportunities the compiler had when the callable's concrete type was known. `std::invoke` provides one uniform syntax for calling anything callable, including pointers-to-member-function, which ordinary `f(args...)` syntax cannot call directly.

### Undefined, Unspecified, and Implementation-Defined Behavior

The standard distinguishes three categories of "the standard doesn't pin this down exactly," and the difference between them matters for how much you can rely on any given behavior — this vocabulary has already been used as an annotation convention throughout this workbook (and will keep being used in later chapters); this section is where it's formally defined.

- **Undefined behavior (UB):** the standard imposes *no requirements at all* on what happens — a program that exercises UB has no defined meaning, and a compiler is permitted to assume UB never occurs, which in practice means it may optimize around the assumption in ways that make the actual observed behavior arbitrarily surprising (not just "garbage value," but potentially removing entire branches of code, or miscompiling code that looks unrelated to the UB itself). Signed integer overflow, dereferencing a null pointer, reading an uninitialized local variable's value, and out-of-bounds indexing are all UB. UB is not "the program crashes" — a crash would be a defined, comparatively tame outcome; UB's actual danger is that nothing is guaranteed, including "it happens to work today."
- **Unspecified behavior:** the standard allows *multiple* valid behaviors, and a given implementation picks one, but need not document which — and a *conforming* implementation may even choose differently between runs or optimization levels for some cases. Function-argument evaluation order (before C++17's specific sequencing rules for a few cases) is a classic example: `f(g(), h())` doesn't guarantee whether `g()` or `h()` runs first, and a real compiler may not commit to one deterministic answer at all call sites.
- **Implementation-defined behavior:** like unspecified behavior — multiple valid choices exist — but the implementation *must* document which one it picked, and that choice is then observable/reliable for that specific implementation. The size of `int` (commonly, but not universally, 4 bytes), and the signedness of a plain `char`, are implementation-defined: a given compiler's documentation states the answer, and code that depends on that answer is portable *only* across implementations making the same documented choice.

The practical distinction: implementation-defined behavior is something you can safely depend on if you've checked the target implementation's documentation (and accept the portability cost); unspecified behavior is something you should not depend on producing any particular one of its valid outcomes, even on one specific implementation, without rechecking every time something about the build changes; undefined behavior is something you must never rely on producing *any* particular outcome at all, ever, on any implementation, under any circumstances — code containing genuine UB is not "usually fine" code with a small risk; it is a program with no defined meaning that happens to currently produce output you like.

---

## Common Misconceptions

Read these *after* the crash course, before the problems — they preempt the mistakes the problems are designed to surface.

1. **"`int x;` initializes `x` to zero."** Only true for static/thread storage duration, or as a member of a class with no user constructor being default-constructed as `T{}`. A plain local `int x;` at block scope has an indeterminate value.
2. **"`const` means compile-time constant."** No — `const int n = get_runtime_value();` is perfectly legal; `n` just can't be reassigned. Compile-time-constant is `constexpr`'s job.
3. **"`auto` preserves the exact type of the initializer."** It deduces using template-argument-deduction rules, which strip references and top-level cv-qualifiers by default. `const int& r = x; auto y = r;` makes `y` a plain, non-const `int`.
4. **"A reference is just a pointer that can't be null."** It's a *different kind of thing* — it has no independent identity separate from its referent (no "address of the reference itself" concept), can't be reseated, and participates in overload resolution and template deduction differently than a pointer does.
5. **"Passing by `const&` is always cheaper than by value."** For cheap-to-copy types (an `int`, a small struct), pass-by-value can be *faster* because it avoids the indirection and, combined with move semantics (Ch03), can avoid a copy entirely on rvalue arguments. `const&` is a default for "unknown, possibly expensive" types, not a universal rule.
6. **"Member functions and free functions are interchangeable for operator overloading."** They interact differently with implicit conversions on the left-hand operand — see the Crash Course section above. This distinction is the subject of several problems below.
7. **"A lambda that captures `[&]` is always fine because it's `const`."** `[&]` capture-by-reference has nothing to do with `mutable`/`const` — it's about *what the lambda refers to*, and a reference-capturing lambda that escapes the scope of its captured variables is a dangling reference, full stop.
8. **"Defaulting `operator<=>` automatically gives you `operator==` too."** No — `<=>` and `==` are separate operations the language does not assume are equivalent (equality can sometimes be implemented more cheaply than "compare via `<=>` for the equal case"); a type wanting both must define or `= default` each one.
9. **"Undefined behavior just means the result is some unpredictable garbage value, so worst case I get a weird number."** No — a compiler is permitted to *assume UB never happens at all*, which means it can optimize on that assumption in ways that remove or restructure code that looks entirely unrelated to the UB itself; the actual observed symptom of UB is frequently far stranger (and far less locally-contained) than "a garbage value in one variable."

---

## Quick Checks

Short, single-answer. Answers in `SOLUTIONS.md` under "Quick Check Answers." Don't skip these to get to the Problems — several problems assume you've internalized these.

- **01-QC1.** Does `int x;` at namespace scope leave `x` indeterminate, like a block-scope local does?
- **01-QC2.** Is `constexpr int f(int n) { return n * 2; }` required to be evaluated at compile time when called as `f(5)` inside an ordinary (non-constexpr) function?
- **01-QC3.** Can a `constinit` variable also be `const`?
- **01-QC4.** Given `static int counter = 0;` inside a function body, what is its scope, and what is its storage duration?
- **01-QC5.** Can you have a reference to a reference (e.g. `int& & r = x;`)?
- **01-QC6.** What does `auto x = 5;` deduce `x`'s type as, versus `decltype(auto) x = 5;`?
- **01-QC7.** Given `int i = 0; decltype(i) a; decltype((i)) b = i;` — is `b`'s type `int` or `int&`?
- **01-QC8.** How many user-defined conversions may occur in a single implicit conversion sequence?
- **01-QC9.** Does marking a constructor `explicit` prevent `MyType m = MyType(5);`?
- **01-QC10.** What concrete type does a lambda expression have?
- **01-QC11.** Does `std::function<void()>` calling a captured-by-value lambda ever allocate on the heap?
- **01-QC12.** Is a `mutable` lambda's `operator()` itself `const`?
- **01-QC13.** If `struct P { int x; double y; auto operator<=>(const P&) const = default; };`, what comparison-category type does the defaulted `<=>` deduce, and why not `std::strong_ordering`?
- **01-QC14.** Does defining `operator<=>` for a type also give you a working `operator==`?
- **01-QC15.** Is "the compiler may assume this code path is unreachable" a plausible consequence of undefined behavior, or an exaggeration?
- **01-QC16.** Is the exact size of `int` on a given platform undefined behavior, unspecified behavior, or implementation-defined behavior?

---

## Problems

Ordered by increasing difficulty, L1 (Recognition) through L6 (Production) — this chapter has no L7 (Principal Reasoning) problems; those begin once ownership/lifetime/systems concerns exist in later chapters. Debugging problems are interleaved among the others and tagged **[DEBUG]** rather than pulled into their own section, so you encounter them without already knowing the code is broken.

### Level 1 — Recognition (8)

**01-P01.** For each of the following declarations, state whether the variable is default-initialized, zero-initialized, direct-initialized, or copy-initialized, and whether its value is determinate:
```cpp
int a;
int b{};
int c(7);
int d = 7;
std::string s;
```

**01-P02.** Classify each of the following as having automatic, static, thread, or dynamic storage duration:
```cpp
void f() {
    int x;
    static int y;
    thread_local int z;
    int* p = new int;
}
int g;
```

**01-P03.** Which of these are legal aggregate-initializations, and which are not, given:
```cpp
struct A { int x; int y; };
struct B { int x; B(int v) : x(v) {} };
struct C { private: int x; public: int y; };
```
```cpp
A a{1, 2};
B b{1};
C c{1, 2};
```

**01-P04.** State, for `const int* p`, `int* const p`, and `const int* const p`, what each protects — the pointee, the pointer itself, or both.

**01-P05.** Given `auto x = 5;`, `auto& y = x;`, and `const auto z = x;`, state the deduced type of each of `x`, `y`, and `z`.

**01-P06.** Is the following well-formed? If so, what is `r`'s relationship to `x` after the block?
```cpp
int& make_ref() {
    int x = 5;
    return x;
}
```

**01-P07.** For `struct Point { int x, y; };` and a variable `Point p;` at block scope, is `p.x` guaranteed to be `0`?

**01-P08.** Given two overloads `void f(int)` and `void f(double)`, which is selected by the call `f('a')`, and why?

### Level 2 — Prediction (15)

**01-P09.** Predict the compiler's behavior for each line:
```cpp
int a{3.14};     // (1)
int b = 3.14;    // (2)
int c(3.14);     // (3)
```

**01-P10. [DEBUG]** A teammate is confused why this doesn't compile:
```cpp
struct Widget {
    Widget(int id) : id_(id) {}
    int id_;
};
Widget w{1, 2};
```
What's actually happening, and what would make `{1, 2}`-style initialization work for `Widget` again?

**01-P11.** Predict the output:
```cpp
constexpr int square(int n) { return n * n; }
int arr[square(3)];       // (1)
int x = 5;
int y = square(x);        // (2)
```
Is line (1) guaranteed to compile-time-evaluate `square(3)`? Is line (2)?

**01-P12.** Does the following compile? If not, why not — be specific about which rule is violated.
```cpp
consteval int triple(int n) { return n * 3; }
int f(int runtime_value) {
    return triple(runtime_value);
}
```

**01-P13.** Explain what problem the following avoids that a plain `static int` would not, and why `constinit` (rather than `constexpr`) is the right tool:
```cpp
constinit int global_counter = compute_initial_value();
```
(Assume `compute_initial_value()` is itself `constexpr`-capable.)

**01-P14. [DEBUG]** Diagnose the bug:
```cpp
int total = 0;
for (int i = 0; i < 10; ++i) {
    int total = total + i;   // intent: accumulate
}
std::cout << total;
```

**01-P15.** State the scope and storage duration of `cache` in:
```cpp
int lookup(int key) {
    static std::unordered_map<int,int> cache;
    // ...
}
```
Is `cache` re-constructed on every call to `lookup`?

**01-P16.** Given `int x = 10; int& r = x; int* p = &x;`, predict the values of `x`, `r`, and `*p` after each of these lines, executed in order:
```cpp
r = 20;
*p = 30;
```

**01-P17.** Does the following compile? If not, identify the exact rule violated.
```cpp
int x = 5;
int& r;      // (1)
r = x;
```

**01-P18. [DEBUG]** A function returns a reference to a `thread_local`. Is the following safe across threads? Why or why not?
```cpp
int& counter() {
    thread_local int c = 0;
    return ++c;
}
```

**01-P19.** Predict the deduced types of `a`, `b`, and `c`:
```cpp
const std::string s = "hi";
auto  a = s;
auto& b = s;
auto&& c = std::string("temp");
```

**01-P20.** Given `int i = 42;`, what are the types of `decltype(i)`, `decltype((i))`, and `decltype(i + 0)`?

**01-P21.** Predict which overload is selected, and whether the call is ambiguous:
```cpp
void f(int);
void f(long);
f(3.0);   // double argument
```

**01-P22.** Given
```cpp
struct Meters { explicit Meters(double v) : v(v) {} double v; };
void move(Meters m);
```
which of these calls compile?
```cpp
move(5.0);          // (1)
move(Meters(5.0));  // (2)
move((Meters)5.0);  // (3)
```

**01-P23. [DEBUG]** This lambda is intended to be a running-total accumulator. Explain why it doesn't behave that way, and fix it:
```cpp
auto make_accumulator() {
    int total = 0;
    return [total](int x) { total += x; return total; };
}
```

### Level 3 — Implementation (12)

**01-P24.** Implement a function template-free `Meters` type (using operator overloading, not templates) that supports `Meters + Meters -> Meters` and `Meters * double -> Meters`, but does **not** implicitly convert from a raw `double`. State which constructor qualifier makes this true.

**01-P25.** Write a class `RangeChecked` wrapping an `int`, whose constructor is `constexpr` and which asserts (via a mechanism that works at both compile time and runtime) that the value is within `[0, 100]`. Explain what changes are needed for the check to actually fire during constant evaluation versus at runtime.

**01-P26.** Implement `operator<<` for a `Point{int x, int y;}` type such that `std::cout << p` prints `(x, y)`. Justify, in a comment, why this must be a free function rather than a member function.

**01-P27.** Implement a generic lambda (no explicit template) that accepts any two comparable values and returns the larger one. State its effective signature as the compiler would generate it.

**01-P28.** Write a function `constexpr auto make_lookup()` that returns a `std::array` computed entirely at compile time from a `constexpr` formula (e.g., squares of 0..9). Verify it can be used as an array bound elsewhere.

**01-P29. [DEBUG]** The following is intended to memoize expensive results per call site but has a subtle bug in a multi-threaded context. Identify it. (You are not expected to fix it with synchronization primitives yet — those are Ch11. Just diagnose it.)
```cpp
int expensive_compute(int key) {
    static std::unordered_map<int, int> cache;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    int result = /* ... expensive ... */ key * key;
    cache[key] = result;
    return result;
}
```

**01-P30.** Write a `std::function<int(int)>` variable that at different points in a program holds: a free function, a capturing lambda, and a functor object with `operator()`. Confirm all three compile through the same variable.

**01-P31.** Using `std::invoke`, write a single helper `call_any` that can invoke a free function, a member function via a pointer-to-member, and a lambda, all through one template. State what `std::invoke` does that plain `f(args...)` syntax cannot.

**01-P32.** Write two overloads of `clamp`, one taking `int` and one taking `double`, and demonstrate a call site where the choice of overload is ambiguous. Then fix the ambiguity without adding a third overload.

**01-P33. [DEBUG]** This code is intended to select the `double` overload but silently doesn't as expected on one compiler and does on another. Explain the divergence.
```cpp
void f(int) { /* A */ }
void f(float) { /* B */ }
f(3.0);  // double literal
```

**01-P34.** Implement a `constexpr` function `is_power_of_two(unsigned n)` and demonstrate its use both as a compile-time array-bound expression and as an ordinary runtime boolean check on user input, in the same program.

**01-P35.** Write a lambda that captures a `unique`-ish local by reference and demonstrate — via a comment, not a crash — the exact code pattern that would make it dangle. Then show the by-value-capture fix.

### Level 4 — Debugging & Deeper Prediction (6, interleaved above via [DEBUG] tags; standalone continuation below)

**01-P36. [DEBUG]** A colleague's code compiles cleanly with `-Wall` off but they suspect UB. Find it:
```cpp
int f() {
    int x;
    if (some_runtime_condition()) x = 1;
    return x;
}
```

**01-P37. [DEBUG]** Explain why this fails to compile, and what the fix reveals about `decltype`:
```cpp
template <typename T, typename U>
auto add(T t, U u) -> decltype(t + u) {
    return t + u;
}
auto r = add(std::string("a"), 1);  // intended: error, but WHY specifically?
```

**01-P38. [DEBUG]** This function is supposed to return a reference to whichever of two ints is larger, to allow the caller to modify it. Explain the actual bug:
```cpp
int& larger(int a, int b) {
    return a > b ? a : b;
}
```

**01-P39.** A struct has both an aggregate-style shape and a user-declared constructor added later for validation. Given the before/after code, explain exactly which call sites silently change meaning (not just which fail to compile):
```cpp
// before
struct Config { int retries; int timeout_ms; };
Config c{3, 1000};

// after
struct Config {
    Config(int r, int t) : retries(r), timeout_ms(t) { /* validate */ }
    int retries; int timeout_ms;
};
Config c{3, 1000};
```

**01-P40.** Given the following, predict whether it compiles, and if so, what `v` prints. Explain in terms of overload resolution and explicit conversions:
```cpp
struct Wrapper { explicit Wrapper(int v) : v(v) {} int v; };
std::vector<Wrapper> vec;
vec.push_back(5);  // ?
vec.emplace_back(5);  // ?
```

**01-P41.** Explain, using the exact rule name, why the following does not compile, and provide the minimal fix:
```cpp
struct A { operator int() const { return 1; } };
struct B { operator int() const { return 2; } };
void f(int);
A a; B b;
f(a);  // ok? consider what happens if instead f took a user-defined type requiring both conversions
```
(Focus your answer on the general principle this demonstrates about chained user-defined conversions, using a call shape that would require two.)

### Level 5 — Integration-Adjacent (3)

**01-P42.** Design (and implement) a small `Angle` type that stores degrees internally, is constructed only via a named factory function `Angle::from_degrees(double)` or `Angle::from_radians(double)` (no public constructor taking a bare number), supports `operator+`, and converts to `double` degrees only via an explicit conversion operator. Justify each design choice (why no public constructor, why explicit conversion) in terms of overload-resolution and implicit-conversion pitfalls this chapter covered.

**01-P43.** Write a `constexpr`-evaluable lookup table generator (a function returning a fixed-size `std::array`) for the first N Fibonacci numbers, where N is a template parameter, and demonstrate it being used both in a `static_assert` and as a runtime-computed value from user input — i.e., the *same* function code serving both a compile-time and a runtime call site. Explain why `constexpr` (not `consteval`) is the correct choice here.

**01-P44. [DEBUG]** A library exposes:
```cpp
class Logger {
public:
    Logger(std::string name) : name_(std::move(name)) {}
    void log(const std::string& msg) const;
private:
    std::string name_;
};
void configure(Logger logger);  // takes by value
```
and a caller writes `configure("app");`. It compiles. Explain the full chain of implicit conversions and constructor calls that makes this work, and identify the one change to `Logger`'s constructor that would break this call site while breaking almost nothing else in the codebase.

### Level 6 — Production Judgment (1)

**01-P45.** You are reviewing a pull request that changes a widely-used public struct from an aggregate:
```cpp
struct Rect { int width; int height; };
```
to a validated type:
```cpp
class Rect {
public:
    Rect(int width, int height);
    int width() const;
    int height() const;
private:
    int width_, height_;
};
```
Write a short review comment (a paragraph, not code) identifying every category of call site this change silently breaks or changes the meaning of (not just ones that fail to compile), referencing the specific initialization/overload-resolution rules from this chapter that are responsible for each. Then state what a *safer* migration path would look like.

### Level 2 — Prediction (continued: `<=>`, UB taxonomy)

**01-P46.**
```cpp
struct Version {
    int major, minor, patch;
    auto operator<=>(const Version&) const = default;
};
Version a{1, 2, 3}, b{1, 3, 0};
std::cout << (a < b) << ' ' << (a == b);
```
Predict the output, and state exactly which comparison-category type (`std::strong_ordering`, `std::weak_ordering`, or `std::partial_ordering`) the defaulted `<=>` deduces for `Version`, given that every member is an `int`. Also predict whether `a == b` even compiles as written, given only `operator<=>` is defaulted — if it doesn't, explain precisely why not.

**01-P47.**
```cpp
struct Reading {
    double value;
    auto operator<=>(const Reading&) const = default;
};
Reading r1{1.0}, r2{std::numeric_limits<double>::quiet_NaN()};
// caller wants to know: is r1 < r2 well-defined, and what comparison category applies?
```
Predict which comparison-category type the defaulted `<=>` deduces here (not `std::strong_ordering` — explain specifically why a floating-point member forces a weaker category), and predict what `r1 < r2` evaluates to given that `r2`'s value is NaN.

### Level 4 — Debugging (continued: UB taxonomy)

**01-P48. [DEBUG]**
```cpp
int compute(int x) {
    int y;              // uninitialized
    if (x > 0) y = x * 2;
    return y;            // read of y when x <= 0
}
```
A colleague argues "worst case, `compute(-1)` just returns some garbage integer, which is annoying but harmless since we're only printing it." Identify precisely which category (undefined, unspecified, or implementation-defined) this bug falls into, and explain — using this chapter's UB section — why "garbage integer, but otherwise harmless" understates what a compiler is actually permitted to do here, including with code elsewhere in the same function that has no obvious connection to `y`.

### Level 6 — Production Judgment (continued: UB taxonomy)

**01-P49.** A code review comment on a pull request says: "This relies on evaluation order between `f(a(), b())`'s two arguments, but that's implementation-defined, so it's fine as long as we document which compiler we target." Evaluate whether this claim (specifically, the *category* being invoked) is accurate, referencing the precise distinction this chapter draws between unspecified and implementation-defined behavior, and state what should actually be verified (or changed in the code) before accepting this justification.

---

## Integration Challenge

**01-IC1.** Build a small, `constexpr`-evaluable unit-conversion table using operator overloading and overload resolution across implicit and explicit conversions. Specifically:

- Define at least two strong-typedef-style unit types (e.g. `Meters`, `Feet`) that are *not* implicitly interconvertible with each other or with `double`.
- Provide an explicit, named conversion between them (e.g. `to_feet(Meters)`), not an implicit conversion operator, and explain in a comment why an implicit conversion here would be a design mistake given this chapter's overload-resolution rules.
- Support `operator+` between two values of the *same* unit type, and make at least one usage of it `constexpr`-evaluable in a `static_assert`.
- Provide a `constexpr` factory function per unit type (not a public converting constructor) so that `Meters(5)` (bare numeric construction) is not directly callable from outside the type.

This deliberately exercises initialization, `const`/`constexpr`, explicit conversions, and operator overloading together — the four hardest-to-separate concepts in the chapter.

---

## Chapter Project

**P-1.1 — Strong Typedef & Unit-Safe Quantity Type.** See [`PROJECT_ROADMAP.md`](../PROJECT_ROADMAP.md) for the pointer entry; full problem statement lives in `projects/level-1/strong-typedef/STATEMENT.md` once generated (not yet — see repository `README.md` Status). This project is a larger-scope continuation of the Integration Challenge above: a general-purpose, reusable strong-typedef/unit-safe quantity template, rather than the two hand-written unit types above.

---

## Next

Solutions for every Quick Check and Problem above are in [`SOLUTIONS.md`](SOLUTIONS.md). Don't open it until you've attempted the problem — see `PROGRESS.md`'s Definitions of Done for why "read the solution first" doesn't count as solving it.
