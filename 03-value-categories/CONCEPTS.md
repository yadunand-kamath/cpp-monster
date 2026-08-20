# Chapter 03 — Value Categories, Move Semantics, and Forwarding

> Prerequisites: [Chapter 01](../01-core-semantics/CONCEPTS.md), [Chapter 02](../02-lifetime-raii/CONCEPTS.md).
> This chapter is **prediction-heavy by design** — the expression taxonomy is something you learn by predicting compiler behavior, not by reciting definitions. Don't be surprised that L1 is nearly empty and L2 is the largest tier.

## Crash Course

### The Value Category Taxonomy

Every C++ expression has a **type** and a **value category**. The value category answers: does this expression identify a persistent object with identity, and can it be moved from?

The primary categories (post-C++11) form this tree:

```
                    expression
                   /          \
              glvalue        rvalue
             /       \       /     \
         lvalue    xvalue          prvalue
```

- **lvalue** — has identity, cannot be moved from (in general). Example: a named variable `x`, a dereferenced pointer `*p`, a function returning `T&`.
- **prvalue** ("pure rvalue") — has no identity, can be moved from. Example: a literal `42`, the result of `x + y`, a function returning `T` by value (before C++17's guaranteed elision, this "materialized" a temporary; post-C++17, prvalues are not materialized until needed).
- **xvalue** ("expiring value") — has identity, but can be moved from. Example: `std::move(x)`, a function returning `T&&`.
- **glvalue** — lvalue or xvalue (has identity, regardless of movability).
- **rvalue** — xvalue or prvalue (regardless of identity, can be moved from).

**The two questions that fully classify any expression:** (1) does it have identity? (2) can it be moved from? Four combinations, but "no identity + cannot move" is empty — nothing is both unidentifiable and immovable in a meaningful sense (a `const` prvalue is a degenerate edge case addressed in Common Misconceptions).

### `std::move` Does Nothing At Runtime

`std::move(x)` is `static_cast<T&&>(x)` — a **compile-time relabeling** of `x` as an xvalue. It doesn't move anything; it doesn't call any function; it produces zero instructions on its own. All it does is change which overload resolution picks for the expression that follows — specifically, it makes rvalue-reference overloads (like a move constructor) viable candidates. If no move constructor exists, `std::move(x)` used in initialization silently falls back to copy — a `std::move` that "does nothing" is not a compile error, which is precisely what makes it a booby trap for a class that's movable-looking but isn't (see 02-P03/02-P11 from last chapter, revisited here as 03-P09).

### Copy Elision and NRVO

**Copy elision** is the compiler omitting a copy/move construction entirely, constructing the object directly in its final location.

- **Mandatory elision (C++17):** a prvalue used to initialize an object of the same type is never materialized into a temporary first — `T x = f();` where `f()` returns `T` by value constructs `x` directly, no copy or move constructor is invoked at all (not "invoked and elided" — genuinely never called; this is why the type doesn't even need to be copyable or movable for this to compile).
- **NRVO (Named Return Value Optimization):** returning a *named* local variable is only an **optional, quality-of-implementation** elision, never guaranteed by the standard — a compiler is permitted to elide the move/copy of `return local;`, and virtually all mainstream compilers do at typical optimization levels, but nothing requires it, and specific conditions (multiple return paths returning different named locals, a returned parameter, debug builds with optimizations disabled) commonly defeat it.
- The distinction matters concretely: mandatory elision means "the copy/move constructor is never called, full stop, this is guaranteed"; NRVO means "the copy/move constructor is *usually* not called, verify with instrumentation, don't rely on it for correctness — only for performance."

### Move Semantics: What "Moving From" Actually Means

A moved-from object is left in a **valid but unspecified state** (per the standard's requirement on the standard library's own types; a hand-written class can define whatever moved-from state it wants, but "valid but unspecified" is the conventional contract to honor). Valid means: its destructor may still run safely, and it may be assigned a new value — it does *not* mean its old value is preserved, retrievable, or even sensible to read.

- `std::unique_ptr`'s move constructor: source becomes `nullptr`. This one is standard-guaranteed, not just conventional — it's explicit in the spec.
- `std::vector`'s move constructor: source becomes empty (also standard-guaranteed for `vector` specifically, though not for every container).
- A hand-written class's move constructor: whatever you wrote it to do — if you don't null out a raw pointer member after "moving" it, the moved-from object still holds it, and its destructor still runs, potentially double-freeing (this is 03-P14).

### Forwarding References and Reference Collapsing

`template <typename T> void f(T&& x)` — this `T&&` is a **forwarding reference** (colloquially "universal reference"), not an ordinary rvalue reference, specifically because `T` is deduced *at this call site* from a template parameter. The deduction rule:

- Calling `f(lvalue)` deduces `T = U&`, and reference collapsing turns `T&& = U& &&` into `U&` — an lvalue reference.
- Calling `f(rvalue)` deduces `T = U`, so `T&& = U&&` — an rvalue reference, unchanged.

**Reference collapsing rules** (the only two that ever apply, since you cannot directly write a reference to a reference — collapsing only happens through this template-substitution mechanism): `& &` → `&`, `& &&` → `&`, `&& &` → `&`, `&& &&` → `&&`. Mnemonic: any `&` in the mix wins, unless both are `&&`.

A **non-template** `T&&` (where `T` is a concrete, already-known type, not deduced at this call) is always an ordinary rvalue reference — it is never a forwarding reference, no matter how it looks syntactically. `void f(std::string&& x)` binds only to rvalues; `template<typename T> void f(T&& x)` called as `f(some_string)` binds to that lvalue. Distinguishing these by eye is a common source of confusion, addressed directly in Common Misconceptions.

### `std::forward`

`std::forward<T>(x)` conditionally casts `x` to an rvalue reference *only if* `T` was deduced as a non-reference type (i.e., the original argument was an rvalue) — otherwise it casts to an lvalue reference, preserving the argument's original value category through the forwarding call. Its entire purpose is inside a forwarding-reference function template: `f(x)` (no forward) always passes `x` as an lvalue to whatever `f` is called next, regardless of what was originally passed in — because a named parameter, once bound, is itself an lvalue expression when you refer to it by name. `std::forward<T>(x)` restores the category information that binding to a named parameter erased.

### The `noexcept` Connection to Move Selection (Stub — Formalized in Ch06)

Several standard containers (`std::vector` on reallocation being the canonical case) use `std::move_if_noexcept` internally: if a type's move constructor is not marked `noexcept` (and it has no usable copy constructor... actually the condition is: no accessible copy constructor, or the move constructor is noexcept), the container falls back to **copying** during reallocation rather than moving, specifically to preserve the strong exception-safety guarantee — a copy that throws mid-reallocation leaves the original elements untouched, but a move that throws mid-reallocation leaves you with some elements already moved-from and no way to undo it. This is introduced here only as a fact to know; the exception-safety guarantee vocabulary (basic/strong/nothrow) is formalized in Chapter 06 and revisited there.

### Copy/Move Cost — Semantics Half (Measurement Deferred to Ch12)

This chapter teaches *which* operation the compiler selects and *why* — a move constructor is asymptotically cheaper than a copy constructor precisely when the type owns a resource that can be transferred by pointer/handle swap rather than duplicated (a `vector`'s heap buffer, a `unique_ptr`'s pointer) — but does not teach how to *measure* that cost empirically or reason about cache effects; that's Chapter 12, which revisits this exact chapter's Integration Challenge instrumentation technique under a profiler.

## Common Misconceptions

1. **"`std::move(x)` moves `x`."** No — `std::move` performs no action; it's a cast to xvalue that makes move-constructor overloads *eligible*. Whether a move actually happens depends entirely on what happens with the result (assigned to something? Ignored? Is there even a move constructor?).

2. **"Any `T&&` parameter is a forwarding reference."** No — only `T&&` where `T` is a template parameter deduced *at this call*. `void f(std::vector<int>&& v)` is an ordinary rvalue reference — `std::vector<int>` is a concrete type, not a deduced template parameter, even though the function might itself be inside a class template.

3. **"NRVO is guaranteed by the standard, just like the C++17 mandatory-elision case."** No — NRVO is a permitted, common, but *optional* optimization. Mandatory elision applies specifically to prvalues initializing same-type objects; returning a named local is a different, non-guaranteed case. Conflating the two leads to code that silently regresses (extra copy/move) on a compiler or optimization level where NRVO doesn't kick in.

4. **"A moved-from object has no valid state — don't touch it at all."** For standard-library types, a moved-from object is valid (destructible, reassignable) but its *value* is unspecified. Calling its destructor, or assigning it a new value, is fine and expected; reading its old value or relying on any particular content is not.

5. **"`std::forward` is just a fancier `std::move`."** No — `std::forward<T>(x)` is conditional based on `T`; it forwards `x` as whatever value category it originally had. `std::move` unconditionally casts to xvalue regardless of what was passed. Using `std::move` instead of `std::forward` inside a forwarding-reference template forces an rvalue cast even when the caller passed an lvalue they expected to survive the call — a real bug, covered in 03-P22.

6. **"Returning `std::move(local)` helps NRVO."** It does the opposite in many cases — wrapping a named local in `std::move` for a return statement can *defeat* NRVO (because the expression is no longer simply "the named local," it's a cast expression), forcing a move construction where the compiler might otherwise have elided entirely. The correct idiom is `return local;`, unadorned — let the compiler apply NRVO or fall back to a move on its own.

7. **"A `const` rvalue and a non-const rvalue behave the same for overload resolution."** They don't — `const T&&` is a real, distinct category from `T&&`, and a `const` prvalue/xvalue will *not* bind to a non-const `T&&` move-constructor overload; it falls back to the `const T&` copy overload instead, silently converting an intended move into a copy. This is a genuine edge case (returning `const T` by value is the classic way to trigger it) and is why "prvalue implies movable" in the Crash Course's taxonomy needs this asterisk.

## Quick Checks

**03-QC1.** What are the two yes/no questions that fully classify any C++ expression's value category?

**03-QC2.** Is `std::move(x)` in `T y = std::move(x);` guaranteed to invoke `T`'s move constructor?

**03-QC3.** `template<typename T> void f(T&& x)` is called as `f(42)`. What is `T` deduced as, and what is the resulting type of `x`?

**03-QC4.** Is `void g(std::string&& s)` a forwarding reference?

**03-QC5.** True or false: mandatory copy elision (C++17) means the copy/move constructor is called but its call is optimized away.

**03-QC6.** Why does `return std::move(local);` often produce *worse* code than `return local;`?

**03-QC7.** Inside a forwarding-reference function template, why does simply writing `next(x)` (instead of `next(std::forward<T>(x))`) always pass `x` to `next` as an lvalue, regardless of what the caller originally passed?

**03-QC8.** A function returns `const std::string` by value. The caller does `std::string s = std::move(f());`. Does this invoke the move constructor?

## Problems

### Level 1 — Recognition

**03-P01.** For each expression, classify it as lvalue, prvalue, or xvalue: (a) `x` where `int x;`, (b) `42`, (c) `std::move(x)`, (d) `x + 1`, (e) a call to `int getValue();`, (f) a call to `int& getRef();`, (g) a call to `int&& getRvalueRef();`.

**03-P02.** Given `template<typename T> void f(T&& x)`, state whether `x`'s effective type is an lvalue reference or rvalue reference for each call: (a) `f(10)`, (b) `int n = 5; f(n);`, (c) `f(std::move(n));`, (d) `const int c = 1; f(c);`.

**03-P03.** Apply the reference-collapsing rules to reduce each: (a) `int& &`, (b) `int& &&`, (c) `int&& &`, (d) `int&& &&`. (These aren't directly writable in source — assume they arise via template substitution.)

### Level 2 — Prediction

**03-P04.** Predict, with justification, whether each of the following invokes the copy constructor, the move constructor, or neither (elided), assuming `Widget` has both a copy and a (non-`noexcept`) move constructor, both instrumented to print:
```cpp
Widget a;
Widget b = a;                    // (a)
Widget c = std::move(a);         // (b)
Widget d = Widget();             // (c)
Widget make() { return Widget(); }
Widget e = make();               // (d)
Widget f(Widget w) { return w; } // (e) -- called as f(Widget())
```

**03-P05.** `std::vector<Widget> v; v.push_back(w);` where `w` is an lvalue `Widget` with both copy and move constructors, move marked `noexcept`. Which constructor runs? Now suppose the move constructor is *not* marked `noexcept` (copy still available) and a reallocation is triggered by this `push_back` — which constructor runs for the *existing* elements being relocated to the new buffer, and why is it different from the one used for `w` itself?

**03-P06.** A function is declared `std::string f();`. At the call site `std::string s = f();`, is the copy/move constructor of `std::string` guaranteed to be elided, guaranteed to run, or implementation-defined? Does your answer change under `-std=c++14` versus `-std=c++17` (MSVC: `/std:c++14` vs `/std:c++17`)?

**03-P07.** 
```cpp
std::string make(bool flag) {
    std::string a = "long-lived-a", b = "long-lived-b";
    if (flag) return a;
    return b;
}
```
Is NRVO guaranteed here? Explain what property of this function (relative to a single-named-local, single-return-path version) makes NRVO's applicability weaker or implementation-dependent.

**03-P08.** Predict what happens (compiles and moves? compiles and copies? fails to compile?) for:
```cpp
const std::vector<int> make_const_vec() { return {1,2,3}; }
std::vector<int> v = std::move(make_const_vec());
```
Justify by value category and overload resolution, referencing Misconception 7.

**03-P09.** [DEBUG] A teammate writes:
```cpp
class Buffer {
public:
    Buffer(Buffer&& other) { data_ = other.data_; other.data_ = nullptr; }
    // no destructor, no copy constructor, no copy/move assignment written
private:
    int* data_;
};
Buffer make_buffer() { Buffer b(/*...*/); return std::move(b); }
```
They observe that a `Buffer` "seems to move fine" in testing but their program corrupts memory intermittently in production. Identify every special member function silently missing here and explain the compounding hazard (tie back to Ch02's Rule of 5).

**03-P10.** Given `void f(int& x)`, `void f(int&& x)`, and `void f(const int& x)` all declared as overloads, which one is selected for: (a) `int a; f(a);`, (b) `f(5);`, (c) `const int c = 3; f(c);`, (d) `int a; f(std::move(a));`?

**03-P11.** 
```cpp
template<typename T>
void wrapper(T&& arg) {
    inner(std::move(arg));
}
```
`wrapper` is called twice: once as `wrapper(some_lvalue)`, once as `wrapper(std::move(some_other))`. In the *first* call, is using `std::move(arg)` (instead of `std::forward<T>(arg)`) a correctness bug, given that `arg` is a named parameter inside `wrapper`? Explain what value category `arg` has as a named entity, independent of what was passed to `wrapper`.

**03-P12.** Two overloads: `void g(std::string s)` (by value) and `void g(const std::string& s)` are **not** both declared — only the by-value one exists. Predict, for `std::string s1 = "x"; g(s1); g(std::move(s1)); g("literal");`, which of the three calls constructs `s` (the by-value parameter) via copy versus move.

**03-P13.** 
```cpp
struct Pair {
    std::string a, b;
};
Pair make() { return {"x", "y"}; }
Pair p = make();
```
Under C++17 mandatory elision rules, how many `std::string` constructions occur in total for `p.a` and `p.b` combined (not counting the string literals' own conversion), and how many `Pair` copy/move constructions occur?

**03-P14.** [DEBUG]
```cpp
class Handle {
public:
    Handle(Handle&& other) noexcept : fd_(other.fd_) {}  // missing: other.fd_ = -1;
    ~Handle() { if (fd_ >= 0) close(fd_); }
private:
    int fd_;
};
```
Identify the bug purely from the value-category/move-semantics angle (not the Ch02 RAII angle already covered) — specifically, what part of the "valid but unspecified moved-from state" contract is violated, and what's the concrete failure mode when a moved-from `Handle` is later destroyed?

**03-P15.** A function template is written as:
```cpp
template<typename T>
void set(T& target, T&& value) { target = std::move(value); }
```
Explain why this is **not** a forwarding-reference overload despite containing `T&&`, given that `T` also appears as `T&` in the same template's parameter list (so `T` is still deduced once, consistently, from both parameters — reason about what that implies for what can legally be passed as `value`).

**03-P16.** `auto&& x = some_expression;` — for each of the following as `some_expression`, state whether this compiles and, if so, what `x`'s deduced type is: (a) `42`, (b) `some_lvalue_int`, (c) `std::move(some_lvalue_int)`, (d) a call to a function returning `int&`.

**03-P17.** Trace this exact call chain and state, for each numbered line, whether the object bound to the parameter is treated as an lvalue or rvalue by the *next* function in the chain:
```cpp
template<typename T> void c(T&& x) { /* (3) */ }
template<typename T> void b(T&& x) { c(std::forward<T>(x)); /* (2) */ }
template<typename T> void a(T&& x) { b(std::forward<T>(x)); /* (1) */ }
a(std::string("temp"));
```

**03-P18.** 
```cpp
std::vector<int> v = {1,2,3};
std::vector<int> v2 = std::move(v);
v.push_back(4);
```
Is the `push_back` on line 3 well-defined? What does `v` contain immediately before it, per the standard's guarantee for `vector`'s move constructor specifically (not merely "unspecified")?

**03-P19.** For a type `T` where the move constructor is present but declared **without** `noexcept`, and the copy constructor is also present and accessible, predict which is used when a `std::vector<T>` reallocates during a `push_back` that exceeds current capacity. Now predict the same thing if `T`'s copy constructor is `= delete`d (move still non-`noexcept`).

### Level 3 — Implementation

**03-P20.** Implement `template<typename T> constexpr T&& my_move(T&& t) noexcept;` from scratch (i.e., a from-scratch `std::move`) — no using `std::move` inside it. State why the parameter must be declared `T&&` (a forwarding reference) rather than a plain `T&`, even though the function unconditionally casts to rvalue.

**03-P21.** Implement `template<typename T> constexpr T&& my_forward(T&& t) noexcept;` (a from-scratch `std::forward`) using `std::is_lvalue_reference` and `std::remove_reference` (or equivalent trait logic you write yourself) to decide the cast target. Explain, in your own implementation's terms, why the deduced `T` alone carries enough information to reconstruct the original value category.

**03-P22.** [DEBUG] A perfect-forwarding wrapper is buggy:
```cpp
template<typename T>
void log_and_call(T&& arg) {
    std::cout << "calling with arg\n";
    target_function(std::move(arg));   // bug
}
```
Fix it to correctly preserve the caller's original value category, and construct a concrete calling scenario (a specific call to `log_and_call`) whose observable behavior differs between the buggy and fixed versions — describe what a caller would see go wrong with the bug in place.

**03-P23.** Implement a minimal move-only RAII type `UniqueBuffer` (owns a `int* ` and a `size_t`) with: constructor, destructor, deleted copy operations, and correctly-implemented move constructor and move-assignment operator that leave the moved-from object in the standard-library-style "valid but unspecified" (specifically: null pointer, zero size) state. Then write a short test that move-assigns a `UniqueBuffer` to itself (`a = std::move(a);`) and explain why your move-assignment operator must guard against this (or explain why, given your specific implementation, it happens to be safe without an explicit guard — justify either answer with the actual code).

**03-P24.** Write a function template `template<typename... Args> auto make_thing(Args&&... args)` that perfectly forwards a variadic argument pack into `std::make_unique<Thing>(...)`. Explain what reference-collapsing/deduction happens per-argument when this is called with a mix of lvalues and rvalues, e.g. `make_thing(lvalue_arg, 42, std::move(other_lvalue));`.

**03-P25.** Implement a `swap` function template by hand (`template<typename T> void my_swap(T& a, T& b)`) using only move construction and move assignment (no `std::swap`, no copy). State the exception-safety property your implementation has if `T`'s move constructor can throw partway through, versus if it's `noexcept`.

**03-P26.** Given the class from 03-P23 (`UniqueBuffer`), add a `clone()` member function that produces a deep-copy *without* adding a public copy constructor to the class itself (i.e., the class remains move-only from the outside, but `clone()` provides an explicit, discoverable way to get an independent copy when genuinely needed). Explain the API-design reasoning for preferring an explicit `clone()` over a public copy constructor for a type like this.

**03-P27.** Write two overloads of a function `process`:
```cpp
void process(const std::vector<int>& v);  // (A) — binds to lvalues, doesn't take ownership
void process(std::vector<int>&& v);       // (B) — binds to rvalues, can steal
```
Implement (B) to actually take advantage of the fact that its argument is moveable (e.g., move it into a member/local rather than copying), and write three call sites — one that must call (A), one that must call (B), and one where the caller has an lvalue they no longer need and *wants* (B) to be called, showing how they'd force that.

### Level 4 — Debugging

**03-P28.** [DEBUG] 
```cpp
std::string& get_ref_to_temp() {
    std::string s = "danger";
    return s;
}
std::string& r = get_ref_to_temp();
std::cout << r;
```
This is a dangling-reference bug, but explain it specifically in value-category terms: what is the value category of the call expression `get_ref_to_temp()`, and why does binding a *named* `std::string&` to it not extend the temporary's lifetime the way binding a `const std::string&` directly to a temporary *would* (temporary lifetime extension) — connect this to why the function itself, not just the binding, is the actual defect.

**03-P29.** [DEBUG]
```cpp
template<typename T>
T&& forward_wrong(T&& x) { return x; }  // returns T&&, but by returning the *parameter* as-is

auto&& r = forward_wrong(std::string("temp"));
std::cout << r;
```
Is `r` dangling here? Distinguish this case carefully from 03-P28 — trace exactly which object `x` refers to, whether that object's lifetime has ended by the time `r` is used, and why returning `T&&` (as opposed to `T`) from a function taking a forwarding reference is itself not automatically the bug (identify what specific extra ingredient would make it one).

**03-P30.** [DEBUG]
```cpp
class Matrix {
public:
    Matrix(Matrix&& other) noexcept
        : data_(other.data_), rows_(other.rows_), cols_(other.cols_) {}
    // data_ intentionally left in other, "for performance, to skip the null-check branch"
private:
    double* data_;
    int rows_, cols_;
};
```
The author's stated rationale skips nulling `other.data_`. Walk through what happens the next time this moved-from `Matrix` is destroyed or move-assigned-from again, and explain precisely which "valid but unspecified" contract clause this violates (hint: it's not about the *value* being unspecified).

**03-P31.** [DEBUG] A benchmark reports that a hand-written `Vector<T>::push_back` triggering reallocation is mysteriously falling back to O(n) *copies* instead of O(n) *moves* on every single reallocation, for a `T` the author insists "definitely has a move constructor." Given only that fact, what is the single most likely cause, and what one-line change to `T`'s declaration would confirm/fix it? (Reference the `move_if_noexcept` mechanism from the Crash Course.)

**03-P32.** [DEBUG]
```cpp
std::vector<std::unique_ptr<int>> v;
v.push_back(std::make_unique<int>(1));
auto p = v[0];               // (a)
auto p2 = std::move(v[0]);   // (b)
```
Line (a) fails to compile. Explain precisely why in value-category terms (what value category is `v[0]`, and why does that category matter for `unique_ptr`'s available constructors), then explain why line (b) — if used as a replacement, not in addition — compiles and what state `v[0]` is left in afterward.

**03-P33.** [DEBUG] A reviewer flags this function:
```cpp
template<typename T>
void store(T&& item) {
    buffer_.push_back(std::forward<T>(item));
    log(item);   // uses item again after forwarding it
}
```
Explain precisely what's wrong with using `item` again after `std::forward<T>(item)` was used to (potentially) move from it — specifically, under what caller-supplied argument (lvalue vs. rvalue) does this become an actual bug versus merely bad style, and why.

**03-P34.** [DEBUG]
```cpp
struct Node {
    Node(Node&& other) = default;   // (a)
    ~Node() { std::cout << "destroyed\n"; }  // (b) added later by another developer
    std::vector<int> payload;
};
```
A developer adds the destructor at `(b)` to log destruction, without touching `(a)`. Explain what happens to `Node`'s move constructor and move-assignment operator as a *result* of adding `(b)`, tying this back to the special-member-generation rule from Chapter 02, and predict the concrete, observable performance regression this causes for code that does `Node n2 = std::move(n1);` (in terms of what happens to `payload` specifically).

### Level 5 — Integration

**03-P35.** Design and implement a small `Logger` class that owns a `std::ofstream` (move-only by nature, since streams aren't copyable). Give `Logger` a correct move constructor and move-assignment operator (the copy operations should not exist — don't write deleted stubs if letting them be implicitly suppressed already achieves it; justify whether that's actually the case here per Ch02's suppression rules applied to a class with a non-copyable member). Then write a factory function `Logger make_logger(const std::string& path)` and explain, referencing this chapter's elision rules, exactly what happens (construction-wise) at `Logger l = make_logger("out.log");` — how many `Logger` constructions actually occur, and whether that answer is guaranteed or merely typical.

**03-P36.** Write a generic `template<typename T, typename... Args> T make_and_log(Args&&... args)` factory that perfectly forwards its arguments to `T`'s constructor, logs the type name, and returns the constructed `T` by value. Verify (by reasoning, and if you have a compiler handy, by instrumenting `T`'s constructors) that no copy or move of `T` occurs at the *call site* `T obj = make_and_log<T>(1, 2, 3);` beyond what's mandatorily-elided-away, tracing the full chain: argument forwarding into `T`'s constructor, `T`'s prvalue construction inside `make_and_log`, and `make_and_log`'s own return.

**03-P37.** You're given a class `LegacyBuffer` from a third-party header you cannot modify: it has a working copy constructor, no move constructor, and no destructor issues (it's Rule-of-0-safe internally via a `vector` member, so copies are merely "wasteful," not unsafe). Write a wrapper or free function that lets *your* code move-construct-like behavior out of a `LegacyBuffer` lvalue you're about to discard, without modifying the class — and explain the actual limits of what you can achieve this way (can you make `std::move(legacy_buffer_instance)` actually invoke a move, or only simulate the effect at your call sites?).

**03-P38.** Implement a small type-erasure wrapper `AnyCallable` (conceptually a tiny slice of `std::function`) that must correctly perfect-forward its call arguments to the wrapped callable regardless of the callable's own parameter value-category requirements. Specifically: `AnyCallable` wraps any callable `F`, and its own `operator()` is a variadic forwarding-reference template that forwards straight through to `F`'s `operator()`. Explain what would go wrong (in terms of category loss) if `operator()` took its arguments by plain `Args...` (by value) instead of `Args&&...`.

### Level 6 — Production

**03-P39.** You're asked to review a production PR that changes a hot-path function's signature from `void process(const std::vector<Event>& events)` to `void process(std::vector<Event> events)` (by value) with the justification "the caller usually has a temporary anyway, so this avoids a copy via move." Evaluate this justification rigorously: under what caller shapes is it actually true, under what caller shapes does it silently regress (introduce a copy that didn't exist before), and what signature (possibly requiring two overloads, or a forwarding-reference template, or leaving it as `const&` and taking a `std::move` at the one call site that needs it) would you recommend instead, with justification tied to this chapter's mandatory-elision and reference-collapsing rules — not vibes.

**03-P40.** A codebase has a widely-used utility `template<typename T> T identity(T&& x) { return std::forward<T>(x); }` intended purely as a forwarding pass-through (e.g., used inside macro-generated code to normalize an expression before further processing). A bug report claims that for certain call sites, `identity` introduces an *extra* copy that direct use of the underlying expression would not have. Determine under what conditions this claim is true, precisely diagnosing the interaction between `identity`'s **by-value return type** (`T`, not `T&&` or `decltype(auto)`) and the value category of what's passed in — and propose (with tradeoffs, not just "always use `decltype(auto)`") the return-type change that would eliminate the issue, including what new hazard that changed return type reintroduces (referencing 03-P29's returned-reference-to-parameter danger) and why it does *not* actually apply in this specific `identity`-forwarding case if implemented carefully — or does; make the actual determination.

## Integration Challenge — 03-IC1

Given a small class hierarchy:

```cpp
class Base {
public:
    Base();
    Base(const Base&);
    Base(Base&&) noexcept;
    virtual ~Base();
private:
    std::vector<int> owned_;
};
class Derived : public Base {
public:
    Derived();
    Derived(const Derived&);
    Derived(Derived&&) noexcept;
private:
    std::string extra_;
};
```

Instrument every constructor (copy and move, both classes) to print its own name. For each of the following five call-site shapes, **first predict** which constructor(s) run and how many times, **then verify** by compiling and running:

1. `Derived d1; Derived d2 = d1;`
2. `Derived d1; Derived d2 = std::move(d1);`
3. `Derived make() { Derived d; return d; }` then `Derived d3 = make();`
4. `std::vector<Derived> v; v.reserve(1); v.push_back(Derived());` followed by a second `push_back` that forces reallocation.
5. `void consume(Derived d) {} consume(Derived());` versus `Derived d; consume(d);` versus `consume(std::move(d));`

For any prediction that turns out wrong once verified, diagnose *why* your mental model was incomplete — tie the correction back to a specific Crash Course section or Common Misconception in this chapter.

## Chapter Projects

This chapter feeds directly into:

- **[P-1.3](../PROJECT_ROADMAP.md) `small_vector<T, N>`** — a small-buffer-optimized vector requires getting move construction, move assignment, and reallocation-time `move_if_noexcept` behavior exactly right; see `PROJECT_ROADMAP.md` for the full statement once generated.
- **[P-1.4](../PROJECT_ROADMAP.md) Copy/Move Instrumentation Harness** — this chapter's Integration Challenge *is* effectively a hand-run pilot of this project; the project generalizes it into a reusable instrumentation library, and formally also depends on Ch01-02.
