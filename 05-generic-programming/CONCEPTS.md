# Chapter 05 — Generic Programming: Templates, Concepts, and Compile-Time Design

> Prerequisites: [Chapter 01](../01-core-semantics/CONCEPTS.md), [Chapter 03](../03-value-categories/CONCEPTS.md) (forwarding is mandatory here), [Chapter 04](../04-stl/CONCEPTS.md) (iterator-concept fluency).
> This chapter is about writing code correct for a *set* of types, and constraining that set legibly — not template metaprogramming as a sport. It is the first chapter with genuine Level 7 problems: incomplete-requirements design reasoning, not just harder implementation.

## Crash Course

### Function and Class Templates — The Basic Mechanism

A template is a blueprint the compiler instantiates per concrete type used. Nothing is compiled for a template until it's instantiated — errors inside an uninstantiated template body can go undetected until someone actually calls it with a specific type (unlike concepts-constrained code, covered below, which can reject bad types before ever reaching the body).

```cpp
template<typename T>
T max_of(T a, T b) { return a > b ? a : b; }

template<typename T>
class Box { T value; public: explicit Box(T v) : value(std::move(v)) {} };
```

Template *argument deduction* infers `T` from call-site arguments when possible; when it can't (or you want to be explicit), you supply it: `max_of<double>(1, 2.5)`.

### Specialization and Partial Specialization

A **full specialization** provides an entirely separate implementation for one specific set of template arguments. A **partial specialization** (class templates only — function templates cannot be partially specialized, only overloaded) matches a *pattern* of arguments rather than one exact set.

```cpp
template<typename T> struct TypeName { static constexpr const char* value = "unknown"; };
template<> struct TypeName<int> { static constexpr const char* value = "int"; };          // full

template<typename T> struct TypeName<T*> { static constexpr const char* value = "ptr"; }; // partial
```

The compiler picks the *most specialized* match that fits; ambiguity between two equally-specific partial specializations is a compile error, not a silent tie-break.

### Variadic Templates, Parameter Packs, and Fold Expressions

A **parameter pack** holds zero or more template arguments (or function arguments) as a single named entity, expanded with `...`.

```cpp
template<typename... Ts>
void print_all(Ts&&... args) {
    ((std::cout << args << ' '), ...);   // fold expression, C++17
}
```

A **fold expression** (`(pack op ...)` or `(... op pack)`) expands a binary operator across every element of a pack without hand-written recursion. Before C++17, the same effect required recursive variadic templates with a base-case overload — fold expressions replace that entire pattern for the common case of "apply this operator across all of them."

### Type Traits and SFINAE

**Type traits** (`std::is_integral<T>`, `std::remove_reference<T>`, `std::conditional<...>`, etc.) are compile-time predicates/transformations on types, mostly living in `<type_traits>`. They are the vocabulary SFINAE and, later, concepts are built from.

**SFINAE** ("Substitution Failure Is Not An Error") is the rule that when substituting a deduced/specified template argument into a function template's signature produces an invalid construct, that overload is silently removed from the candidate set — rather than being a hard compile error — as long as the failure occurs in the *immediate context* of the substitution (not deep inside the function body).

```cpp
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T> half(T x) { return x / 2; }
```

This is the pre-C++20 idiom for constraining templates. It works, but the error messages when no overload matches are notoriously unreadable (a wall of substitution failures rather than a clear "T doesn't satisfy X"), and the constraint is smuggled into the return type rather than stated directly — both problems concepts were designed to fix.

### Two-Phase Name Lookup and Dependent Names

A template's body is checked in **two phases**. Phase one happens at template *definition* time and resolves every name that doesn't depend on a template parameter, using ordinary (non-ADL) lookup at that point in the source. Phase two happens at *instantiation* time and resolves everything that does depend on a template parameter — these are called **dependent names** — using the set of overloads visible at the instantiation point plus ADL (below) from the argument types' namespaces.

```cpp
template<typename T>
void f(T t) {
    g(t);          // dependent on T -> resolved at instantiation (phase 2), ADL included
    undeclared();  // NOT dependent on T -> must be visible at definition point (phase 1) or it's an error, period
}
```

This split is *why* a template can compile cleanly and still fail (or silently call the wrong overload) depending on what's visible at each call site — a non-dependent name typo'd or simply not yet declared is a hard error immediately at definition, with no chance for a later, better-matching overload to rescue it, no matter what types the template is eventually instantiated with.

A name is dependent if its meaning could plausibly change depending on the template argument — most commonly, anything of the form `T::something`. The compiler cannot know, at phase one, whether `T::something` names a type, a value, or a template, because that depends entirely on what `T` turns out to be. By default, the compiler's fallback assumption for `T::something` is "this is a value" — which is why two disambiguating keywords exist:

```cpp
template<typename T>
void f() {
    typename T::value_type v{};      // "T::value_type is a TYPE" — without `typename`, this is a compile error
    T::template rebind<int>::type x; // "T::rebind is a TEMPLATE" — without `template`, `<` parses as less-than
}
```

`typename` disambiguates a dependent type name; `template` (before a dependent name used as a template) disambiguates a dependent template name. Omitting either when the compiler needs it is a compile error, not a silent misinterpretation — but the error message ("expected expression", "'<' is not a less-than operator here") rarely names the actual missing keyword, which is why this is a common source of "this compiles for a concrete type but not for the generic version" confusion.

### Template Template Parameters and Tag Dispatch

A **template template parameter** is a template parameter that itself names a template, not a type — used when a generic component needs to be parameterized by *what container/wrapper template* to use, not by one already-fixed type.

```cpp
template<template<typename> class Container, typename T>
class Wrapper {
    Container<T> data;
};
Wrapper<std::vector, int> w;   // Container = std::vector (the template itself, not std::vector<int>)
```

**Tag dispatch** is the pre-concepts idiom for selecting between implementations based on a type's category, by overloading on an empty "tag" type derived from a trait, rather than branching on the trait's boolean value inside one function body.

```cpp
template<typename Iter>
void advance_impl(Iter& it, int n, std::random_access_iterator_tag) { it += n; }        // O(1)
template<typename Iter>
void advance_impl(Iter& it, int n, std::input_iterator_tag) { while (n--) ++it; }        // O(n)

template<typename Iter>
void my_advance(Iter& it, int n) {
    advance_impl(it, n, typename std::iterator_traits<Iter>::iterator_category{});
}
```

Overload resolution — not a runtime or even an `if constexpr` branch — picks the right `advance_impl` based on the tag type's inheritance (`random_access_iterator_tag` derives from `input_iterator_tag`, so a random-access iterator can still match a less-specific overload if no exact one exists). `if constexpr` has replaced tag dispatch for most new code, but the idiom remains common in existing codebases and in exactly this kind of iterator-category-driven dispatch, which the standard library itself still uses internally.

### Argument-Dependent Lookup (ADL) and Hidden Friends

**Argument-Dependent Lookup** extends unqualified name lookup for a function call to also search the namespaces of the call's *argument types* — not just the namespaces visible lexically at the call site. This is what makes `std::swap(a, b)` for a user-defined type in namespace `ns` actually find `ns::swap` (a better-matching overload the user wrote) via plain `swap(a, b)` (no `std::` qualification, no explicit `using`), and it is exactly the mechanism a dependent-name call like `g(t)` in the previous section relies on at instantiation time.

```cpp
namespace ns {
    struct Widget {};
    void log(const Widget&) { /* ... */ }   // found via ADL, no qualification or using needed
}
ns::Widget w;
log(w);   // unqualified call still finds ns::log because ns is Widget's namespace
```

A **hidden friend** is a `friend` function defined *inside* a class body — it is only findable via ADL (because it isn't a member of the enclosing namespace's ordinary lookup set at all, only associated with the class through the friend declaration), never via qualified lookup or a plain namespace-level call with an argument of an unrelated type. This is a deliberate technique, not an accident: it keeps an operator or helper function (commonly `operator==`, `operator<<`, or a `swap`) discoverable exactly when it's relevant (someone has a `Widget` in hand) and invisible otherwise — it can't accidentally be found in an overload set for unrelated types, and it can't be called with explicit `ns::` qualification by someone trying to bypass the type system.

```cpp
namespace ns {
    class Widget {
        int value;
    public:
        friend bool operator==(const Widget& a, const Widget& b) { return a.value == b.value; }  // hidden friend
    };
}
```

### Concepts and Constraints (C++20)

A **concept** is a named, compile-time predicate over a template parameter, checked via `requires`. It replaces SFINAE-based constraining for virtually all new code.

```cpp
template<typename T>
concept Addable = requires(T a, T b) { a + b; };

template<Addable T>
T sum(T a, T b) { return a + b; }
```

A `requires` clause can also be written inline (`template<typename T> requires Addable<T> T sum(T a, T b)`), and a `requires` *expression* (the `requires(T a, T b) { ... }` form above) checks that a set of expressions is well-formed for the given types — this is the mechanism concepts are defined in terms of. Constraint failures produce a direct "constraint not satisfied, here's which one" diagnostic instead of SFINAE's wall of substitution failures — this is the primary practical reason concepts have replaced SFINAE as the default idiom for new template-constraining code.

### Compile-Time Programming: `constexpr`, `if constexpr`, and Beyond

`constexpr` functions can run at compile time (when all their inputs are compile-time constants) or at runtime (otherwise) — the same function body serves both. `if constexpr` is a compile-time-evaluated `if` inside a template: the discarded branch isn't just skipped at runtime, it isn't even instantiated, which is what makes it usable to select between code paths that wouldn't even *compile* for the other branch's type.

```cpp
template<typename T>
auto describe(T x) {
    if constexpr (std::is_pointer_v<T>) return *x;   // only instantiated for pointer T
    else return x;
}
```

### CRTP — The Curiously Recurring Template Pattern

CRTP is a base class templated on its own derived class: `template<typename Derived> class Base { ... };` with `class Derived : public Base<Derived> { ... };`. It gives compile-time (static) polymorphism — the base can call derived-class methods via `static_cast<Derived*>(this)->method()` with no virtual dispatch, no vtable, and (since the concrete type is known at compile time) full inlining opportunity. The tradeoff: no runtime polymorphism (you can't hold a `Base*` to heterogeneous derived types the way virtual dispatch allows), and a mistake — forgetting the derived class actually inherits from `Base<Derived>` with itself as the argument — produces confusing compile errors rather than a runtime symptom.

### Policy-Based Design

A **policy** is a template parameter that supplies a *strategy* (a family of related behaviors) rather than a *data type* — e.g., a container templated on an allocation policy, a locking policy, or a comparison policy, each independently swappable. This composes orthogonal design decisions via template parameters instead of via inheritance or runtime flags, at the cost of each unique combination being a distinct instantiated type (more compile time, larger binaries, per Ch08/Ch10's territory — referenced, not owned, here).

### Type Erasure

**Type erasure** hides a concrete type behind a uniform, type-independent interface, trading compile-time genericity for a single runtime-polymorphic entry point — `std::function`, `std::any` (Ch04), and a hand-rolled "own a `unique_ptr<Concept>` pointing at a `Model<T>`" wrapper are all type erasure. It is the tool that lets a *template*-based generic design interoperate with code that needs one concrete, non-templated type to store, pass across an ABI boundary, or hold heterogeneously in a container — the price is (usually) one indirection and a heap allocation per erased value, plus the loss of the compiler's ability to inline through the erasure boundary.

### Class Template Argument Deduction (CTAD) and Deduction Guides

**CTAD** (C++17) lets a class template's arguments be deduced from constructor arguments, the same way function template arguments are deduced from call arguments — `std::vector v{1, 2, 3};` deduces `std::vector<int>` with no `<int>` written.

```cpp
template<typename T>
class Box {
public:
    Box(T v) : value(v) {}
private:
    T value;
};
Box b{42};   // CTAD deduces Box<int> from the constructor argument, no <int> needed
```

CTAD deduces from the constructors that exist, using the same deduction rules as function templates — which fails in exactly the cases plain function-template deduction fails (e.g., a constructor taking `std::initializer_list` interacting ambiguously with a same-typed multi-argument constructor). A **deduction guide** is an explicit, user-written rule for cases where the constructors alone don't deduce what's intended:

```cpp
template<typename Iter>
class Span {
public:
    Span(Iter first, Iter last);
};
template<typename Iter>
Span(Iter, Iter) -> Span<typename std::iterator_traits<Iter>::value_type>;  // deduction guide
```

Without the guide, `Span{first, last}` would deduce `Span<Iter>` (the iterator type itself) from the constructor signature as written — almost never what's wanted for a range-like wrapper, whose template parameter should be the *element* type, not the *iterator* type. The guide overrides what constructor-based deduction alone would produce, telling the compiler explicitly what the deduced class template arguments should be for that constructor pattern. The standard library ships guides for exactly this reason — e.g. `std::vector(Iter, Iter)`'s implicit constructor-based deduction would deduce the iterator type, so the library provides a guide deducing the iterator's `value_type` instead.

## Common Misconceptions

1. **"A template is compiled once, like a regular function, and then works for any type."** No — each distinct set of template arguments used anywhere in the program causes a *separate instantiation*, each independently type-checked and independently compiled. "It compiles for `int`" says nothing about whether it compiles for `std::string`.

2. **"SFINAE and concepts do fundamentally different things."** No — a `requires`-constrained template still relies on substitution/constraint failure to remove ill-suited overloads from the candidate set; concepts are a better-diagnosed, more readable *syntax and mechanism* for the same underlying substitution-based overload-resolution idea, not a different resolution model entirely.

3. **"`if constexpr`'s discarded branch is just dead code, like a runtime `if (false)` branch."** No — a runtime-false branch still gets compiled (and must be *valid* code for the given type, even if unreachable); `if constexpr`'s discarded branch is not instantiated at all for that specialization, which is precisely why it can contain code that wouldn't even compile for that type.

4. **"CRTP is basically the same as a virtual base class, just with templates instead of `virtual`."** No — CRTP resolves entirely at compile time (no vtable, no indirect call, one concrete type per instantiation); a `Base*` pointing at a CRTP hierarchy cannot hold different `Derived` types polymorphically at runtime the way a `virtual`-based `Base*` can — CRTP buys performance and inlining, not runtime substitutability.

5. **"A partial specialization overrides the primary template the same way a derived-class method overrides a base's."** No — there is no dynamic dispatch involved; the compiler picks the best-matching specialization at *compile time*, based on which pattern the actual template arguments satisfy — there is no notion of "calling the base version" from inside a specialization the way `Base::method()` works for virtual overrides.

6. **"Type erasure is free — it's just an interface."** No — type erasure almost always costs one level of indirection (a pointer to the erased value or its vtable-like dispatch table) and, for owning erasure types like `std::function`/`std::any`, typically a heap allocation per stored value (small-object optimization aside) — it is a deliberate, real tradeoff of runtime cost for interface uniformity, not a free abstraction.

7. **"A concept just renames what SFINAE already checked, so switching costs nothing but syntax."** Mostly true for the *mechanism*, but not for the *contract*: a well-written concept documents intent (`Addable`, `Sortable`) as a named, reusable, composable entity, whereas an `enable_if_t` constraint is usually an anonymous, single-use predicate baked into one function's signature — the switch is also a design improvement, not purely cosmetic.

8. **"A name inside a template body is either resolved or not — 'dependent' is just jargon for 'uses `T`'."** No — dependent vs. non-dependent determines *when* and *how* the name is resolved (phase two, with ADL, vs. phase one, without it), which is why the exact same-looking unqualified call can behave completely differently depending on whether it happens to mention a template parameter — this is a resolution-timing distinction with real behavioral consequences, not a cosmetic one.

9. **"ADL is a fallback that only kicks in when normal lookup finds nothing."** No — ADL's candidates are *added to* the ordinary lookup's candidate set and participate in the same overload resolution together; ADL isn't a last resort tried only after ordinary lookup comes up empty, which is why an ADL-found overload can outrank (be a better match than) an already-visible one, not merely fill in when nothing else exists.

10. **"CTAD means you never need to write a deduction guide — the compiler figures out the right template arguments from the constructors alone."** No — CTAD deduces using the constructors' *signatures*, and a constructor's parameter types are frequently not the same as the class template's intended parameter (e.g. an iterator-pair constructor vs. an element-type template parameter) — a deduction guide exists precisely to correct deductions that would otherwise be technically-consistent but semantically wrong.

## Quick Checks

**05-QC1.** Why can a template that "compiles for `int`" still fail to compile for `std::string`, when a regular (non-template) function that compiles, compiles?

**05-QC2.** Can a function template be partially specialized the way a class template can?

**05-QC3.** What's the practical difference in *diagnostics* between a SFINAE-based `enable_if_t` constraint failing and a C++20 concept constraint failing?

**05-QC4.** Why can `if constexpr`'s discarded branch contain code that would fail to compile for that type, when a runtime `if (false)` branch cannot?

**05-QC5.** Does a CRTP-based `Base<Derived>` allow storing heterogeneous `Derived` types behind a single `Base*` and calling a method polymorphically at runtime?

**05-QC6.** What does a fold expression like `(args + ...)` expand to for a pack `{a, b, c}`?

**05-QC7.** What's the typical runtime cost type erasure (e.g. `std::function`) pays that a plain template parameter doesn't?

**05-QC8.** Why does policy-based design tend to increase compile time and binary size relative to a single non-templated implementation with runtime flags?

**05-QC9.** Inside a template, when is `typename` required before a name like `T::value_type`, and what does the compiler assume about that name if `typename` is omitted?

**05-QC10.** Why can `log(w)` (unqualified, no `using` in scope) find `ns::log` when `w` is of type `ns::Widget`?

**05-QC11.** Why does `Span{first, last}` deduce the wrong template argument without an explicit deduction guide, if `Span`'s only constructor takes two `Iter` parameters?

## Problems

### Level 1 — Recognition

**05-P01.** Given `template<typename T> T identity(T x) { return x; }`, state what happens (deduction, instantiation, or an error) for each call: `identity(5)`, `identity<double>(5)`, `identity("hello")`.

**05-P02.** For `template<typename T> struct Wrapper { T value; };` and a full specialization `template<> struct Wrapper<bool> { ... };`, does the primary template's definition get used at all when a program only ever instantiates `Wrapper<bool>`? Justify.

**05-P03.** Which of these is legal C++: a partial specialization of a *function* template; a partial specialization of a *class* template; an overload of a function template for a more specific type pattern?

**05-P04.** State whether each is a type trait, a concept, or neither: `std::is_integral<T>`, `std::same_as<T, U>` (C++20 concept), `std::vector<T>`, `std::remove_cv_t<T>`.

### Level 2 — Prediction

**05-P05.**
```cpp
template<typename T>
void f(T x) { x.nonexistent_method(); }

int main() { /* f is never called */ }
```
Does this program compile? Justify using the "templates aren't checked until instantiated" rule.

**05-P06.**
```cpp
template<typename T> struct Trait { static constexpr bool value = false; };
template<typename T> struct Trait<T*> { static constexpr bool value = true; };
template<> struct Trait<int> { static constexpr bool value = true; };

std::cout << Trait<int>::value << Trait<int*>::value << Trait<double>::value;
```
Predict the output. Which specialization wins for `Trait<int>` — could both the full specialization and the (non-matching, since `int` isn't a pointer) partial specialization apply?

**05-P07.**
```cpp
template<typename T>
std::enable_if_t<std::is_integral_v<T>, void> print_kind(T) { std::cout << "integral\n"; }

template<typename T>
std::enable_if_t<std::is_floating_point_v<T>, void> print_kind(T) { std::cout << "float\n"; }

print_kind(5);
print_kind(5.0);
print_kind('c');
```
Predict the output for all three calls, including what happens with `char` under `is_integral_v`.

**05-P08.**
```cpp
template<typename... Ts>
constexpr size_t count_args(Ts... args) { return sizeof...(args); }

std::cout << count_args(1, 2.0, "three");
```
Predict the output. Does `sizeof...` evaluate its arguments at runtime?

**05-P09.**
```cpp
template<typename T>
auto safe_divide(T a, T b) {
    if constexpr (std::is_floating_point_v<T>) {
        return a / b;
    } else {
        if (b == 0) throw std::runtime_error("div by zero");
        return a / b;
    }
}
```
For `T = double`, is the `if (b == 0) throw ...` branch's code ever compiled into the instantiation? Justify via `if constexpr`'s discard semantics.

**05-P10.**
```cpp
template<typename T>
concept Printable = requires(T t) { std::cout << t; };

struct NoOutput {};
void show(Printable auto x) { std::cout << x; }

show(5);
show(NoOutput{});
```
Predict which call compiles and which fails, and what the failure looks like relative to an equivalent SFINAE-based version (qualitative description of the diagnostic, not exact text).

**05-P11.** Predict the output:
```cpp
template<typename T>
struct Base {
    void greet() { static_cast<T*>(this)->greet_impl(); }
    void greet_impl() { std::cout << "Base default\n"; }
};
struct Derived : Base<Derived> {
    void greet_impl() { std::cout << "Derived override\n"; }
};
Base<Derived>& b = *(new Derived());
b.greet();
```

**05-P12.**
```cpp
template<typename T>
struct Container {
    std::vector<T> data;
    void add(T x) { data.push_back(std::move(x)); }
};
Container<int> c1;
Container<double> c2;
```
Are `Container<int>` and `Container<double>` the same type, related types (e.g. one derived from the other), or entirely unrelated types with no implicit conversion between them?

**05-P13.**
```cpp
template<typename T, typename U>
auto add(T a, U b) -> decltype(a + b) { return a + b; }

auto r1 = add(1, 2.5);
auto r2 = add(std::string("a"), std::string("b"));
```
Predict the deduced return type for each call, and whether `decltype(a+b)` in the trailing return type requires `a`/`b` to already be in scope (contrast with a leading return type, where they wouldn't be).

**05-P14.** For `template<typename T> void f(T&& x)` called as `f(5)` (an rvalue `int`) and `f(some_lvalue_int)`, what does `T` deduce to in each case? (This is 03-P-family territory — forwarding-reference deduction — now applied inside a template context with additional template parameters possible.)

**05-P15.**
```cpp
template<typename... Args>
void log(Args&&... args) {
    (std::cout << ... << args) << '\n';
}
log(1, " ", 2.5, " ", "three");
```
Predict the output, and identify which fold direction (left or right) `(std::cout << ... << args)` expands in.

**05-P16.**
```cpp
template<typename T>
concept HasSize = requires(T t) { t.size(); };

template<typename T> requires HasSize<T>
void print_size(const T& c) { std::cout << c.size(); }

template<typename T>
void print_size(const T&) { std::cout << "no size\n"; }

print_size(std::vector<int>{1,2,3});
print_size(5);
```
Predict which overload each call resolves to, given that both an unconstrained and a constrained overload exist with the same name.

### Level 3 — Implementation

**05-P17.** Write a function template `template<typename T> T clamp_value(T x, T lo, T hi)` that returns `x` clamped to `[lo, hi]`, constrained with a concept requiring `T` support `<` and `>` comparisons (define the concept yourself; don't reach for `std::totally_ordered` directly, since the point is defining your own constraint).

**05-P18.** Write a variadic function template `template<typename... Ts> auto sum_all(Ts... args)` that sums an arbitrary number of arguments of possibly-different-but-mutually-addable types, using a fold expression. State what happens if called with zero arguments, and whether that's a compile error or well-defined (hint: consider what `(args + ...)` expands to for an empty pack, and whether a fold expression requires an initial value for `+` the way `std::accumulate` does).

**05-P19.** Implement a type trait `template<typename T> struct is_container` (with a `::value` and a `_v` helper) that detects, via SFINAE (not concepts — this problem is specifically about writing the pre-C++20 idiom), whether `T` has both a `.begin()` and a `.end()` member function. Then write the C++20 concept equivalent and compare which is shorter/clearer.

**05-P20.** Implement `CountedBase<Derived>` — a CRTP mixin that tracks how many live instances of `Derived` currently exist, incrementing a static counter on construction and decrementing on destruction, exposing a static `count()`. Demonstrate two distinct `Derived`-like classes each inheriting `CountedBase<ThatClass>` and show their counts are tracked *independently* (not sharing one global counter) — explain precisely why templating the base on `Derived` is what achieves that independence, rather than a single non-templated `CountedBase`.

**05-P21.** Write a small policy-based `Logger<OutputPolicy>` class where `OutputPolicy` is a template parameter supplying a static (or instance) `write(std::string_view)` method; implement two policies — `ConsolePolicy` and `NullPolicy` (a no-op, for disabling logging with zero runtime branching) — and show that `Logger<NullPolicy>`'s `log()` calls can be fully optimized away (state, rather than empirically prove via disassembly, why an optimizer *can* eliminate them entirely, tying it to the policy method being known at compile time with an empty body).

**05-P22.** Implement a minimal type-erasure wrapper `class AnyDrawable` that can hold *any* type with a `.draw()` method (no common base class required among the held types), exposing a uniform `draw()` on `AnyDrawable` itself. Use the classic "concept/model" internal structure (an abstract inner interface plus a templated inner implementation holding the concrete `T`). State which single member function is virtual and why that's the only indirection needed.

**05-P23.** Write a `template<typename T> constexpr T factorial(T n)` that is usable in both a `constexpr` (compile-time) context (e.g. as a `std::array<int, factorial(5)>` bound) and a runtime context (called with a variable, not a literal). Demonstrate one call site of each kind, and explain what makes the *same* function body legal in both contexts (state the relevant rule about `constexpr` function evaluation, not just "it works").

**05-P24.** Implement a fold-expression-based `template<typename... Ts> bool all_true(Ts... args)` and `any_true(Ts... args)` (both take a pack of bool-convertible arguments). State, for `all_true` called with zero arguments, what the fold expression evaluates to and why that particular value is the mathematically/logically correct "identity" for AND (contrast with what `any_true`'s empty-pack identity should be for OR, and confirm the language actually produces that identity or whether you need a special case).

**05-P25.** Write a concept `template<typename F, typename... Args> concept InvocableReturning = requires(F f, Args... args) { { f(args...) } -> std::same_as<bool>; };` (spelled out, don't just use `std::predicate` directly) that constrains `F` to be callable with `Args...` and return exactly `bool`. Demonstrate one lambda that satisfies it and one that doesn't (returns `int` instead), and explain what the `{ expr } -> Concept` requires-expression syntax specifically checks versus a bare `{ expr; }`.

**05-P26.** Implement `template<typename T> struct remove_all_pointers` (a type trait, recursively unwrapping `T**...*` down to `T`) using partial specialization and recursion at the type level (not a runtime loop — this is a compile-time-only construct). Provide the base case and the recursive case, and state how many total specializations are needed structurally (just the primary template plus one partial specialization, or more) and why.

**05-P27.** Write `template<typename Container, typename Predicate> auto count_matching(const Container& c, Predicate pred)` constrained so that `Container` must satisfy a concept requiring `begin()`/`end()` (reuse or adapt 05-P19's concept) and `Predicate` must be invocable with the container's value type and return something convertible to `bool`. Implement the counting logic and demonstrate it compiles for both `std::vector<int>` and `std::list<std::string>` with different predicate types.

**05-P28.** Implement a small `template<typename T, size_t N> class FixedRing` (fixed-capacity ring buffer, no dynamic allocation, `N` known at compile time) that supports `push`/`pop`/`size`, and separately implement a `constexpr`-friendly `capacity()` returning `N` directly usable in a `static_assert`. Explain why making `N` a template parameter (rather than a constructor argument) is specifically what allows the internal storage to be a fixed-size array member (`T data_[N];`) rather than a heap allocation.

**05-P29.** Implement a small `template<typename T> struct is_streamable` type trait (SFINAE-based, following 05-P19's idiom) detecting whether `T` supports `operator<<` into a `std::ostream`, then write a generic `template<typename T> void debug_print(const T& x)` that uses `if constexpr` on `is_streamable<T>::value` to print `x` directly if streamable, or print a fallback like `"<unprintable>"` otherwise — demonstrate both branches actually get exercised with two different `T`s (one streamable, one not).

**05-P30.** Implement `template<typename T> class Wrapper` with an explicit conversion operator `explicit operator T() const` returning its wrapped value, and a *separate*, unconstrained templated conversion `template<typename U> operator U() const` that forwards via `static_cast<U>(value_)`. Demonstrate a call site where overload resolution between the two conversion operators is ambiguous, and explain — using a concept constraint on the templated conversion operator (`requires std::convertible_to<T, U>` or similar) — how you'd resolve the ambiguity by narrowing which `U`s the templated overload even participates for.

### Level 4 — Debugging

**05-P31.** [DEBUG]
```cpp
template<typename T>
class Stack {
    std::vector<T> data_;
public:
    void push(T x) { data_.push_back(x); }
    T pop() {
        T top = data_.back();
        data_.pop_back();
        return top;
    }
};

Stack<std::unique_ptr<int>> s;
s.push(std::make_unique<int>(5));   // does this even compile?
```
Identify the compile error this produces and explain precisely which member function's signature is incompatible with a move-only `T`, then fix `push` (and any other member that needs it) to work correctly for move-only types without breaking copyable types.

**05-P32.** [DEBUG]
```cpp
template<typename T>
struct Adder {
    T value;
    template<typename U>
    auto add(U other) { return value + other; }
};

Adder<int> a{5};
auto r = a.add(2.5);
static_assert(std::is_same_v<decltype(r), int>);
```
The `static_assert` fails. Explain precisely what type `r` actually is and why (walk through `decltype(value + other)`'s type-deduction rules for mixed `int`/`double` arithmetic), and correct the `static_assert` to the actual expected type rather than "fixing" `add` to force a wrong-but-matching type.

**05-P33.** [DEBUG]
```cpp
template<typename T>
concept Comparable = requires(T a, T b) { a < b; };

template<Comparable T>
T my_min(T a, T b) { return a < b ? a : b; }

struct Point { int x, y; };
my_min(Point{1,2}, Point{3,4});
```
Identify the compile error and explain precisely what the concept `Comparable` requires that `Point` doesn't provide, then fix it two different ways: (a) add the missing capability to `Point` directly, (b) leave `Point` unchanged and supply the missing capability externally (state which mechanism from Ch01's or Ch04's territory makes (b) possible).

**05-P34.** [DEBUG]
```cpp
template<typename Derived>
class Counter {
public:
    Counter() { ++count_; }
    static int count_;
    static int count() { return count_; }
};

class Widget : public Counter<Widget> {};
class Gadget : public Counter<Widget> {};   // bug: should be Counter<Gadget>
```
Explain precisely what goes wrong at runtime (not a compile error — this compiles fine) when `Widget` and `Gadget` objects are constructed, given the copy-pasted CRTP argument bug in `Gadget`'s declaration, and what `Widget::count()` vs `Gadget::count()` would each report.

**05-P35.** [DEBUG]
```cpp
template<typename T>
auto get_first(T&& container) {
    if constexpr (std::is_array_v<std::remove_reference_t<T>>) {
        return container[0];
    } else {
        return container.front();
    }
    return T{};   // "just in case" fallback the author added
}
```
Identify why this fails to compile (hint: consider what happens to a function's deduced return type when it has multiple `return` statements of potentially different types, combined with what `if constexpr` does and doesn't guarantee about unreachable code after it), and fix it.

**05-P36.** [DEBUG]
```cpp
template<typename T>
class Wrapper {
public:
    Wrapper(T v) : value_(std::move(v)) {}
    template<typename U>
    Wrapper(const Wrapper<U>& other) : value_(other.value_) {}   // "converting constructor"
private:
    T value_;
};

Wrapper<int> w1(5);
Wrapper<double> w2(w1);
```
Identify the compile error (there is a genuine access/encapsulation issue here, not just a type-conversion one), and explain precisely why a member-template constructor doesn't automatically get access to another instantiation's private members the way you might expect "it's the same class template" to imply. Fix it with the minimal, idiomatic change.

**05-P37.** [DEBUG]
```cpp
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

template<Numeric T>
class SafeDivider {
public:
    static T divide(T a, T b) {
        return a / b;
    }
};

std::cout << SafeDivider<int>::divide(10, 0);
```
This compiles and the concept correctly restricts `T` to arithmetic types — but a reviewer flags that the concept alone hasn't prevented the actual bug class this class was presumably designed to guard against. Identify what class of bug `Numeric` does *not* catch, and explain (don't necessarily fix, unless you choose to) why no concept on `T` alone could catch it — this is a runtime-value problem, not a type problem.

**05-P38.** [DEBUG]
```cpp
template<typename T, typename... Rest>
void print_pack(T first, Rest... rest) {
    std::cout << first << ' ';
    print_pack(rest...);   // recursive pack expansion
}
print_pack(1, 2, 3);
```
Identify the compile error (there's a missing piece specific to variadic-template recursion, not a logic bug in the printing itself), explain exactly why the recursion as written cannot terminate as compiled, and provide the minimal fix.

### Level 5 — Integration

**05-P39.** Design and implement a small generic `template<typename T> class Optional` (a hand-rolled, simplified `std::optional`) supporting construction from a value, `has_value()`, `value()` (throwing when empty), and `value_or(T default_value)`. Constrain any converting-constructor overloads you add with concepts rather than SFINAE, and use `if constexpr` internally at least once to specialize behavior based on whether `T` is trivially destructible (skipping an explicit destructor call when it would be a no-op). State one genuine simplification your implementation makes relative to the real `std::optional` (e.g., no `std::optional<T&>` handling, no monadic `.and_then`) and why that's an acceptable scope cut for this exercise.

**05-P40.** Build a small compile-time "unit-safe quantity" system: `template<typename Tag> class Quantity { double value; };` where `Tag` is an empty struct like `struct Meters{};`/`struct Seconds{};` used purely as a compile-time discriminator (a phantom type). Implement `operator+`/`operator-` (only between same-`Tag` quantities — mismatched tags must fail to compile) and a `operator/` between two *different*-tagged quantities that produces a new, third tag type representing the quotient unit (e.g., `Quantity<Meters> / Quantity<Seconds> -> Quantity<MetersPerSecond>`, where you define `MetersPerSecond` yourself). Explain how this design catches a unit-mismatch bug (e.g., adding meters to seconds) at compile time with zero runtime cost, tying the "zero runtime cost" claim to the fact that `Tag` carries no actual data.

**05-P41.** Implement a policy-based `template<typename T, typename ThreadingPolicy> class Counter` where `ThreadingPolicy` supplies `increment(T&)`/`decrement(T&)` — provide a `SingleThreaded` policy (plain `++`/`--`, no synchronization) and a `ThreadSafe` policy (using `std::atomic<T>`-style operations, or a mutex if `T` isn't naturally atomic). Demonstrate that switching policies is a one-line template-argument change with no change to `Counter`'s own logic, and state precisely what runtime cost `ThreadSafe` pays that `SingleThreaded` doesn't (this previews Ch11's memory-model territory — you don't need to prove thread-safety rigorously here, just identify the cost/guarantee tradeoff the policy switch represents).

**05-P42.** Design and implement a small type-erased `Shape` wrapper (via the concept/model type-erasure pattern from 05-P22) that can hold any type providing `.area()` and `.perimeter()` methods, stored in a single `std::vector<Shape>` alongside genuinely unrelated concrete types (e.g., a hand-rolled `Circle` and a hand-rolled `Square` sharing no common base class before erasure). Implement a `total_area(const std::vector<Shape>&)` free function using only the erased interface. Contrast this design against the alternative of a traditional virtual base class `ShapeBase` with `Circle`/`Square` inheriting from it, and state one concrete advantage type erasure has here (hint: think about types you don't own, e.g. a third-party library's shape-like type you can't retroactively make inherit from your base).

**05-P43.** Implement `template<typename Range> auto to_vector(Range&& r)` (a small, hand-rolled version of `std::ranges::to`) constrained via a concept requiring `Range` to have `begin()`/`end()` and a usable `value_type` (or deduce the element type via `decltype(*std::begin(r))`), that eagerly materializes any range-like input into a `std::vector` of its element type. Demonstrate it working with a `std::views::filter` pipeline (Ch04 material) as input, tying this directly to Ch04's discussion of eager-vs-lazy ranges — explain precisely what compile-time constraint (rather than runtime check) ensures `to_vector` rejects a type with no `begin()`/`end()` at all, with a clear diagnostic, rather than producing a confusing deep-template error.

**05-P44.** Build a small "constrained visitor" system: given `std::variant<A, B, C>` (three unrelated structs), implement a generic `template<typename... Handlers> auto visit_typed(const std::variant<A,B,C>& v, Handlers... handlers)` that behaves like `std::visit` with an overload set, but additionally uses a concept to `static_assert`-enforce (with a clear, custom diagnostic message, not just a raw compiler error) that the number of handlers passed exactly matches the number of variant alternatives, catching "someone forgot a handler" as a *named, readable* compile error rather than `std::visit`'s existing (correct, but generic) overload-resolution failure. State precisely what additional value this custom check adds over just relying on `std::visit`'s own exhaustiveness enforcement from Ch04.

**05-P45.** Design and implement a `template<typename T> class LazyValue` that wraps a `std::function<T()>` computation and caches the result after first access (a memoizing lazy-value wrapper), constrained so that `T` must be at least move-constructible (state your reasoning for why copy-constructible alone wouldn't be a sufficient constraint if you want to support move-only `T` — or state why you chose to require copyable instead, if you go that direction, and justify either choice explicitly). Use `if constexpr` to provide a specialized, cheaper code path when `T` is trivially copyable (state precisely what the cheaper path skips and why it's safe to skip only for that trait).

### Level 6 — Production

**05-P46.** You're asked to add a generic `template<typename T> Result<T> parse_config_value(std::string_view raw)` to a shared configuration library, where `T` may be `int`, `double`, `bool`, `std::string`, or a small closed set of enum types the library doesn't control (each consuming application defines its own enums). Design the constraint on `T` (a concept, not SFINAE) that must be satisfiable by both the library's own built-in types and a consuming application's not-yet-written enum type, without the library needing to enumerate every possible `T` in advance. State what the constraint requires each `T` to provide (a customization point — e.g., a free function `parse_config_value_impl(std::string_view, T&)` found via ADL) and why that customization-point design, rather than requiring `T` to be one of an enumerated closed set, is the correct choice for a library whose consumers add their own types.

**05-P47.** A code review flags a heavily templated policy-based cache class (`template<typename K, typename V, typename EvictionPolicy, typename HashPolicy, typename LockPolicy> class Cache`) as having caused a measurable increase in build times and binary size across the codebase, since every unique combination of policies used anywhere instantiates a fully separate class. Propose two concrete mitigations that preserve the compile-time-genericity benefit for call sites that need it, while reducing the instantiation-explosion cost for the (likely common) case where most call sites actually use the same one or two policy combinations repeatedly. For each mitigation, state its own tradeoff (this connects to Ch08/Ch10's compile-time/binary-size territory, referenced but not owned here — a brief, correct pointer to "this is fundamentally an instantiation-count problem, addressed further via explicit instantiation and extern templates in Ch08/Ch10" is sufficient; you don't need Ch08/Ch10's full mechanics to answer this problem).

**05-P48.** A production library exposes `template<typename T> class Matrix` used across dozens of call sites with `T` almost always being `float` or `double`. A reviewer notices header-only distribution means every translation unit that includes `Matrix` and uses `Matrix<float>` re-instantiates (and re-compiles) the entire class from scratch. Propose a concrete fix using `extern template` declarations plus one explicit instantiation definition in a single `.cpp` file for the two hot types (`float`, `double`), and explain precisely what `extern template class Matrix<float>;` promises the compiler in every other translation unit (i.e., "don't instantiate this here, it exists elsewhere") versus what happens with no such declaration at all. State the one concrete limitation this fix imposes going forward (what happens if a caller elsewhere in the codebase now needs `Matrix<int>`, a type that was never explicitly instantiated).

### Level 7 — Principal Reasoning

**05-P49.** You're the lone C++ specialist on a team building a plugin system: third-party plugin authors (who you don't control and can't require to use your exact compiler/STL version) need to provide types that your host application's generic algorithms (sorting, filtering, serialization) will operate on genericly, without your host application depending on any plugin author's headers at compile time. The requirements you've been given are incomplete — no mention of ABI stability across plugin/host compiler versions, no mention of whether plugin types need to support serialization, no mention of expected plugin count or how often the plugin *type* changes at runtime. Identify at least three concretely missing requirements you would need answered before committing to a design, propose the design you'd tentatively pursue *assuming reasonable answers* to those questions (a specific stance on where templates end and type erasure/an ABI-stable interface begins, and why), and explicitly justify why "let plugin authors write templates that get compiled into the host" is very likely the wrong answer here regardless of how those open questions resolve — tie the justification to a concrete cross-cutting-concept point about compile-time genericity fundamentally requiring shared source-level compilation, which a plugin boundary (separately compiled, dynamically loaded, per Ch08/Ch09's territory) structurally cannot offer.

**05-P50.** Your team's public library exposes a heavily concept-constrained generic API (similar in spirit to 05-P46's `parse_config_value`) that has been stable and well-liked internally for two years. A major consuming team now asks you to relax one of your concepts (currently requiring `std::totally_ordered`) to something weaker, because their type only supports equality, not ordering, and they want to use your API anyway — but doing so would mean the one algorithm in your library that currently relies on sorting internally can no longer assume an order exists. Reason through the tradeoff: propose at least two structurally different ways to resolve this tension (e.g., splitting the API into an ordered and an unordered variant with different guarantees; keeping one API but making the sorting-dependent behavior conditionally available via `if constexpr`/a second constrained overload; or declining the request and explaining why), and for your recommended approach, explicitly identify what future maintenance burden or flexibility you are trading away, and what question you would still need answered from the requesting team before finalizing (this is intentionally open — a reasonable, well-justified stance is graded on the reasoning, not on matching one "correct" architecture).

### Level 2 — Prediction (continued: name lookup, ADL, CTAD)

**05-P51.**
```cpp
template<typename T>
void f(T t) {
    g(t);
}
namespace ns {
    struct Widget {};
    void g(Widget) { std::cout << "ns::g\n"; }
}
void g(int) { std::cout << "::g(int)\n"; }

f(ns::Widget{});
f(42);
```
Predict the output of both calls, and explain why `g(t)` inside `f` can find `ns::g` for the first call despite `ns::g` not being visible via any `using` declaration or qualification anywhere in scope.

**05-P52.**
```cpp
template<typename T>
struct Container {
    typename T::value_type first() const { return {}; }
};
```
This compiles. Now a caller instantiates `Container<int>`. Predict whether this compiles or fails, and explain specifically what `typename T::value_type` is asking the compiler to do, and why the failure (if any) happens at instantiation time rather than at the point `Container` itself was defined.

**05-P53.**
```cpp
template<typename T>
class Widget {
public:
    Widget(T v) : value(v) {}
    Widget(std::initializer_list<T> vs) : value(*vs.begin()) {}
private:
    T value;
};
Widget w1{5};
Widget w2{1, 2, 3};
```
Predict what `w1` and `w2` deduce to via CTAD, paying attention to which constructor each braced-init-list call actually selects — and state whether this is the same ambiguity `std::vector v{5};` (int, not `initializer_list`) versus `std::vector v{1,2,3};` runs into.

### Level 4 — Debugging (continued: name lookup, ADL, CTAD)

**05-P54.** [DEBUG]
```cpp
template<typename Iter>
class Range {
public:
    Range(Iter first, Iter last) : first_(first), last_(last) {}
private:
    Iter first_, last_;
};

std::vector<int> v{1, 2, 3};
Range r{v.begin(), v.end()};
// caller expected r to be usable as Range<int> (a range of the vector's element type)
```
Identify what `Range` actually deduces to via CTAD as written (hint: look at the constructor's parameter types, not at what the caller *wants*), explain why that's not a bug in CTAD itself, and write the deduction guide that makes `r` deduce the intended element type instead.

**05-P55.** [DEBUG]
```cpp
template<typename T>
class Node {
public:
    T::child_type* first_child();   // intent: return a pointer to the first child node
private:
    T::child_type* child_ = nullptr;
};
```
This fails to compile for every `T`, even ones that do have a nested `child_type`. Identify the missing keyword(s) (there are two separate spots that need it), explain why the compiler's default assumption about `T::child_type` without the fix is wrong here, and provide the corrected declarations.

### Level 7 — Principal Reasoning (continued: name lookup)

**05-P56.** Your team is designing a header-only numerics library that must interoperate with third-party numeric types the library doesn't control (a `BigInt` class from one dependency, a `Rational` class from another), including calling each type's own `operator+`, `operator*`, and a hypothetical `to_string`-like customization point those types may or may not provide. The requirements you've been given don't specify whether the third-party types live in their own namespaces, whether they might be found only via ADL or might also be visible in the global namespace (risking ambiguity), or what should happen when a type provides no customization point at all. Identify at least two concretely missing requirements you'd need answered, and propose a design for the customization point (a free function found via ADL, a trait class the third-party author specializes, or an alternative) that works correctly whether or not the third-party type's author anticipated your library's existence — explicitly justify why relying purely on ADL-found free functions, with no fallback, is risky for a library whose consumers may have types with no such free function defined at all, and what you'd do about that gap.

## Integration Challenge — 05-IC1

Design a constrained generic algorithm — a `template<typename Container> auto median(Container& c)` that returns the median value of any container supporting the necessary operations — that works correctly over **both** a standard container (e.g. `std::vector<int>`) and a hand-rolled custom container type you define yourself (not derived from any standard container, not just a thin wrapper — it must have its own genuinely independent storage and iterator type).

1. Write the hand-rolled container first (a small fixed-or-dynamic collection with its own iterator type satisfying at least Forward iterator, per Ch04's iterator-category vocabulary).
2. Write `median` using a **C++20 concept** constraining `Container` to whatever capability set `median`'s implementation actually needs (don't over-constrain — state exactly which operations you require and why each is necessary, not just "it needs to be a container").
3. Now write the **SFINAE-era equivalent** of the same constraint (an `enable_if_t`-based version of `median`), and compare, concretely, the compiler diagnostic each version produces when called with a type that satisfies *most but not all* of the requirement (e.g., has `begin()`/`end()` but its iterator is only Input, not Forward, or doesn't support the arithmetic `median` needs) — you don't need the literal compiler output, but describe qualitatively what each diagnostic's structure and readability would be.
4. State whether `median`'s implementation needs to `sort` a copy of the container's elements internally (and if so, why a copy rather than sorting in place — connect this to whether the caller's container is logically ordered data the algorithm shouldn't reorder as a side effect) — and whether that sorting requirement changes what iterator category your concept must demand from the container.

## Chapter Projects

This chapter feeds directly into:

- **[P-2.3](../PROJECT_ROADMAP.md) `function_ref` + `unique_function`** — a type-erased callable-wrapper library draws directly on 05-P22/05-P42's type-erasure pattern and 05-P14's forwarding-reference-in-templates discipline.
- **[P-2.5](../PROJECT_ROADMAP.md) Compile-Time Reflection-Free Serializer** — a versioned, endian-correct serializer built generically over arbitrary user types draws on 05-P19's trait/concept-detection pattern, 05-P23/05-P28's compile-time-constant-driven design, and 05-P46's customization-point ADL pattern for extending behavior to types the library doesn't own.
