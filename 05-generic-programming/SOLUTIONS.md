# Chapter 05 — Solutions

## Quick Check Answers

**05-QC1.** A non-template function is fully type-checked once, at its own definition. A template's body is only type-checked *per instantiation* — the compiler doesn't (and largely can't, since `T` is unknown until a call site fixes it) verify the body against an abstract "any `T`" at definition time. "Compiles for `int`" only proves the specific expressions used happen to be valid for `int`; nothing guarantees those same expressions are valid for `std::string` until that instantiation is actually attempted.

**05-QC2.** No. Only class templates can be partially specialized. A function template can only be *overloaded* for a more specific pattern — the compiler then picks the best-matching overload via normal overload resolution, which is a related but distinct mechanism from partial specialization's pattern-matching.

**05-QC3.** An `enable_if_t` failure, when no overload matches, presents as a wall of "candidate template ignored: substitution failure [...]" messages — one per failed candidate, each showing the internal `enable_if_t` machinery rather than naming the actual missing capability. A concept failure produces a direct "constraint not satisfied" diagnostic naming the specific concept and, often, which requirement inside it failed — a qualitatively shorter, more legible path from error message to root cause.

**05-QC4.** `if constexpr`'s condition is evaluated at compile time, and the branch not selected is discarded before instantiation — it is never compiled for that specific template instantiation, so it never needs to be *valid* code for that `T`. A runtime `if (false)` branch is still part of the function's single compiled body; the compiler must successfully compile it (as ordinary, always-present code) even though it will never execute.

**05-QC5.** No. CRTP resolves the derived type at compile time via the template parameter — there is one concrete `Base<Derived>` per `Derived`, with no vtable and no virtual dispatch. A `Base<Widget>*` cannot point at a `Base<Gadget>` object; there is no single type that could uniformly hold "any CRTP-derived type" the way a virtual base's pointer holds any of its derived classes.

**05-QC6.** `(args + ...)` (the `(pack op ...)` form) is a **right fold**: for `{a, b, c}` it expands to `a + (b + c)`. For plain addition the grouping is invisible in the result — it computes the sum of all three elements either way — but the fold *direction* is what the question is really asking about, and `(pack op ...)` always groups from the right, while the alternate `(... op pack)` form groups from the left as `(a + b) + c`.

**05-QC7.** One level of indirection through the erased interface's dispatch (typically a virtual call or an equivalent function-pointer table) plus, for owning type-erasure wrappers like `std::function`, typically a heap allocation to store the concrete value (unless a small-object optimization inline buffer applies) — costs a plain template parameter never pays, since the concrete type is baked into the instantiation with no runtime dispatch needed.

**05-QC8.** Because every distinct combination of policy template arguments used anywhere in the program is a genuinely distinct type requiring its own separate instantiation, type-checking, and code generation — N independently-varying policy slots with even a handful of implementations each multiplies out to many more instantiated classes than a single non-templated implementation with runtime `if`/flag checks, which only ever compiles one class regardless of how many behavioral combinations are reachable at runtime.

## Problem Solutions

### Level 1 — Recognition

**05-P01.**

**Reference Solution:** `identity(5)` — deduction, `T` deduces to `int`, instantiates `identity<int>`. `identity<double>(5)` — `T` is explicitly specified as `double`; `5` (an `int`) implicitly converts to `double` at the call site, instantiates `identity<double>`. `identity("hello")` — deduction; the argument's type is `const char[6]` decaying to `const char*` in most deduction contexts for a by-value parameter, so `T` deduces to `const char*`, instantiates `identity<const char*>`.

**Explanation:** Explicit template arguments suppress deduction for that parameter entirely — the compiler doesn't try to infer `T` from the argument, it just checks the argument converts to the specified `T`.

---

**05-P02.**

**Reference Solution:** No. When a full specialization exists for a given set of arguments, the compiler uses *only* that specialization's definition for those arguments — the primary template's body is not instantiated, consulted, or even required to be valid for `bool` in this program (as long as it's never itself explicitly used).

**Explanation:** A full specialization is a wholesale replacement, not an override layered on top of the primary — there's no notion of "falling back" to the primary's members for a type that has its own full specialization.

---

**05-P03.**

**Reference Solution:** Partial specialization of a function template — **illegal**. Partial specialization of a class template — **legal**. An overload of a function template for a more specific type pattern — **legal** (this is precisely the mechanism used *instead of* function-template partial specialization).

**Explanation:** The language deliberately omits function-template partial specialization because ordinary overload resolution already provides equivalent (and more flexible, since it interacts correctly with argument-dependent lookup and implicit conversions) behavior for "pick the more specific match."

---

**05-P04.**

**Reference Solution:** `std::is_integral<T>` — type trait. `std::same_as<T, U>` — concept (as stated). `std::vector<T>` — neither (it's a class template, an ordinary generic type, not a predicate). `std::remove_cv_t<T>` — type trait (specifically a type *transformation* trait, not a boolean predicate, but still in the type-trait family).

**Explanation:** Type traits and concepts overlap heavily in *implementation* (many concepts are defined directly in terms of type traits) but differ in *role*: a trait computes or checks something about a type; a concept is specifically the vocabulary used to constrain a template parameter.

---

### Level 2 — Prediction

**05-P05.**

**Reference Solution:** Yes, it compiles. Because `f` is never called with any type, `f<T>` is never instantiated, and an uninstantiated template's body is not required to be valid for any particular `T` — only the template's syntax (parsing) is checked at definition time, not full semantic validity of a body containing a dependent-type member-function call.

**Explanation:** This is the direct consequence of Common Misconception #1: nothing is compiled, in the "type-checked and code-generated" sense, for a template until something forces an instantiation.

---

**05-P06.**

**Reference Solution:** Output is `110`. `Trait<int>::value` uses the **full specialization** (`1`) — the full specialization for exactly `int` always wins over the unrelated (non-matching) partial specialization for `T*`, since `int` isn't a pointer type at all and the partial specialization's pattern simply doesn't match; there's no ambiguity here because only one specialization's pattern actually fits `int`. `Trait<int*>::value` matches the partial specialization for `T*` (`1`). `Trait<double>::value` matches neither specialization, falling back to the primary template (`0`).

**Explanation:** "Could both apply" is a trick framing — the partial specialization's pattern is `T*`, and `int` is not a pointer type under any substitution of `T`, so it structurally cannot match; there's no competition to resolve for `Trait<int>` at all.

---

**05-P07.**

**Reference Solution:** `print_kind(5)` → `"integral\n"`. `print_kind(5.0)` → `"float\n"`. `print_kind('c')` → `"integral\n"` — `char` satisfies `std::is_integral_v` (the standard classifies `char`, along with `bool`, the various `int` widths, and other character types, as integral types).

**Explanation:** `is_integral_v<char>` being `true` is a common surprise for readers who associate "integral" purely with the `int` family; the standard's definition of integral types is broader, covering every type that's fundamentally represented as a whole-number bit pattern with integer arithmetic semantics.

---

**05-P08.**

**Reference Solution:** Output is `3`. `sizeof...` is purely a compile-time count of the pack's elements — it never evaluates, inspects the values of, or even requires the arguments to be usable expressions; it only needs the pack's *arity* to be known, which it always is once the template is instantiated.

**Explanation:** This distinguishes `sizeof...(pack)` from `sizeof(expr)`, which (for non-VLA cases) is also usually compile-time but conceptually operates on one expression's type rather than a pack's cardinality.

---

**05-P09.**

**Reference Solution:** No, that branch's code is never compiled into the `T = double` instantiation. `if constexpr (std::is_floating_point_v<T>)` is `true` for `double`, so the `else` branch (containing the `throw`) is the discarded branch for this specialization and is never instantiated — it does not need to be valid code for `double` (and, incidentally, it wouldn't especially matter here since it would be valid for `double` too, but the point generalizes to cases where the discarded branch genuinely wouldn't compile for that `T`).

**Explanation:** Direct application of the `if constexpr` discard rule from the Crash Course and QC4 — the discarded branch is dropped before instantiation-time type-checking ever reaches it.

---

**05-P10.**

**Reference Solution:** `show(5)` compiles — `int` satisfies `Printable` since `std::cout << 5` is well-formed. `show(NoOutput{})` fails to compile — `NoOutput` has no `operator<<` overload, so it doesn't satisfy `Printable`. The failure presents as a direct "constraint `Printable` not satisfied by `NoOutput`" style diagnostic, typically also showing the specific `requires`-expression clause (`std::cout << t`) that failed — a short, targeted message. The SFINAE-based equivalent (an `enable_if_t` constraint built on the same "is streamable" check) would instead fail with a "no matching overload" error listing every candidate `show` and, for each rejected candidate, a substitution-failure note — a longer, more mechanical error requiring the reader to infer *why* the substitution failed rather than being told directly.

---

**05-P11.**

**Reference Solution:** Output is `"Derived override\n"`. `b.greet()` calls `Base<Derived>::greet`, which does `static_cast<Derived*>(this)->greet_impl()` — since `this` genuinely points at a `Derived` object (constructed as `new Derived()`), the cast is valid and resolves to `Derived::greet_impl`, which hides/shadows `Base`'s `greet_impl` for that call.

**Explanation:** This is CRTP's static-polymorphism mechanism working exactly as designed — no vtable is involved; the "override" is really just ordinary name lookup on the concrete `Derived` type reached via the `static_cast`.

---

**05-P12.**

**Reference Solution:** Entirely unrelated types, with no implicit conversion between them. `Container<int>` and `Container<double>` are two independently instantiated classes that happen to share a common template definition — they are no more related to each other than any two unrelated classes would be; neither derives from the other, and passing a `Container<int>` where a `Container<double>` is expected (or vice versa) is a type error.

**Explanation:** This is Common Misconception #1 applied to class templates specifically — "same template" is not "same type" or "related types" in any inheritance sense.

---

**05-P13.**

**Reference Solution:** `r1`'s deduced type is `double` (the usual arithmetic conversions promote `int + double` to `double`). `r2`'s deduced type is `std::string` (via `std::string::operator+`). Yes, a trailing return type's `decltype(a + b)` requires `a` and `b` to already be in scope, which they are — the trailing-return-type syntax places the return-type expression *after* the parameter list, so the parameters are already declared and visible by the time `decltype` is evaluated; a leading return type (before the parameter list) would not have `a`/`b` in scope at all, which is precisely why this pattern requires the trailing form.

---

**05-P14.**

**Reference Solution:** `f(5)` — `5` is an rvalue `int`, so `T` deduces to `int` (and `T&&` collapses to `int&&`, an rvalue reference). `f(some_lvalue_int)` — the argument is an lvalue, so `T` deduces to `int&` (and `T&&` becomes `int& &&`, which reference-collapses to `int&`).

**Explanation:** This is the same forwarding-reference deduction rule from Ch03, unchanged by appearing inside a template that could in principle have additional template parameters — the deduction rule operates per-parameter and doesn't care how many other template parameters coexist alongside `T`.

---

**05-P15.**

**Reference Solution:** Output is `1 2.5 three\n`. `(std::cout << ... << args)` is a `(unary-op ... op pack)`-shaped fold with `std::cout` as the initial left operand — it expands left-associatively as `((std::cout << arg1) << arg2) << arg3`, i.e., `std::cout` is the leftmost operand and each subsequent argument is chained onto it in left-to-right order, matching `operator<<`'s natural chaining and producing the arguments printed in the order passed.

---

**05-P16.**

**Reference Solution:** `print_size(std::vector<int>{1,2,3})` resolves to the **constrained** overload (prints `3`) — `std::vector<int>` satisfies `HasSize`, and when both a constrained and an unconstrained overload are viable candidates, overload resolution prefers the more constrained (more specific) one. `print_size(5)` resolves to the **unconstrained** overload (prints `"no size\n"`) — `int` has no `.size()` member, so the constrained overload is removed from the candidate set entirely, leaving only the unconstrained one.

**Explanation:** This is analogous to preferring a more specialized function-template overload in ordinary overload resolution — a satisfied constraint makes an overload strictly more specific than an unconstrained sibling, so it wins whenever both are otherwise viable.

---

### Level 3 — Implementation

**05-P17.**

**Reference Solution:**
```cpp
template<typename T>
concept Orderable = requires(T a, T b) { a < b; a > b; };

template<Orderable T>
T clamp_value(T x, T lo, T hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}
```

**Explanation:** The concept states exactly the two operations `clamp_value`'s body actually uses (`<` and `>`) rather than reaching for a broader "totally ordered" notion the implementation doesn't need — narrowly-scoped constraints are easier to satisfy for user types and document intent precisely.

---

**05-P18.**

**Reference Solution:**
```cpp
template<typename... Ts>
auto sum_all(Ts... args) {
    return (args + ...);
}
```
Called with zero arguments, `(args + ...)` for an empty pack with operator `+` is a **compile error** — unlike logical `&&`/`||` (which have well-defined identities, `true`/`false`), and unlike `,` (comma), `+` has no language-defined empty-pack identity value, so the fold expression is simply ill-formed for an empty pack unless an explicit initial value is supplied via the alternate fold form `(0 + ... + args)`.

**Explanation:** This is a genuine gap `std::accumulate` avoids by requiring an explicit initial value parameter; fold expressions only get a "free" empty-pack identity for a small, fixed set of operators the language special-cases (`&&`→`true`, `||`→`false`, `,`→void expression), not for `+`.

---

**05-P19.**

**Reference Solution:**
```cpp
template<typename T, typename = void>
struct is_container : std::false_type {};

template<typename T>
struct is_container<T, std::void_t<decltype(std::declval<T>().begin()),
                                    decltype(std::declval<T>().end())>>
    : std::true_type {};

template<typename T>
inline constexpr bool is_container_v = is_container<T>::value;

// C++20 concept equivalent:
template<typename T>
concept Container = requires(T t) { t.begin(); t.end(); };
```

**Explanation:** The SFINAE version requires a `void_t`-based detection idiom, a defaulted second template parameter, and a partial specialization that only matches when both `.begin()`/`.end()` substitutions succeed — several moving parts working together indirectly. The concept version states the same requirement directly as a `requires`-expression in roughly a fifth of the code, with no auxiliary machinery — a clear illustration of why concepts replaced this idiom for new code.

---

**05-P20.**

**Reference Solution:**
```cpp
template<typename Derived>
class CountedBase {
    static inline int count_ = 0;
public:
    CountedBase() { ++count_; }
    ~CountedBase() { --count_; }
    static int count() { return count_; }
};

class Widget : public CountedBase<Widget> {};
class Gadget : public CountedBase<Gadget> {};
```

**Explanation:** `CountedBase<Widget>` and `CountedBase<Gadget>` are two entirely distinct instantiated classes (per 05-P12's "different template argument, different type" rule), each with its *own* independent `static inline int count_` — templating the base on `Derived` is precisely what forces a separate instantiation, and therefore a separate static counter, per derived class. A single non-templated `CountedBase` would have exactly one `count_` shared by every class that inherits from it, mixing all derived types' counts together.

---

**05-P21.**

**Reference Solution:**
```cpp
struct ConsolePolicy { static void write(std::string_view s) { std::cout << s; } };
struct NullPolicy     { static void write(std::string_view)   {} };

template<typename OutputPolicy>
class Logger {
public:
    void log(std::string_view msg) { OutputPolicy::write(msg); }
};
```

**Explanation:** `Logger<NullPolicy>::log`'s body is, after inlining, a call to a static function whose entire body is empty — the optimizer can see (at compile time, since the policy method is known and its body is trivially empty, with no side effects and no observable state) that the call produces no effect whatsoever and can eliminate it entirely, all the way up through `log`'s own call sites, with zero runtime branching ever generated to "check" which policy is active.

---

**05-P22.**

**Reference Solution:**
```cpp
class AnyDrawable {
    struct DrawableConcept {
        virtual ~DrawableConcept() = default;
        virtual void draw() const = 0;
    };
    template<typename T>
    struct DrawableModel : DrawableConcept {
        T value;
        explicit DrawableModel(T v) : value(std::move(v)) {}
        void draw() const override { value.draw(); }
    };
    std::unique_ptr<DrawableConcept> impl_;
public:
    template<typename T>
    AnyDrawable(T v) : impl_(std::make_unique<DrawableModel<T>>(std::move(v))) {}
    void draw() const { impl_->draw(); }
};
```

**Explanation:** Only `DrawableConcept::draw` is virtual — that single virtual call is the one indirection needed to dispatch, at runtime, to whichever concrete `T`'s `.draw()` was captured at construction; everything else (`AnyDrawable::draw`, the constructor) is ordinary non-virtual code that simply forwards through that one dispatch point.

---

**05-P23.**

**Reference Solution:**
```cpp
template<typename T>
constexpr T factorial(T n) { return n <= 1 ? T{1} : n * factorial(n - 1); }

constexpr int five_fact = factorial(5);
std::array<int, five_fact> arr{};    // compile-time use

int runtime_n = 6;
int r = factorial(runtime_n);        // runtime use
```

**Explanation:** A `constexpr` function is legal to evaluate at compile time *when all its inputs are themselves compile-time constants and the evaluation would not require anything the compiler can't do at compile time* (no I/O, no non-constexpr calls); the same body remains an entirely ordinary function otherwise, callable at runtime with runtime values, with no separate "runtime version" needed — the language rule is that `constexpr` marks a function as *eligible* for compile-time evaluation, not as exclusively compile-time.

---

**05-P24.**

**Reference Solution:**
```cpp
template<typename... Ts>
bool all_true(Ts... args) { return (args && ...); }

template<typename... Ts>
bool any_true(Ts... args) { return (args || ...); }
```
`all_true()` (zero arguments) evaluates to `true` — the language special-cases `&&`'s empty-pack fold as `true`, which is exactly the correct logical identity for AND (a conjunction of zero terms is vacuously true). `any_true()`'s correct OR identity would be `false` (a disjunction of zero terms is vacuously false); the language does produce this automatically — `||`'s empty-pack fold is specifically defined as `false` — so no special case is needed for either function.

---

**05-P25.**

**Reference Solution:**
```cpp
template<typename F, typename... Args>
concept InvocableReturning = requires(F f, Args... args) { { f(args...) } -> std::same_as<bool>; };

auto good = [](int x) { return x > 0; };            // satisfies: returns bool
auto bad  = [](int x) { return x;    };             // fails: returns int, not bool

static_assert(InvocableReturning<decltype(good), int>);
static_assert(!InvocableReturning<decltype(bad), int>);
```

**Explanation:** `{ expr } -> Concept` checks that `expr` is well-formed **and** that its resulting type satisfies `Concept` (here, `std::same_as<bool>` — exactly `bool`, not merely convertible to `bool`). A bare `{ expr; }` only checks that the expression is well-formed at all, saying nothing about the resulting type — `bad`'s `int`-returning lambda would satisfy a bare `{ f(args...); }` requirement but correctly fails the type-checked `-> std::same_as<bool>` form.

---

**05-P26.**

**Reference Solution:**
```cpp
template<typename T>
struct remove_all_pointers { using type = T; };                       // base case

template<typename T>
struct remove_all_pointers<T*> { using type = typename remove_all_pointers<T>::type; };  // recursive case
```

**Explanation:** Structurally, only **two** specializations total are needed: the primary template (serving as the base case, for any non-pointer `T`) and exactly one partial specialization matching the pattern `T*` (the recursive case, which peels off one pointer level and recurses on what remains). No additional specializations are required for "two levels of pointer," "three levels," etc. — the single `T*` partial specialization handles every pointer depth via repeated self-application.

---

**05-P27.**

**Reference Solution:**
```cpp
template<typename C>
concept Container = requires(C c) { c.begin(); c.end(); };   // per 05-P19's concept

template<typename Container_, typename Predicate>
    requires Container<Container_>
auto count_matching(const Container_& c, Predicate pred) {
    size_t n = 0;
    for (const auto& x : c) if (pred(x)) ++n;
    return n;
}
```
Demonstrated with `std::vector<int>` and a predicate `[](int x){ return x % 2 == 0; }`, and with `std::list<std::string>` and a predicate `[](const std::string& s){ return s.size() > 3; }` — both compile and produce correct counts, since the `Predicate` template parameter is deduced independently per call and only needs to be invocable with that container's actual value type.

---

**05-P28.**

**Reference Solution:**
```cpp
template<typename T, size_t N>
class FixedRing {
    T data_[N];
    size_t head_ = 0, count_ = 0;
public:
    void push(T x) {
        data_[(head_ + count_) % N] = std::move(x);
        if (count_ < N) ++count_; else head_ = (head_ + 1) % N;
    }
    T pop() {
        T front = std::move(data_[head_]);
        head_ = (head_ + 1) % N; --count_;
        return front;
    }
    size_t size() const { return count_; }
    static constexpr size_t capacity() { return N; }
};
static_assert(FixedRing<int, 8>::capacity() == 8);
```

**Explanation:** Because `N` is a template parameter, its value is known at compile time for every instantiation — this is exactly what lets `T data_[N];` be an ordinary fixed-size array *member*, sized once at compile time, rather than a runtime-sized allocation; a constructor-argument `N` would only be known at runtime, which a fixed-size array member fundamentally cannot accommodate (array bounds in that position must be compile-time constants).

---

**05-P29.**

**Reference Solution:**
```cpp
template<typename T, typename = void>
struct is_streamable : std::false_type {};

template<typename T>
struct is_streamable<T, std::void_t<decltype(std::cout << std::declval<T>())>>
    : std::true_type {};

template<typename T>
void debug_print(const T& x) {
    if constexpr (is_streamable<T>::value) std::cout << x;
    else std::cout << "<unprintable>";
}

struct NotStreamable {};
debug_print(42);              // exercises the streamable branch
debug_print(NotStreamable{}); // exercises the fallback branch
```

**Explanation:** Following 05-P19's `void_t`-detection idiom directly for `operator<<` well-formedness. Because the selection is via `if constexpr` (not a runtime `if`), only the branch matching each specific `T`'s trait value is ever instantiated for that `T` — `debug_print<NotStreamable>` never attempts to instantiate `std::cout << x`, so it doesn't fail to compile despite `NotStreamable` having no such operator.

---

**05-P30.**

**Reference Solution:**
```cpp
template<typename T>
class Wrapper {
    T value_;
public:
    explicit Wrapper(T v) : value_(std::move(v)) {}
    explicit operator T() const { return value_; }
    template<typename U> requires std::convertible_to<T, U>
    operator U() const { return static_cast<U>(value_); }
};
```
An unconstrained templated `operator U()` alongside the exact `explicit operator T()` creates ambiguity at a call site like `double d = static_cast<double>(w)` when `T` is `double`-adjacent — both the exact-`T` operator and `U = double` of the templated operator are viable, and overload resolution has no basis to strictly prefer one, producing an ambiguous-conversion error.

**Explanation:** Constraining the templated conversion with `requires std::convertible_to<T, U>` doesn't, by itself, resolve the ambiguity between the two operators when `U` happens to equal `T` exactly (both are still viable for that exact case) — the practical fix is to additionally exclude `U == T` from the templated overload's constraint (`requires (!std::same_as<T, U>) && std::convertible_to<T, U>`), leaving the non-templated `explicit operator T()` as the sole candidate whenever the target type is exactly `T`, and the templated one as the sole candidate for every genuinely different, convertible `U`.

---

### Level 4 — Debugging

**05-P31.** [DEBUG]

**Reference Solution:** This does not compile. `push`'s parameter `T x` is taken **by value**, which requires *copying* the argument into the parameter — but `std::unique_ptr<int>` has no copy constructor (it's move-only), so passing `std::make_unique<int>(5)` (an rvalue, movable into the parameter) still fails because the parameter-binding itself is specified as a by-value copy-construction in the general case the compiler must support for `push`'s single signature, and more concretely: `data_.push_back(x)` inside `push`'s body copies `x` into the vector, which is the actual failing call for a move-only `T`. Fix: take `x` by value (already correct for supporting both copies and moves at the *call* boundary via the usual "pass by value, then move into storage" idiom) but move it into the vector explicitly:
```cpp
void push(T x) { data_.push_back(std::move(x)); }
```
`pop` also needs `std::move`, which it already correctly has (`T top = data_.back();` is itself a copy-construction bug for move-only `T` — should be `T top = std::move(data_.back());`).

---

**05-P32.** [DEBUG]

**Reference Solution:** `r`'s actual type is `double`, not `int`. `value + other` is `int + double` (the usual arithmetic conversions promote the mixed-type expression to the wider/floating type), so `decltype(value + other)` — and therefore `add`'s deduced `auto` return type — is `double`. The `static_assert` is simply asserting a false premise; the correct fix is:
```cpp
static_assert(std::is_same_v<decltype(r), double>);
```
"Fixing" `add` to force an `int` result (e.g., via an explicit cast) would silently discard the fractional part of any mixed-type addition — a strictly worse outcome that trades a caught compile-time assertion failure for a silent runtime precision bug.

---

**05-P33.** [DEBUG]

**Reference Solution:** `Point` has no `operator<`, so `Comparable<Point>` is not satisfied (the `requires`-expression's `a < b` is ill-formed for two `Point`s), and `my_min<Point>` is rejected at the constraint-check step with a "constraint not satisfied" diagnostic. Fix (a) — add `operator<` as a member or hidden friend directly to `Point`:
```cpp
struct Point { int x, y; bool operator<(const Point& o) const { return x*x+y*y < o.x*o.x+o.y*o.y; } };
```
Fix (b) — leave `Point` unchanged and supply `operator<` as a free, non-member function found via ordinary lookup/ADL:
```cpp
bool operator<(const Point& a, const Point& b) { return a.x*a.x+a.y*a.y < b.x*b.x+b.y*b.y; }
```
This is exactly the "non-intrusive extension via free functions / ADL" pattern already relied on for user-defined `operator<<` in Ch04's stream-insertion territory — types you don't own (or don't want to modify) can still satisfy externally-defined concepts as long as the required operation can be supplied as a free function.

---

**05-P34.** [DEBUG]

**Reference Solution:** This compiles fine because `Counter<Widget>` is a valid instantiation regardless of which class actually inherits from it — nothing in the CRTP pattern itself checks that the base's template argument matches the derived class doing the inheriting. At runtime: both `Widget` and `Gadget` construct instances of the *same* instantiated class, `Counter<Widget>` (since `Gadget : public Counter<Widget>` uses `Widget` as the argument, not `Gadget`), so they share one single `static int count_` between them. `Widget::count()` and `Gadget::count()` are actually the same static member of the same instantiated base class — both report the *combined* count of all `Widget` **and** `Gadget` instances ever constructed, not independent per-derived-class counts as the design presumably intended. This is precisely the failure mode 05-P20's CRTP mixin is designed to *avoid* when the pattern is used correctly (`Derived : public CountedBase<Derived>`, matching itself) — here the copy-paste bug breaks that invariant silently, with no compile error at all.

---

**05-P35.** [DEBUG]

**Reference Solution:** This fails to compile because a function using deduced (`auto`) return type must have every one of its `return` statements deduce to the *same* type — and the compiler must consider **all** `return` statements in the function's source text as written, including the trailing `return T{};`, even though that line is (for any actually-taken code path) unreachable after the `if constexpr`/`else`'s own returns. `if constexpr` discarding a *branch* doesn't remove code that sits entirely outside both branches — the trailing `return T{};` is neither branch, so it's always present, and its type (`T`) generally differs from `container[0]`'s or `container.front()`'s deduced type (an element reference or value, not `T` itself), producing a return-type-deduction conflict. Fix: remove the unreachable trailing statement entirely (it can never actually execute, since both branches of the `if constexpr`/`else` already return):
```cpp
template<typename T>
auto get_first(T&& container) {
    if constexpr (std::is_array_v<std::remove_reference_t<T>>) {
        return container[0];
    } else {
        return container.front();
    }
}
```

---

**05-P36.** [DEBUG]

**Reference Solution:** This fails to compile because `Wrapper<int>` and `Wrapper<double>` are two entirely distinct, unrelated classes (per 05-P12) — `Wrapper<double>`'s converting-constructor body, `value_(other.value_)`, tries to access `other.value_` where `other` is a `const Wrapper<int>&`, and `value_` is `private` in `Wrapper<int>`. Being "the same class template" does not grant one instantiation automatic friend-like access to another instantiation's private members — each instantiation is checked as its own independent class for access-control purposes. Minimal idiomatic fix: declare every instantiation of `Wrapper` a friend of every other instantiation, via a templated friend declaration inside the class:
```cpp
template<typename T>
class Wrapper {
public:
    Wrapper(T v) : value_(std::move(v)) {}
    template<typename U>
    Wrapper(const Wrapper<U>& other) : value_(other.value_) {}
private:
    template<typename U> friend class Wrapper;   // grants cross-instantiation access
    T value_;
};
```

---

**05-P37.** [DEBUG]

**Reference Solution:** The `Numeric` concept only restricts `T`'s *type category* (arithmetic vs. not) — it says nothing about the *runtime value* of `b`, and division by zero (`10 / 0` for integral `T`) is a runtime-value problem entirely outside any concept's reach, since concepts are checked at compile time against types, not against the specific values a function happens to be called with at some particular call site. No concept constrained purely on `T` could catch this — the fix, if pursued, would need to be a runtime check inside `divide` itself (e.g., throwing or returning an error result when `b == 0`), which is a different kind of validation entirely (a value precondition, not a type constraint) and belongs to Ch06's error-handling territory rather than anything expressible in the type system via `T` alone.

---

**05-P38.** [DEBUG]

**Reference Solution:** There is no base-case overload to stop the recursion — `print_pack(rest...)` for a pack that has shrunk to zero remaining arguments calls `print_pack()` with zero arguments, but the only declared `print_pack` requires at least one parameter (`T first`), so there's no matching overload for the empty-pack call, and the recursion (as instantiated down to that point) fails to compile with "no matching function" at the base of the recursion. Minimal fix: add a zero-argument overload to terminate the recursion:
```cpp
void print_pack() {}   // base case — terminates the recursion

template<typename T, typename... Rest>
void print_pack(T first, Rest... rest) {
    std::cout << first << ' ';
    print_pack(rest...);
}
```

---

### Level 5 — Integration

**05-P39.**

**Reference Solution:**
```cpp
template<typename T>
class Optional {
    alignas(T) unsigned char storage_[sizeof(T)];
    bool has_value_ = false;

    T* ptr() { return reinterpret_cast<T*>(storage_); }
    const T* ptr() const { return reinterpret_cast<const T*>(storage_); }
public:
    Optional() = default;

    template<typename U> requires std::convertible_to<U, T>
    Optional(U&& v) : has_value_(true) { ::new (storage_) T(std::forward<U>(v)); }

    ~Optional() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            if (has_value_) ptr()->~T();
        }
    }

    bool has_value() const { return has_value_; }
    T& value() { if (!has_value_) throw std::runtime_error("empty Optional"); return *ptr(); }
    T value_or(T default_value) const { return has_value_ ? *ptr() : std::move(default_value); }
};
```

**Explanation:** The converting constructor is constrained with `std::convertible_to<U, T>` (a concept) rather than an `enable_if_t` predicate. The destructor uses `if constexpr` on `std::is_trivially_destructible_v<T>` to skip the explicit `~T()` call entirely for trivially-destructible `T` — for such `T`, no destructor call is instantiated at all, which also means the branch never needs `ptr()->~T()` to even be well-formed for such types (though in this case it would be regardless; the more important point is skipping unnecessary work). Genuine scope cut: no `Optional<T&>` reference specialization and no monadic `.and_then`/`.transform` — both are reasonable omissions for a hand-rolled exercise version, since they're additive features on top of the same core storage/lifetime mechanics already demonstrated here.

**C++ Considerations:** Manual placement-new/explicit-destructor-call storage management (rather than a plain `T value_;` member) is what allows `Optional<T>` to represent "no value" without requiring `T` to be default-constructible.

---

**05-P40.**

**Reference Solution:**
```cpp
struct Meters {}; struct Seconds {}; struct MetersPerSecond {};

template<typename Tag>
class Quantity {
    double value_;
public:
    explicit Quantity(double v) : value_(v) {}
    double value() const { return value_; }
    Quantity operator+(const Quantity& o) const { return Quantity(value_ + o.value_); }
    Quantity operator-(const Quantity& o) const { return Quantity(value_ - o.value_); }
};

Quantity<MetersPerSecond> operator/(const Quantity<Meters>& m, const Quantity<Seconds>& s) {
    return Quantity<MetersPerSecond>(m.value() / s.value());
}
```
`Quantity<Meters>{5} + Quantity<Seconds>{5}` fails to compile — `operator+` is only defined between two `Quantity<Tag>`s of the *same* `Tag`, so `Quantity<Meters>::operator+` doesn't even accept a `Quantity<Seconds>` argument; the mismatch is caught entirely at compile time, as a plain overload-resolution/type-mismatch error.

**Explanation:** `Tag` (e.g. `Meters`, `Seconds`) is an empty struct that exists purely to make `Quantity<Meters>` and `Quantity<Seconds>` distinct *types* — it contributes zero bytes to `Quantity`'s actual layout (no member of type `Tag` is ever stored) and generates no runtime code of its own; the entire unit-safety guarantee is enforced by the type system at compile time, with the compiled code operating on nothing but the underlying `double`, identical to what an untagged, unit-unsafe version would generate.

---

**05-P41.**

**Reference Solution:**
```cpp
struct SingleThreaded {
    template<typename T> static void increment(T& v) { ++v; }
    template<typename T> static void decrement(T& v) { --v; }
};
struct ThreadSafe {
    template<typename T> static void increment(T& v) { std::atomic_ref<T>(v).fetch_add(1); }
    template<typename T> static void decrement(T& v) { std::atomic_ref<T>(v).fetch_sub(1); }
};

template<typename T, typename ThreadingPolicy>
class Counter {
    T value_{};
public:
    void increment() { ThreadingPolicy::increment(value_); }
    void decrement() { ThreadingPolicy::decrement(value_); }
    T get() const { return value_; }
};

Counter<int, SingleThreaded> c1;   // fast, not thread-safe
Counter<int, ThreadSafe>     c2;   // thread-safe, pays synchronization cost
```

**Explanation:** Switching from `Counter<int, SingleThreaded>` to `Counter<int, ThreadSafe>` is a one-line template-argument change; `Counter`'s own `increment`/`decrement` bodies never change. `ThreadSafe` pays for an atomic read-modify-write operation (which, depending on the target architecture, involves a hardware-level locked instruction or equivalent memory-barrier machinery) on every call — strictly more expensive than `SingleThreaded`'s plain, unsynchronized `++`/`--` — in exchange for the guarantee that concurrent increments/decrements from multiple threads don't race (a guarantee `SingleThreaded` doesn't provide at all).

---

**05-P42.**

**Reference Solution:**
```cpp
struct Circle { double r; double area() const { return 3.14159 * r * r; } double perimeter() const { return 2 * 3.14159 * r; } };
struct Square { double s; double area() const { return s * s; } double perimeter() const { return 4 * s; } };

class Shape {
    struct ShapeConcept {
        virtual ~ShapeConcept() = default;
        virtual double area() const = 0;
        virtual double perimeter() const = 0;
    };
    template<typename T>
    struct ShapeModel : ShapeConcept {
        T value;
        explicit ShapeModel(T v) : value(std::move(v)) {}
        double area() const override { return value.area(); }
        double perimeter() const override { return value.perimeter(); }
    };
    std::unique_ptr<ShapeConcept> impl_;
public:
    template<typename T>
    Shape(T v) : impl_(std::make_unique<ShapeModel<T>>(std::move(v))) {}
    double area() const { return impl_->area(); }
    double perimeter() const { return impl_->perimeter(); }
};

double total_area(const std::vector<Shape>& shapes) {
    double sum = 0;
    for (const auto& s : shapes) sum += s.area();
    return sum;
}
```

**Explanation:** Uses the same concept/model structure from 05-P22, generalized to two required methods instead of one. Against a traditional virtual `ShapeBase` with `Circle`/`Square` inheriting from it: the concrete advantage of type erasure here is that `Circle`/`Square` (or, notably, a third-party library's own shape-like type that the codebase doesn't own and can't retroactively make inherit from anything) never need to know `Shape`/`ShapeConcept` exist at all — any type that merely *happens* to provide `.area()`/`.perimeter()` can be wrapped, with zero coupling between the concrete type's definition and the erasure wrapper's existence, whereas the virtual-base approach requires every participating type to be written (or retrofitted) specifically to inherit from `ShapeBase`.

---

**05-P43.**

**Reference Solution:**
```cpp
template<typename R>
concept RangeLike = requires(R r) { r.begin(); r.end(); };

template<RangeLike Range>
auto to_vector(Range&& r) {
    using Elem = std::remove_cvref_t<decltype(*std::begin(r))>;
    std::vector<Elem> out;
    for (auto&& x : r) out.push_back(std::forward<decltype(x)>(x));
    return out;
}

std::vector<int> data{1,2,3,4,5,6};
auto v = to_vector(data | std::views::filter([](int x){ return x % 2 == 0; }));
```

**Explanation:** The `RangeLike` concept is the compile-time constraint (checked via `requires`) ensuring `to_vector` rejects, at the template-instantiation/constraint-check step, any `Range` lacking `begin()`/`end()` — producing a direct "constraint not satisfied" diagnostic rather than a runtime check (there is no runtime check here at all; a type with no `begin()`/`end()` simply never successfully calls `to_vector` in the first place) or a confusing deep-template error from inside the loop body where `std::begin(r)` would otherwise fail deep in an unconstrained instantiation. Ties directly to Ch04: `std::views::filter` is lazy, and `to_vector` is exactly the eager materialization step Ch04 discusses as necessary once a caller needs an owned, independent, randomly-indexable result rather than a view still tied to `data`'s lifetime.

---

**05-P44.**

**Reference Solution:**
```cpp
template<typename... Handlers>
auto visit_typed(const std::variant<struct A, struct B, struct C>& v, Handlers... handlers) {
    static_assert(sizeof...(Handlers) == 3,
        "visit_typed: exactly 3 handlers required (one per A/B/C alternative) — you passed a different count");
    struct Overload : Handlers... { using Handlers::operator()...; };
    return std::visit(Overload{handlers...}, v);
}
```

**Explanation:** The custom `static_assert` with a named, specific message ("exactly 3 handlers required... you passed a different count") fires *before* overload resolution is even attempted, immediately naming the actual mistake ("wrong handler count") — whereas relying solely on `std::visit`'s own exhaustiveness enforcement (05-P25-family territory / Ch04) would, for a *missing* handler specifically, produce a generic "no viable `operator()` for alternative N" error deep inside `std::visit`'s internals, which is correct but requires the reader to infer "oh, I must be missing a handler" rather than being told so directly. The additional value is purely diagnostic quality, not new correctness — both approaches do eventually fail to compile for a missing handler.

---

**05-P45.**

**Reference Solution:**
```cpp
template<typename T> requires std::move_constructible<T>
class LazyValue {
    std::function<T()> compute_;
    mutable std::optional<T> cached_;
public:
    explicit LazyValue(std::function<T()> f) : compute_(std::move(f)) {}
    const T& get() const {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (!cached_) cached_ = compute_();   // cheap path: plain copy-assign is fine
        } else {
            if (!cached_) cached_.emplace(compute_());  // avoid an extra copy for expensive/move-only-leaning T
        }
        return *cached_;
    }
};
```

**Explanation:** Requiring only move-constructible (not copy-constructible) is the right constraint if move-only `T` (e.g. `std::unique_ptr<X>`) should be supported at all — a copy-constructible requirement would reject every move-only `T` outright, even though nothing about "compute once, cache, return by reference" fundamentally needs copies. The `if constexpr` branch on `is_trivially_copyable_v<T>` skips the extra move/in-place-construction bookkeeping `.emplace(...)` provides (safe to skip specifically because a trivially-copyable type has no move/copy semantics worth optimizing around — a plain copy-assignment is already maximally cheap for such types, typically just a `memcpy`-equivalent).

---

### Level 6 — Production

**05-P46.**

**Reference Solution:**
```cpp
template<typename T>
concept ConfigParsable = requires(std::string_view raw, T& out) {
    { parse_config_value_impl(raw, out) } -> std::same_as<bool>;
};

template<ConfigParsable T>
Result<T> parse_config_value(std::string_view raw) {
    T out{};
    if (!parse_config_value_impl(raw, out)) return Result<T>::error("parse failure");
    return Result<T>::ok(std::move(out));
}
```
The library provides `parse_config_value_impl` overloads for its own built-in types (`int`, `double`, `bool`, `std::string`); a consuming application's enum type satisfies `ConfigParsable` simply by defining its own free function `bool parse_config_value_impl(std::string_view, MyEnum&)` in its own namespace, found via ADL when `parse_config_value<MyEnum>` is instantiated.

**Explanation:** The constraint requires each `T` to supply a **customization point** — a free function discoverable via ADL — rather than requiring `T` to belong to an enumerated closed set the library hardcodes (e.g., a `switch` over a fixed list of known types). The customization-point design is correct here specifically because the library's consumers (who the library doesn't control, and whose enum types don't exist yet at the time the library is written) need a way to extend `parse_config_value` to their own types *without modifying the library itself* — an enumerated closed set would require the library to be updated (and re-released) every time a new consumer needs a new `T`, which doesn't scale for a shared library with independent consuming teams.

---

**05-P47.**

**Reference Solution:** Mitigation 1 — **type-erase the policies at the `Cache`'s public boundary**: keep the fully-templated `Cache<K,V,EvictionPolicy,HashPolicy,LockPolicy>` available for call sites that genuinely need compile-time specialization, but additionally provide a non-templated (or minimally-templated, only on `K`/`V`) `ErasedCache<K,V>` facade that holds a type-erased policy set internally and is what most call sites actually use — collapsing "N call sites use the same 2 policy combos" down to just 1-2 concretely instantiated `Cache` specializations behind the facade, plus however many `ErasedCache<K,V>` instantiations are needed (typically far fewer, since `K`/`V` vary more than the policy combination does in practice). Tradeoff: the facade's calls pay type-erasure's usual cost (one indirection, and typically the erased inner `Cache` needs to live on the heap) for call sites that go through it. Mitigation 2 — **provide a small number of named, pre-instantiated policy-combination aliases** (e.g. `using DefaultCache = Cache<K,V,LRU,StdHash,Mutex>;`) covering the handful of combinations actually used in practice, and steer call sites toward those aliases via code-review convention rather than letting every call site independently spell out its own four policy arguments — this doesn't reduce the *number* of instantiations directly, but it makes accidental near-duplicate instantiations (two call sites that meant to use the same combination but each spelled it out slightly differently) far less likely. Both connect back to the same root cause — this is fundamentally an instantiation-count problem — addressed more directly via explicit instantiation and `extern template` in Ch08/Ch10, which this problem doesn't need in full to answer, per the given framing.

---

**05-P48.**

**Reference Solution:**
```cpp
// Matrix.h
template<typename T> class Matrix { /* ... */ };
extern template class Matrix<float>;
extern template class Matrix<double>;

// Matrix.cpp
template class Matrix<float>;   // explicit instantiation definition
template class Matrix<double>;
```

**Explanation:** `extern template class Matrix<float>;` promises every other translation unit that includes `Matrix.h` that an instantiation of `Matrix<float>` already exists elsewhere (compiled once, in `Matrix.cpp`) — telling the compiler "don't instantiate this class here," which suppresses that TU's own from-scratch re-instantiation and re-compilation of the entire class, instead just emitting a reference resolved at link time to the one definition compiled in `Matrix.cpp`. With no such declaration, every TU that uses `Matrix<float>` independently instantiates and compiles the full class itself (and the linker typically discards the resulting duplicate definitions, but the compile-time cost of generating them in the first place is paid redundantly in every TU). The one concrete limitation: if a caller elsewhere in the codebase later needs `Matrix<int>` — a type never explicitly instantiated in `Matrix.cpp` — that caller's TU silently falls back to instantiating `Matrix<int>` itself from the header (the `extern template` declarations only apply to the specific types they name), which works correctly but means `Matrix<int>` gets none of the shared-instantiation benefit `float`/`double` get, and a maintainer might not immediately notice that a "hot type" list in `Matrix.cpp` has quietly gone stale relative to what the codebase now actually uses.

---

### Level 7 — Principal Reasoning

**05-P49.**

**Reference Solution:** **Missing requirements to clarify before committing to a design:** (1) Does a plugin need to be reloaded/updated *without restarting the host process*, and if so, how often — this determines whether the boundary needs to support unloading a stale version safely, which is a much harder problem than "load once at startup." (2) Is ABI stability across host/plugin *compiler* versions and standard-library implementations required (e.g., can a plugin built with a newer MSVC than the host, or built with a different STL implementation entirely, be loaded), or is "same compiler, same STL, same build" an acceptable constraint to impose on plugin authors — this determines whether the boundary can use anything beyond a C-compatible ABI. (3) What's the expected plugin count and how performance-sensitive is the generic-algorithm dispatch across the boundary — one indirection per operation is likely fine for a handful of plugins processing moderate data, but might matter for, say, thousands of small per-plugin calls in a tight loop. (4) Do plugin types need to support serialization/persistence across host restarts, or only in-memory use during a single session — this affects whether the erased interface needs a stable, versioned wire format in addition to the in-memory operations.

**Tentative design** (assuming reasonable answers — infrequent reloads, no cross-compiler ABI guarantee beyond a documented minimum C++ standard and calling convention, moderate plugin count, no cross-session serialization requirement): draw the templates-vs-type-erasure line exactly at the plugin boundary itself. Inside the host, and inside each plugin's own compiled code, ordinary templates and generic algorithms are used freely (compile-time genericity, full optimization, no restriction) — but the boundary that the host and each plugin actually cross is a small, deliberately non-templated, ABI-stable interface (a C-linkage vtable-like struct of function pointers, or an equivalent stable-layout C++ interface avoiding STL types in signatures), analogous to 05-P22/05-P42's type-erasure "concept" pattern but applied across a dynamically-loaded-library boundary rather than within one binary. The host's generic sort/filter/serialize algorithms then operate on *that* stable interface, not directly on each plugin author's concrete type.

**Why "let plugin authors write templates that get compiled into the host" is the wrong answer regardless:** this fundamentally requires the host's generic algorithm and the plugin author's type definition to be compiled together, from shared source, in the same translation — but "separately compiled, dynamically loaded" (the entire premise of a plugin boundary, per Ch08/Ch09's territory) means the plugin's code and the host's code are, by construction, compiled independently and only linked at load time. A template instantiation is not a runtime-linkable entity in the way a function with a stable calling convention is — there is no ABI-portable way to hand a not-yet-instantiated template across a dynamic-load boundary and have the *host's* already-compiled algorithm instantiate against it, because instantiation is a compile-time act requiring the template's full definition to be visible to the compiler doing the instantiating. This isn't a matter of picking a cleverer template trick; it's a structural mismatch between what templates require (shared, compile-time-visible source) and what a plugin boundary provides (opaque, separately-compiled, runtime-linked code) — hence the design must cross that specific boundary via a runtime-polymorphic, ABI-stable interface (type erasure or an equivalent), no matter how the other open questions resolve.

---

**05-P50.**

**Reference Solution:** **Two structurally different resolutions:**

**(A) Split the API** into an ordered variant (keeps `std::totally_ordered`, keeps the sorting-dependent algorithm as-is) and a new, separate unordered variant (requires only equality, and either omits the sorting-dependent algorithm entirely from that variant's surface, or offers a differently-named operation with different complexity/behavior guarantees that doesn't need ordering). This preserves the existing ordered API's contract completely — no existing consumer sees any behavior change — at the cost of maintaining two API surfaces going forward, with the attendant risk of the two drifting inconsistently over time (a bug fixed in one variant's shared logic might be missed in the other if the implementations aren't factored to share code).

**(B) Keep one API, weaken the concept, and make the sorting-dependent behavior conditionally available** via `if constexpr` (or a second, more-constrained overload only viable for `std::totally_ordered` types) inside the single templated implementation — types satisfying only equality get a version of the algorithm that either omits the sorting-dependent step or substitutes a weaker (but universally applicable) alternative, while types satisfying the full `std::totally_ordered` concept still get the original sorting-based behavior. This avoids maintaining two separately-named APIs, but means the *same* function name now has behavior that silently varies by what its `T` happens to satisfy — a consumer reading only the function's name, without checking which capabilities their specific `T` provides, could be surprised by which code path they actually get.

**Recommended approach:** (A), the split — **maintenance burden traded away:** committing to keep two API surfaces in sync indefinitely, and the discoverability cost of consumers needing to know which one applies to their type, in exchange for never silently changing behavior for the existing ordered consumers and never letting one function's documented contract quietly depend on which concept a given `T` happens to satisfy. **Question still needed from the requesting team before finalizing:** does their unordered use case actually need the *same* algorithm's result (in which case an equality-only algorithm needs to be designed and proven correct on its own terms, since it fundamentally cannot rely on sorting to break ties or establish an iteration order), or would a different, explicitly-unordered operation (e.g., "does this collection contain a match" rather than whatever ordering-dependent operation the original API provides) actually satisfy their real need — the answer changes whether (A) requires designing new algorithmic logic at all, or just re-exposing existing equality-only logic under a new name.

**05-P51.**

**Reference Solution:** `f(ns::Widget{})` prints `"ns::g\n"`; `f(42)` prints `"::g(int)\n"`. Inside `f`, the call `g(t)` is a **dependent** call (its argument depends on the template parameter `T`), so it is resolved at instantiation time using ordinary lookup from `f`'s definition context *plus* ADL from `t`'s type's namespace. For `T = ns::Widget`, ADL adds `ns::g` (found because `ns::Widget` lives in `ns`) to the candidate set, and it's selected as the only viable match. For `T = int`, `int` has no associated namespace to search via ADL, so ordinary lookup alone finds the global `::g(int)`.

**Explanation:** No qualification or `using ns::g;` appears anywhere — `ns::g` is found purely because ADL searches the namespaces of the argument's type, not because it was made visible lexically. This is the same mechanism the "Argument-Dependent Lookup (ADL) and Hidden Friends" section's `log(w)` example relies on.

---

**05-P52.**

**Reference Solution:** Fails to compile — `int` has no nested `value_type` member, so `typename T::value_type` cannot resolve for `Container<int>`. `typename T::value_type` asks the compiler to treat the dependent name `T::value_type` as a **type** (rather than the default assumption of a value) once `T` is known; whether that lookup actually succeeds depends entirely on what `T` turns out to be, which is exactly why the failure surfaces at instantiation time (phase two), not when `Container` itself was written and phase-one-checked with no concrete `T` yet in hand.

**Explanation:** `Container<int>` failing while, say, `Container<std::vector<int>>` (which does have `value_type`) would succeed is the practical consequence of two-phase lookup: the template's *definition* is accepted regardless, because at definition time the compiler can't yet know whether any particular `T::value_type` will resolve — only instantiation with a concrete `T` can answer that.

---

**05-P53.**

**Reference Solution:** `w1` deduces `Widget<int>`, calling the `Widget(T v)` constructor (a single non-list-initialization-looking argument prefers the ordinary constructor over the `initializer_list` one when there's only one element and no braces-vs-parens ambiguity forcing list-init preference... but here `w1{5}` *does* use braces). More precisely: braced-init with a single element still prefers a matching `std::initializer_list` constructor if the element type matches — so `w1` actually selects `Widget(std::initializer_list<T>)` with `T` deduced as `int`, giving `Widget<int>` via the list constructor, not the single-value constructor. `w2{1, 2, 3}` similarly selects the `initializer_list` constructor, also deducing `Widget<int>`. Both deduce `Widget<int>`, but via the *same* (list) constructor, not two different ones — the ambiguity risk this problem is pointing at is real for CTAD in general (a single-braced-element call silently preferring `initializer_list` over a same-typed single-argument constructor) even though in this specific case both paths happen to agree on `T = int`.

**Explanation:** This is exactly the class of surprise `std::vector` faces: `std::vector v{5}` deduces `vector<int>` with one element (5), *not* a vector of size 5, precisely because braced-init prefers the `initializer_list` constructor whenever one is viable — a mismatch between "looks like it might mean a count" and "actually means a list of elements" that CTAD does not resolve on the library's behalf; it's why `std::vector<int> v(5)` (parens, not braces) is the idiom for "5 default-constructed elements."

---

**05-P54.** [DEBUG]

**Reference Solution:** As written, `Range` deduces `Range<std::vector<int>::iterator>` — CTAD deduces directly from the constructor's parameter type (`Iter`), which here *is* the iterator type itself, not the element type the caller actually wants. This is not a CTAD bug: CTAD is doing exactly what constructor-based deduction is defined to do (deduce `Iter` from the arguments bound to `Iter first, Iter last`); the mismatch is that the class's *intended* template parameter (an element type) and its *constructor's* parameter (an iterator type) are different things, and nothing about CTAD's ordinary rules can infer that intent on its own. Fix, a deduction guide:
```cpp
template<typename Iter>
Range(Iter, Iter) -> Range<typename std::iterator_traits<Iter>::value_type>;
```
This explicitly overrides the constructor-based deduction, telling the compiler to deduce the iterator's `value_type` (`int`, here) as `Range`'s template argument instead of the iterator type itself.

**Explanation:** This mirrors the "Class Template Argument Deduction (CTAD) and Deduction Guides" crash-course section's `Span` example precisely — an iterator-pair constructor is exactly the recurring pattern deduction guides exist to correct, and it's also the same pattern the standard library itself supplies guides for (e.g. `std::vector`'s iterator-pair constructor).

---

**05-P55.** [DEBUG]

**Reference Solution:** Both `T::child_type*` occurrences need `typename` before them — `typename T::child_type* first_child();` and `typename T::child_type* child_ = nullptr;`. Without `typename`, the compiler's default assumption for a dependent name like `T::child_type` is that it names a **value**, not a type — so `T::child_type* first_child()` parses as an (invalid) multiplication expression between the "value" `T::child_type` and `first_child`, rather than as a pointer-to-`child_type` return type, which is why it fails to compile for every `T` regardless of whether `T::child_type` genuinely exists as a nested type.

**Explanation:** This is a direct instance of the "Two-Phase Name Lookup and Dependent Names" section's `typename T::value_type` rule — the fix is purely syntactic (adding the disambiguating keyword), not a change to what the code is trying to express, and the fact that it fails identically for *every* `T` (including ones with a perfectly valid `child_type`) is itself the tell that the problem is the missing keyword, not the actual types involved.

---

**05-P56.**

**Reference Solution:** **Missing requirements to clarify:** (1) Are the third-party types (`BigInt`, `Rational`) guaranteed to live in their own distinct namespaces reachable by ADL, or could a consumer's own numeric type sit in the global namespace (or share a namespace with unrelated overloads), risking either an ADL failure or an ambiguous overload set once the library's own operators are added to the mix? (2) For types whose author never anticipated this library's existence and provides no customization point at all (no `to_string`-equivalent, no ADL-findable free function), is silent exclusion acceptable, or does the library need a documented, explicit opt-in mechanism (e.g., a trait specialization) as a fallback path?

**Proposed design:** Prefer an ADL-found free function as the *primary* customization point (matching the pattern established for `parse_config_value`'s `parse_config_value_impl`), since it lets third-party authors who *do* control their type's namespace opt in with zero dependency on the library's headers. But pair it with a **trait-class fallback**: a `template<typename T> struct NumericFormat { static std::string apply(const T&) = delete; };` that the library checks (via a concept combining "has an ADL-findable free function" OR "has a specialized `NumericFormat`") before falling back to a hard compile error — this gives a second, explicit path for a type whose author can't or won't add an ADL-findable free function (e.g., a type from a header the consumer can't modify), without requiring the library to enumerate types in advance.

**Why pure ADL with no fallback is risky:** ADL only works if the type's own namespace is where the free function lives, and it silently does nothing (falls through to no match, then a hard error only at the point of use) for consumers who have no ability to add a same-namespace overload at all — e.g. a `BigInt` from a vendored, unmodifiable third-party header. A trait-class fallback lets such a consumer specialize `NumericFormat<TheirType>` from *their own* code without ever touching the third-party header, closing the exact gap ADL-only leaves open; the actual answer, though, still depends on the two open questions above (namespace guarantees, and whether silent exclusion is tolerable) before finalizing which of the two paths should be primary.

## Integration Challenge Solution — 05-IC1

**Reference Solution:**
```cpp
// Hand-rolled container with independent storage + its own Forward iterator
template<typename T>
class FixedBag {
    std::unique_ptr<T[]> data_;
    size_t size_;
public:
    FixedBag(std::initializer_list<T> init) : data_(std::make_unique<T[]>(init.size())), size_(init.size()) {
        std::copy(init.begin(), init.end(), data_.get());
    }
    class iterator {
        T* p_;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T; using difference_type = std::ptrdiff_t;
        using pointer = T*; using reference = T&;
        explicit iterator(T* p) : p_(p) {}
        T& operator*() const { return *p_; }
        iterator& operator++() { ++p_; return *this; }
        bool operator==(const iterator& o) const { return p_ == o.p_; }
    };
    iterator begin() { return iterator(data_.get()); }
    iterator end()   { return iterator(data_.get() + size_); }
    size_t size() const { return size_; }
};

// C++20 concept — states exactly what median needs
template<typename C>
concept MedianCompatible = requires(C c) {
    c.begin(); c.end(); c.size();
    { *c.begin() } -> std::totally_ordered;
} && std::forward_iterator<decltype(std::declval<C&>().begin())>;

template<MedianCompatible C>
auto median(C& c) {
    using T = std::remove_cvref_t<decltype(*c.begin())>;
    std::vector<T> copy(c.begin(), c.end());   // sort a copy, not in place
    std::sort(copy.begin(), copy.end());
    size_t n = copy.size();
    return n % 2 == 1 ? copy[n/2] : (copy[n/2 - 1] + copy[n/2]) / 2;
}

// SFINAE-era equivalent
template<typename C, typename = std::enable_if_t<
    std::is_same_v<decltype(std::declval<C&>().begin()), decltype(std::declval<C&>().begin())> &&
    std::is_base_of_v<std::forward_iterator_tag,
        typename std::iterator_traits<decltype(std::declval<C&>().begin())>::iterator_category>>>
auto median_sfinae(C& c) { /* identical body to median() above */ }
```

**Explanation:**

**Step 2 (concept):** `median` needs exactly: `begin()`/`end()` (to obtain elements at all), a `size()` (to index the middle element(s) directly rather than re-deriving the count via a full traversal — a Forward iterator alone doesn't provide constant-time size), a Forward iterator specifically (needed because the implementation copies via range-construction and re-iterates the copy — Input-only iterators, which are single-pass, would be consumed by the copy and couldn't be traversed again if the design ever needed a second pass over the *original*, though this particular implementation only needs one pass over the original since it copies-then-sorts), and `std::totally_ordered` on the element type (needed for the internal `sort`, which requires a strict weak ordering).

**Step 3 (diagnostic comparison):** For a type with `begin()`/`end()` but only an *Input* iterator (not Forward) — the concept-based `median` fails at the constraint-check step with a direct "constraint `MedianCompatible` not satisfied: `std::forward_iterator<...>` not satisfied" message naming the specific missing capability. The SFINAE-based `median_sfinae` instead fails because the `enable_if_t`'s condition evaluates to `false`, removing the only overload from the candidate set — the caller sees a generic "no matching function `median_sfinae`" error with, at best, a compiler-specific note about the failed `enable_if_t` substitution, giving no direct indication that the *iterator category* specifically (as opposed to, say, a missing `begin()`/`end()`, or a non-ordered element type) is what's insufficient; a reader has to manually inspect the `enable_if_t` condition's internals to find out which sub-check failed.

**Step 4 (copy vs. in-place, and iterator category):** `median` must sort a **copy**, not the caller's original container in place — the caller's container is data the algorithm is only asked to *summarize*, not data it's entitled to reorder as an observable side effect; a caller passing their own meaningfully-ordered `std::vector` (e.g., insertion-ordered log entries) would be surprised and likely broken by `median` silently re-sorting their data as a side effect of merely asking for a statistic. This copy requirement is exactly *why* the concept demands a Forward iterator (to support the range-constructor's traversal building the copy) rather than something weaker — but notably, it does **not** need to demand more than Forward (e.g., Random Access) from the *original* container, since the original is only ever traversed once, linearly, to build the copy; all of the random-access-requiring work (`std::sort`, direct-indexing the middle element) happens entirely on the `std::vector<T> copy`, which is always Random Access by construction regardless of what iterator category the caller's original container provides.
