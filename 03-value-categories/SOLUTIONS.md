# Chapter 03 — Solutions

> Do not read ahead of the problem you're checking. If you needed to read a solution to finish, mark that problem `◐ Assisted` in [`PROGRESS.md`](../PROGRESS.md), not `☑ Done`.

---

## Quick Check Answers

**03-QC1.** (1) Does the expression have identity (does it name a persistent entity you could refer to again)? (2) Can it be moved from? lvalue = identity, no-move; prvalue = no-identity, movable; xvalue = identity, movable.

**03-QC2.** No. It's guaranteed to invoke *some* constructor viable for an rvalue of type `T` — if `T` has an accessible move constructor, that's selected; if not (but a copy constructor exists), it silently falls back to copy. `std::move` only changes overload-resolution eligibility, not outcome.

**03-QC3.** `T` deduces to `int` (a prvalue argument deduces `T` as the non-reference type), so `T&&` becomes `int&&` — an rvalue reference.

**03-QC4.** No. `std::string` is a concrete, non-deduced type in this declaration — `s` is an ordinary rvalue reference, not a forwarding reference. Forwarding references require `T&&` where `T` is itself deduced at that call.

**03-QC5.** False. Mandatory elision means the copy/move constructor is **never called at all** — not called-then-optimized-away. The object is constructed directly in its final location from the start.

**03-QC6.** `return std::move(local);` turns the returned expression from "a plain named local" into a cast expression, which no longer qualifies for NRVO consideration (NRVO applies to returning a named automatic object directly) — the compiler is now forced down the move-construction path, when unadorned `return local;` might have let it elide entirely.

**03-QC7.** Because `x` is a named function parameter, and a named entity used by its own name is always an lvalue expression, regardless of what value category was used to *initialize* that parameter. Binding erases the category information; only `std::forward<T>(x)` (using the deduced `T`) can reconstruct it.

**03-QC8.** No. `f()`'s return type is `const std::string`, so the returned prvalue is const-qualified; a `const` rvalue does not bind to `std::string`'s non-const `T&&` move-constructor parameter — it falls back to the `const T&` copy constructor. `std::move` here is a no-op in effect (per Misconception 7): it still produces a copy.

---

## Problem Solutions

### 03-P01

**Reference Solution:**
(a) `x` — lvalue. (b) `42` — prvalue. (c) `std::move(x)` — xvalue. (d) `x + 1` — prvalue (the result of `operator+` on built-in types has no identity). (e) call to `int getValue();` — prvalue (returns by value). (f) call to `int& getRef();` — lvalue (returns an lvalue reference). (g) call to `int&& getRvalueRef();` — xvalue (returns an rvalue reference — has identity via the reference, but is movable).

---

### 03-P02

**Reference Solution:**
(a) `f(10)` — `10` is a prvalue → `T = int`, `x` is `int&&` (rvalue reference). (b) `f(n)` where `n` is an lvalue → `T = int&`, collapses to `x` being `int&` (lvalue reference). (c) `f(std::move(n))` — argument is xvalue → `T = int`, `x` is `int&&`. (d) `f(c)` where `c` is `const int` lvalue → `T = const int&`, `x` is `const int&`.

---

### 03-P03

**Reference Solution:** (a) `int& &` → `int&`. (b) `int& &&` → `int&`. (c) `int&& &` → `int&`. (d) `int&& &&` → `int&&`.

**Why It Works:** any presence of a single `&` in the collapse wins, unless both sides are `&&`.

---

### 03-P04

**Reference Solution:**
(a) `Widget b = a;` — copy constructor (`a` is an lvalue).
(b) `Widget c = std::move(a);` — move constructor (`std::move(a)` is an xvalue, and `Widget` has one).
(c) `Widget d = Widget();` — **neither**: `Widget()` is a prvalue initializing a same-type object, so mandatory elision applies — no copy or move constructor call occurs at all.
(d) `Widget e = make();` — same as (c): `make()`'s return is a prvalue used to initialize `e` directly; mandatory elision, no constructor call, regardless of whether `make`'s internal `return Widget();` itself also elides (it does, by the same rule, applied one level in).
(e) `f(Widget())` then `return w;` inside `f` — passing `Widget()` (prvalue) to a by-value parameter `w` is itself elision-eligible at the call boundary (the prvalue constructs `w` directly, no separate temporary+move) — so no constructor runs to create `w`. Returning `w` (a named parameter, i.e. a local) is the **NRVO case**, not mandatory elision — a compiler *may* elide the move here, and typically does, but this is optional; a correct answer must flag this call as unlike the earlier ones in guarantee level.

---

### 03-P05

**Reference Solution:** `v.push_back(w)` with `w` an lvalue copy-constructs the new element (there's no rvalue to move from — `w` itself is untouched). If this call also happens to trigger a **reallocation**, the *existing* elements already in `v` are relocated to the new buffer using `std::move_if_noexcept` internally — since in the first stated scenario the move constructor is `noexcept`, those existing elements are **moved**. In the second scenario (move constructor present but not `noexcept`, copy still available), `move_if_noexcept` falls back to **copying** the existing elements during relocation — different from `w`'s own construction (which is unconditionally a copy either way, since `w` was passed as an lvalue) for an unrelated reason: `w`'s copy is forced by its own value category, while the existing elements' copy is forced by the exception-safety fallback rule, not by their value category (they're being moved *from* known, addressable slots — they'd normally move).

---

### 03-P06

**Reference Solution:** Implementation-defined under C++14 (elision permitted but not mandatory — a compiler could legally copy/move here), but **guaranteed elided** under C++17 (`/std:c++17` or later) — this is exactly the mandatory-elision rule for a prvalue initializing a same-type object. Under `/std:c++14`, most real compilers still elide it as a QoI optimization, but it is not something you can rely on portably or assume will survive a `-O0`/Debug build the way the C++17 guarantee does.

---

### 03-P07

**Reference Solution:** NRVO is **not guaranteed** here even under C++17 (NRVO is never mandatory, unlike the prvalue case). This specific function is a materially harder case than a single-named-local/single-return version: it has **two different named locals** (`a` and `b`) returned along **two different paths**, which means the compiler would need to reserve the same return-value storage slot and have *both* branches construct directly into it — a legal but implementation-dependent optimization that not all compilers perform as reliably as the single-local case (some compilers do handle multi-path NRVO fine; the point is it's strictly weaker guarantee territory than the single-path case, and empirically less consistently applied).

---

### 03-P08

**Reference Solution:** Compiles, but **copies**, not moves. `make_const_vec()` returns `const std::vector<int>` by value — the resulting prvalue (and hence the xvalue `std::move` produces from it) is `const`. `std::vector<int>`'s move constructor takes a non-const `vector&&`; a `const vector&&` cannot bind to it, so overload resolution falls back to the copy constructor (`const vector&`), which does accept it. This is Misconception 7's edge case made concrete: `std::move` on a const object is a silent no-op with respect to actually moving.

---

### 03-P09 [DEBUG]

**Reference Solution:** Missing, all silently, as a direct consequence of only the move constructor being user-declared: the **destructor** (so `data_` is never freed by this class at all — a pure leak, on top of whatever else goes wrong), the **copy constructor and copy-assignment operator** (per Ch02's suppression rule, declaring *any* of destructor/copy-ctor/copy-assign suppresses the implicit generation of the *other* copy member and both move members — but here, uniquely, only the move constructor is declared, and a user-declared move constructor **also** suppresses the implicitly-generated copy constructor and copy-assignment operator entirely, per the same family of rules), and **move-assignment operator** (not generated either, since move-assignment generation is likewise suppressed once *any* of the five is user-declared). The compounding hazard: because copy is suppressed rather than merely defaulted-to-a-shallow-copy, any code path that *tries* to copy a `Buffer` fails to compile — which sounds safe, but the move constructor itself is buggy in a way copy-suppression doesn't protect against: it doesn't null out `other.data_`, so both the moved-from and moved-to `Buffer` hold the same pointer, and when *both* are eventually destroyed... except there's no destructor at all, so in fact `data_` simply leaks twice-over conceptually (never freed either time) rather than double-freeing — "seems to move fine" in testing because nothing crashes; it just leaks silently, which is worse to diagnose than a crash.

---

### 03-P10

**Reference Solution:** (a) `f(a)`, `a` lvalue non-const → `f(int&)`. (b) `f(5)`, prvalue → `f(int&&)`. (c) `f(c)`, `c` is `const int` lvalue → `f(const int&)` (neither `int&` nor `int&&` can bind to a const lvalue). (d) `f(std::move(a))`, xvalue → `f(int&&)`.

---

### 03-P11

**Reference Solution:** Yes, using `std::move(arg)` instead of `std::forward<T>(arg)` is a correctness bug for the first call (`wrapper(some_lvalue)`). `arg`, as a named parameter, is itself an lvalue expression regardless of what was passed to `wrapper` — but `std::move(arg)` unconditionally casts it to an rvalue reference before passing to `inner`, forcing `inner` to treat it as movable even though the original caller passed an lvalue they may still need afterward (e.g., `some_lvalue` might get moved-from inside `inner` when the caller never intended to give up ownership of it). `std::forward<T>(arg)` would correctly have produced an lvalue reference in this call (since `T` deduced as `Type&` for an lvalue argument), preserving the caller's original intent.

---

### 03-P12

**Reference Solution:** `g(s1)` — `s1` is an lvalue → copy-constructs the by-value parameter `s`. `g(std::move(s1))` — xvalue argument → move-constructs `s`. `g("literal")` — the string literal converts to a temporary `std::string` (a prvalue), which is elision-eligible: it constructs `s` directly from the literal via `std::string`'s converting constructor, no separate copy or move of a `std::string` occurs (only the one `std::string(const char*)` construction, directly into `s`'s storage).

---

### 03-P13

**Reference Solution:** Zero `Pair` copy/move constructions occur — `make()`'s `return {"x","y"};` is a prvalue of type `Pair` initializing `p` directly (mandatory elision). For the two `std::string` members: `a` and `b` are each constructed exactly once, directly from their respective string-literal arguments via `std::string`'s converting constructor — the aggregate-initialization inside `make()`'s return statement constructs `Pair`'s members in place in what becomes `p`'s storage, so there's no intermediate `Pair` temporary to copy/move members out of. Total: 2 `std::string` constructions (one per member, from literals), 0 `Pair` constructor calls beyond the aggregate init itself.

---

### 03-P14 [DEBUG]

**Reference Solution:** The move constructor copies `fd_`'s value into the new object but never resets `other.fd_` to an invalid sentinel (e.g. `-1`) — violating the specific clause of "valid but unspecified" that requires the moved-from object's *destructor to behave safely*. Here, the moved-from `Handle`'s destructor still sees the *same* `fd_` value the moved-to object now also holds, so `~Handle()` calls `close(fd_)` on it too — the concrete failure is a **double-close** (the file-descriptor analogue of a double-free): the same fd gets closed twice, which on POSIX systems is itself undefined-behavior-adjacent (the second close may silently fail, or — worse — the fd number may have already been reused by an unrelated `open()` call in the meantime, so the second `close()` closes a completely unrelated, currently-in-use file descriptor).

---

### 03-P15

**Reference Solution:** This is **not** a forwarding-reference overload because `T` is also used, undecorated, as `T&` in the *same* template's other parameter (`target`). Template parameter deduction for a single call must deduce a single, consistent `T` from *all* deduction contexts in that call — since `target` is `T&` (an ordinary reference, not itself universal), whatever is passed as `target` fixes `T` to a concrete non-reference type directly (an lvalue `T&` argument for `target` deduces `T` as that plain type, not `T&`). Once `T` is pinned this way by the first parameter, `value`'s `T&&` is simply an ordinary rvalue reference to that already-fixed type — not a forwarding reference, because forwarding-reference behavior specifically requires `T` to be deduced *from that same parameter's own argument*, which isn't happening here; `T` was already decided by `target`. Concretely, this means `value` can only bind to rvalues of exactly the type deduced from `target` — not "anything, category-preserved."

---

### 03-P16

**Reference Solution:** (a) `auto&& x = 42;` — compiles; `42` is a prvalue, so this behaves like a forwarding-reference deduction context: `x`'s type is `int&&`. (b) `auto&& x = some_lvalue_int;` — compiles; `some_lvalue_int` is an lvalue → `x`'s type is `int&`. (c) `auto&& x = std::move(some_lvalue_int);` — compiles; xvalue → `x`'s type is `int&&`. (d) `auto&& x = call_returning_int_ref();` — compiles; the call is an lvalue (returns `int&`) → `x`'s type is `int&`.

**Why It Works:** `auto&&` is itself a forwarding-reference-like deduction context (analogous to `T&&` in a template), which is exactly why it's the idiomatic way to write a range-based `for` loop variable that must bind to anything without forcing a copy.

---

### 03-P17

**Reference Solution:** `a(std::string("temp"))` passes a prvalue → inside `a`, `T` deduces as `std::string`, `x` is `std::string&&`. At **(1)**, `std::forward<T>(x)` with `T = std::string` (non-reference) casts to rvalue → `b` receives an rvalue, so inside `b`, `T` deduces as `std::string` again, `x` is `std::string&&`. At **(2)**, same reasoning → `c` receives an rvalue. At **(3)**, inside `c`, the object is bound as an rvalue reference the entire way through — the category (rvalue) is preserved at every hop specifically *because* `std::forward<T>` is used at each hop rather than a plain `x` or a `std::move`.

---

### 03-P18

**Reference Solution:** Yes, well-defined. Per the standard's specific guarantee for `std::vector`'s move constructor (not merely "unspecified state" — `vector` is one of the containers with an explicit guarantee), `v` is left **empty** (`v.size() == 0`) after being moved from. `push_back(4)` on an empty-but-valid vector is a perfectly ordinary, well-defined operation — it grows `v` to hold a single element, `4`.

---

### 03-P19

**Reference Solution:** With move present-but-not-`noexcept` and copy also accessible: `move_if_noexcept` selects the **copy constructor** for relocating existing elements during reallocation (preserving the strong exception-safety guarantee, per the Crash Course). If instead the copy constructor is `= delete`d (move still non-`noexcept`): `move_if_noexcept` is specified to fall back to using the move constructor anyway when no viable copy constructor exists, **even though it isn't `noexcept`** — because at that point, moving (with a small risk of a corrupted-on-exception intermediate state) is strictly better than failing to compile/relocate at all. So the prediction flips: non-`noexcept` move + no copy available → move is used regardless.

---

### 03-P20

**Reference Solution:**
```cpp
template<typename T>
constexpr T&& my_move(T&& t) noexcept {
    return static_cast<T&&>(t);
}
```
**Explanation:** the parameter must be a forwarding reference (`T&&` with `T` deduced), not a plain `T&`, because `my_move` must accept **both** lvalues and rvalues as input (you can call `std::move` on either an lvalue variable or an already-rvalue expression) — a plain `T&` would only bind to lvalues, making `my_move(std::move(x))` (moving an already-rvalue) fail to compile. The forwarding reference lets `T` deduce as `Type&` for an lvalue argument (collapsing the parameter to `Type&`) or as `Type` for an rvalue argument (parameter stays `Type&&`) — either way, the body's `static_cast<T&&>(t)` collapses to `Type&&` in both cases (since `Type& &&` collapses to `Type&`... wait — precisely: for an lvalue argument, `T = Type&`, so the return type `T&&` is `Type& &&` which collapses to `Type&`, **not** `Type&&`). This is exactly why `my_move`'s declared return type must literally be written as `T&&` (letting collapsing do its job across both deduction cases) rather than something that tries to hardcode "always rvalue reference" — the collapsing mechanics themselves guarantee the cast target ends up being an rvalue reference to the *unqualified* type only when `T` deduced as a non-reference (rvalue-argument case), and this is fine because for an lvalue argument the goal (per real `std::move`) actually still is to produce an rvalue reference to that lvalue's type — meaning the correct reasoning is: `T` deduced as `Type&` for an lvalue argument, and `T&&` still collapses to `Type&`... this shows the *naive* forwarding-reference-return pattern from `my_forward` is different from `my_move`'s actual required behavior, which is why real `std::move`'s signature is `template<typename T> constexpr std::remove_reference_t<T>&& move(T&& t) noexcept` — the `remove_reference_t` is essential specifically so the return type is unconditionally an rvalue reference regardless of whether `T` deduced as a reference. The corrected implementation:
```cpp
template<typename T>
constexpr std::remove_reference_t<T>&& my_move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}
```

**Common Wrong Approaches:** writing the return type as bare `T&&` (as in the first draft above) — this compiles for rvalue-argument calls but produces an lvalue-reference return for lvalue-argument calls, defeating `my_move`'s entire purpose exactly in the case (`my_move(some_lvalue)`) that matters most.

---

### 03-P21

**Reference Solution:**
```cpp
template<typename T>
constexpr T&& my_forward(T&& t) noexcept {
    return static_cast<T&&>(t);
}
```
**Explanation:** unlike `my_move`, `my_forward`'s bare `T&&` return type is *correct as written*, precisely because `std::forward`'s whole purpose is to reconstruct the *original* value category, and `T` alone already fully encodes that: `T` deduces as `Type&` when the original call-site argument was an lvalue (so `T&&` collapses to `Type&` — an lvalue reference, correctly reproducing the lvalue category) and `T` deduces as `Type` (non-reference) when the original argument was an rvalue (so `T&&` stays `Type&&` — correctly reproducing the rvalue category). No `remove_reference` is needed here because, unlike `my_move`, `my_forward` is not trying to force one specific category regardless of input — it's trying to *reflect back* whatever category `T`'s deduction already recorded, and reference collapsing applied to the undecorated `T&&` does exactly that.

**Why It Works:** the deduction that happened once, at the call to `my_forward` itself, already threw away no information — `T` being `Type&` versus `Type` is a complete, lossless encoding of "was it an lvalue or rvalue," and collapsing rules turn that encoding back into the right reference kind automatically.

---

### 03-P22 [DEBUG]

**Reference Solution:**
```cpp
template<typename T>
void log_and_call(T&& arg) {
    std::cout << "calling with arg\n";
    target_function(std::forward<T>(arg));   // fixed
}
```
**Scenario where behavior differs:** call `log_and_call(some_lvalue)` where `target_function` has two overloads, `target_function(const MyType&)` (reads, doesn't consume) and `target_function(MyType&&)` (consumes/moves-from). With the bug (`std::move(arg)`), `target_function`'s rvalue overload is always selected — even though the caller passed an lvalue they intend to keep using afterward — so `some_lvalue` gets silently moved-from inside `target_function`, and the caller's subsequent use of `some_lvalue` observes a moved-from (likely empty/invalid) object where they expected their original value to still be there. With the fix, an lvalue argument correctly selects `target_function`'s `const&` overload, leaving `some_lvalue` untouched.

---

### 03-P23

**Reference Solution:**
```cpp
class UniqueBuffer {
public:
    UniqueBuffer(size_t n) : data_(new int[n]), size_(n) {}
    ~UniqueBuffer() { delete[] data_; }

    UniqueBuffer(const UniqueBuffer&) = delete;
    UniqueBuffer& operator=(const UniqueBuffer&) = delete;

    UniqueBuffer(UniqueBuffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    UniqueBuffer& operator=(UniqueBuffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
private:
    int* data_;
    size_t size_;
};
```
**Self-move-assignment test:**
```cpp
UniqueBuffer a(10);
a = std::move(a);
```
**Explanation:** this specific implementation **needs** the `if (this != &other)` guard — without it, `delete[] data_;` would free the buffer, and then `data_ = other.data_;` (with `other` being the same object, `this`) would read `data_` *after* it was just freed and set to a garbage/dangling value in the general case (or, since `other` and `*this` are the same object, it would actually read the pointer that was just deleted, then immediately null it out afterward via `other.data_ = nullptr`, leaving `data_` pointing at freed memory momentarily then nulled — the exact sequence depends on evaluation, but the freed-then-reused pointer is the hazard regardless). The guard makes self-move-assignment a correct no-op instead.

---

### 03-P24

**Reference Solution:**
```cpp
template<typename... Args>
auto make_thing(Args&&... args) {
    return std::make_unique<Thing>(std::forward<Args>(args)...);
}
```
**Explanation:** each element of the parameter pack `Args` is deduced independently, per-argument, following the same single-argument forwarding-reference rule: `lvalue_arg` (an lvalue) deduces its corresponding `Args_i` as `Type&`; `42` (a prvalue) deduces its `Args_i` as `int`; `std::move(other_lvalue)` (an xvalue) deduces its `Args_i` as `Type` (non-reference, since it's an rvalue argument). `std::forward<Args>(args)...` then expands to forward each one according to its own independently-deduced category — the pack mechanism doesn't force any uniform treatment across arguments; each slot's forwarding is fully independent.

---

### 03-P25

**Reference Solution:**
```cpp
template<typename T>
void my_swap(T& a, T& b) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}
```
**Explanation:** if `T`'s move constructor/assignment can throw and does so partway through (e.g., during `a = std::move(b);`), `my_swap` provides **no exception-safety guarantee at all** — `a` may be left half-modified, `tmp` still holds the original `a`'s value, and `b` is untouched, an inconsistent intermediate state with no rollback. If `T`'s move operations are `noexcept`, `my_swap` is trivially exception-safe (nothing in it can throw, so there is no partial-completion state to worry about) — this is precisely why `std::swap`'s own guarantees, and many algorithms that rely on `swap` internally, are conditioned on `is_nothrow_move_constructible`/`is_nothrow_move_assignable`.

**Complexity:** O(1) additional space (`tmp`), three move operations regardless of `T`'s size (assuming O(1) moves, i.e. not a type whose "move" is secretly an O(n) copy due to missing/expensive move support).

---

### 03-P26

**Reference Solution:**
```cpp
class UniqueBuffer {
public:
    // ... as in 03-P23 ...
    UniqueBuffer clone() const {
        UniqueBuffer copy(size_);
        std::copy(data_, data_ + size_, copy.data_);
        return copy;
    }
};
```
**API-design reasoning:** a public copy constructor makes copying *implicit* — any code that writes `UniqueBuffer b = a;` copies without necessarily intending the (potentially expensive, and semantically loaded — "do I actually want two independent buffers?") duplication; the copy happens silently at every call site that merely looks like ordinary initialization. An explicit `clone()` makes the decision to duplicate **visible and deliberate** at the call site — a reader sees `.clone()` and immediately knows an independent copy was intentionally requested, versus reading `UniqueBuffer b = a;` and having to check the class definition to know whether that's a cheap move or an expensive hidden copy. This is a real, common pattern for deliberately move-only-but-still-duplicable resource-owning types.

---

### 03-P27

**Reference Solution:**
```cpp
class Store {
public:
    void process(const std::vector<int>& v) {
        working_copy_ = v;          // (A): must copy, doesn't own v
    }
    void process(std::vector<int>&& v) {
        working_copy_ = std::move(v);  // (B): steals v's buffer
    }
private:
    std::vector<int> working_copy_;
};

Store s;
std::vector<int> persistent = {1,2,3};
s.process(persistent);                       // must call (A) — persistent is an lvalue still needed after
s.process(std::vector<int>{4,5,6});          // must call (B) — a prvalue temporary, nothing else references it
std::vector<int> disposable = {7,8,9};
s.process(std::move(disposable));            // caller forces (B) — disposable is no longer needed, opts in explicitly
```

---

### 03-P28 [DEBUG]

**Reference Solution:** `get_ref_to_temp()`'s call expression has value category **lvalue** (it returns `std::string&`), but critically, *what it refers to* — the local `s` inside the function — is an automatic-storage-duration object whose lifetime ends when the function returns, regardless of the reference's own category. Binding `std::string& r = get_ref_to_temp();` (a **named non-const lvalue reference**) to that expression does **not** trigger temporary lifetime extension — lifetime extension specifically applies to binding a reference *directly to a temporary* (a prvalue/materialized temporary) at the point of initialization, which is not what's happening here at all: there's no temporary being created by this call; there's a *named local going out of scope*, an entirely different (and always-dangling) situation. The actual defect is inside `get_ref_to_temp()` itself: it returns a reference to a local variable, full stop — no reference-binding rule anywhere rescues this; the function is simply wrong to write, independent of how its result is subsequently bound.

---

### 03-P29 [DEBUG]

**Reference Solution:** `r` is **not** dangling here, and this is a materially different situation from 03-P28. `forward_wrong` takes `T&& x` (a forwarding reference bound to the caller's temporary `std::string("temp")`), and returns `x` — since `x`'s declared type is `T&&` and the function's return type is also (implicitly, per the template) `T&&`, this returns a reference to the **same temporary object the caller created and is still alive in the caller's full expression** — it does not return a reference to a function-local variable the way 03-P28 does. Per the standard's temporary lifetime rules, `std::string("temp")` (the caller's original temporary) persists until the end of the full expression containing the call `forward_wrong(std::string("temp"))` — and `auto&& r = forward_wrong(...)` binds `r` within that same full expression, so `r` is a valid reference to a still-living temporary for the remainder of that statement and beyond (the binding itself, to a reference, doesn't extend it further, but the temporary's *natural* lifetime already covers the point of binding and use here since there's no intervening scope exit). Returning `T&&` from a function taking a forwarding reference is not automatically buggy; what *would* make it a bug is if `x` referred to something whose lifetime ends before the return value is used — e.g., if `forward_wrong` had instead taken its argument, copied it into a genuinely function-local object, and returned a reference to *that* local (converging back to exactly 03-P28's shape). The extra ingredient that turns this pattern dangerous is a function-local intermediate object standing between the parameter and the return, not the mere presence of a `T&&`-returning forwarding function.

---

### 03-P30 [DEBUG]

**Reference Solution:** The next time the moved-from `Matrix` is destroyed, its destructor (implicitly generated or, if written, presumably `delete[] data_;`) operates on `data_`, which still points at the **same memory the moved-to `Matrix` also owns** — this is a double-free the moment both objects are eventually destroyed (or a double-free-then-use if the moved-from object is move-assigned-from *again* before destruction, since a correct move-assignment would first `delete[]` its own current `data_` — which is the same pointer already freed once by the other object's destruction if that happened first). The violated clause is specifically: a moved-from object must remain **valid** — meaning its destructor (and reassignment) must be safe to invoke — and here it is not, because "skip the null-check branch" removed exactly the piece of state (`nullptr`) that would have made the moved-from object's *own* destructor a safe no-op. This is not about the *value* being unspecified (the rationale conflates "we don't promise what value you'll read" with "we don't promise the object is even safely destructible," which are different guarantees — only the first is optional to define loosely; the second is mandatory).

---

### 03-P31 [DEBUG]

**Reference Solution:** Most likely cause: `T`'s move constructor exists but is **not marked `noexcept`** (and `T` still has an accessible copy constructor) — `std::move_if_noexcept`, used internally by standard-library-style reallocation logic (and presumably replicated in this hand-written `Vector<T>`), falls back to copying specifically to preserve the strong exception-safety guarantee when the move constructor could throw. The one-line fix/diagnostic: add `noexcept` to `T`'s move constructor's declaration (`T(T&&) noexcept { ... }`) — if this flips the benchmark from O(n) copies to O(n) moves per reallocation, it confirms the diagnosis exactly.

---

### 03-P32 [DEBUG]

**Reference Solution:** `v[0]` (via `operator[]`) returns `std::unique_ptr<int>&` — an **lvalue** reference to the element. `auto p = v[0];` therefore attempts to **copy-construct** `p` from an lvalue `unique_ptr<int>`, but `unique_ptr`'s copy constructor is deleted (by design — exclusive ownership), so line (a) fails to compile, precisely because the *value category* of `v[0]` (lvalue) rules out move construction and only leaves the (deleted) copy path available. Line (b), `auto p2 = std::move(v[0]);`, explicitly casts that same lvalue expression to an xvalue, making the (existing) move constructor viable — this compiles and move-constructs `p2`, transferring ownership of the underlying `int*`. Afterward, `v[0]` holds a `unique_ptr<int>` in its moved-from state — per `unique_ptr`'s standard-guaranteed moved-from state — **null** (equivalent to holding `nullptr`), so `v[0]` remains a valid, safely-destructible/reassignable `unique_ptr` element, just an empty one.

---

### 03-P33 [DEBUG]

**Reference Solution:** `std::forward<T>(item)` conditionally moves from `item` if the original caller-supplied argument was an rvalue (i.e., `T` deduced as a non-reference type) — in that case, `buffer_.push_back(...)` may have moved-from `item`, leaving it in a valid-but-unspecified state, so the subsequent `log(item)` reads an object whose content is no longer guaranteed to reflect what was originally passed — this is a real bug (reading a moved-from object's value and expecting it to still be meaningful) specifically when the caller passed an **rvalue**. When the caller instead passed an **lvalue**, `T` deduces as a reference type, `std::forward<T>(item)` produces an lvalue reference, `push_back` copy-constructs (not moves) from it, and `item` is left completely untouched — in that case, `log(item)` afterward is entirely correct and not a bug, merely relying on the caller-argument-category-dependent fact that no move occurred. The reviewer's flag is valid as a general pattern (using a possibly-forwarded-from parameter again is fragile/context-dependent), even though it isn't wrong in *every* call.

---

### 03-P34 [DEBUG]

**Reference Solution:** Adding the user-declared destructor at `(b)` **suppresses the implicit generation of the move constructor and move-assignment operator** (Ch02's suppression rule, applied here) — but `(a)`, `Node(Node&& other) = default;`, is *itself* a user-declaration of the move constructor, so it survives explicitly (an explicitly-defaulted declaration still counts as present, it just gets compiler-generated *contents*) — however, this reasoning needs a second look: the move constructor is explicitly defaulted at `(a)`, so it is **not** suppressed (suppression only applies to members the class doesn't itself declare) — meaning `Node`'s move constructor continues to exist and move `payload` correctly. The actual observable effect of adding `(b)` is instead on the **move-assignment operator**, which was never declared anywhere in this class — its implicit generation *is* suppressed by the newly-added user-declared destructor. Any code doing `n2 = std::move(n1);` (move-*assignment*, distinct from `Node n2 = std::move(n1);` which is move-*construction* and still works via `(a)`) now falls back to whatever copy-assignment is available — which, since copy-assignment is *also* implicitly suppressed by the same destructor-declaration rule (unless independently defaulted, which it isn't here), means **move-assignment via `=` no longer compiles at all** for `Node` — not merely a silent performance regression to a copy, but a hard compile error at the first `n2 = std::move(n1);` call site, which is the actual, concrete, discoverable-at-compile-time consequence here (a stricter and more diagnosable outcome than the "silent regression" framing might suggest — worth noting explicitly since it means this bug, unlike several others in this chapter, cannot ship silently).

---

### 03-P35

**Reference Solution:**
```cpp
class Logger {
public:
    explicit Logger(const std::string& path) : out_(path) {}

    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;
    // Copy operations: not explicitly declared here.
private:
    std::ofstream out_;
};
```
**Justification for not writing deleted copy stubs:** `std::ofstream` has no copy constructor (streams are inherently non-copyable), so — per the same reasoning as 02-P26 — an implicitly-generated `Logger` copy constructor would need to copy-construct `out_`, which is ill-formed; the compiler therefore does not generate `Logger`'s copy constructor/assignment at all, automatically, with no need for an explicit `= delete`. Explicitly deleting them anyway is not wrong, but is redundant here — it can still be worth doing for documentation/clarity, but it is not load-bearing the way it was in 02-P11/02-P12's raw-handle cases (where copy would otherwise have compiled, shallowly and dangerously, without an explicit delete).

**Factory and elision reasoning:**
```cpp
Logger make_logger(const std::string& path) { return Logger(path); }
Logger l = make_logger("out.log");
```
`Logger(path)` inside `make_logger` is a prvalue initializing the function's return value — mandatory elision (C++17) constructs it directly in the eventual return slot; `make_logger("out.log")`'s result, itself a prvalue, then directly initializes `l` — also mandatory elision. **Zero `Logger` constructor calls beyond the single `Logger(const std::string&)` call** occur — this is guaranteed (not merely typical) under C++17's mandatory elision rules, precisely because every step in this chain is a prvalue-initializing-same-type-object, not a named-local return (which would only be NRVO, optional).

---

### 03-P36

**Reference Solution:**
```cpp
template<typename T, typename... Args>
T make_and_log(Args&&... args) {
    std::cout << "constructing " << typeid(T).name() << "\n";
    return T(std::forward<Args>(args)...);
}
```
**Trace:** arguments `1, 2, 3` forward into `T`'s constructor unchanged (each is a prvalue int literal, deduced/forwarded as such) — `T(std::forward<Args>(args)...)` constructs a `T` prvalue directly from them, no intermediate copy. This prvalue is the expression in `make_and_log`'s `return` statement, initializing the function's return value — mandatory elision applies (prvalue of type `T`, matching return type). At the call site, `T obj = make_and_log<T>(1, 2, 3);` again takes `make_and_log`'s prvalue result and initializes `obj` directly — mandatory elision again. Net result: exactly **one** call to `T`'s constructor (the one taking `1, 2, 3` after forwarding) occurs; zero copy or move constructions of `T` happen anywhere in the chain, guaranteed by the C++17 mandatory-elision rule applied twice (once inside `make_and_log`, once at the call site).

---

### 03-P37

**Reference Solution:**
```cpp
LegacyBuffer make_moved_like(LegacyBuffer& src) {
    LegacyBuffer result(src);   // copy — LegacyBuffer has no move constructor to use
    src = LegacyBuffer{};       // reset src to a default/empty state, simulating "moved-from"
    return result;
}
```
**Limits:** you **cannot** make `std::move(legacy_buffer_instance)` itself actually invoke a move — `std::move` only changes the value category of the expression; it's overload resolution on `LegacyBuffer`'s *own* constructor set that decides what happens next, and since `LegacyBuffer` has no move constructor, `std::move`-casting an instance of it and using it to initialize another `LegacyBuffer` still resolves to the copy constructor (the same "no-op move" mechanism as Misconception 7/03-P08, but here for a structural reason — no move ctor exists — rather than const-qualification). What you *can* do, as shown above, is simulate the *effect* at your own call sites: perform the (unavoidable) copy, then explicitly reset the source to represent "given up" — but this is a copy-plus-manual-reset happening at your call site, not an actual move happening inside `LegacyBuffer` itself, and it costs exactly what a real copy costs (no performance benefit — only the ownership-transfer *semantics* are approximated, not the efficiency).

---

### 03-P38

**Reference Solution:**
```cpp
template<typename F>
class AnyCallable {
public:
    explicit AnyCallable(F f) : f_(std::move(f)) {}

    template<typename... Args>
    auto operator()(Args&&... args) {
        return f_(std::forward<Args>(args)...);
    }
private:
    F f_;
};
```
**What goes wrong with `Args...` (by value) instead of `Args&&...`:** every call argument would be copy- or move-constructed into `operator()`'s by-value parameters *before* being passed on to `F`'s own `operator()` — this forces a category collapse to "always an lvalue-like owned local" at this layer, meaning: (1) an lvalue argument that the wrapped callable expected to receive by `const&` (to avoid a copy) now gets copied twice — once into `AnyCallable::operator()`'s parameter, once again if `F`'s call operator itself takes by value — and (2) more importantly, an rvalue argument that the wrapped callable's `operator()` overload set specifically wanted to *move from* (an `F::operator()(SomeType&&)` overload) would never be selected, because by the time the by-value `Args...` parameter is passed along, it's a named local — an lvalue — exactly the same category-erasure bug pattern as passing a named parameter along without `std::forward` in 03-P11/03-P22.

---

### 03-P39

**Reference Solution:** The justification ("caller usually has a temporary anyway") is true specifically when call sites look like `process(build_events());` — a prvalue argument binding directly to the by-value parameter, mandatory-elision-constructing it with zero extra copies/moves versus the old `const&` signature (which would have bound the temporary with no copy either, but then the function body would need its own internal copy if it wanted to keep the data — the by-value signature shifts that unavoidable-if-needed copy to the *call boundary*, where it can be elided, rather than inside the function body, where it can't be). It **silently regresses** for call sites like `std::vector<Event> persistent_events = ...; process(persistent_events);` — here `persistent_events` is an lvalue the caller still needs, so it must be **copied** into the by-value parameter (there's no rvalue to elide/move from), whereas the old `const&` signature would have taken a reference with **zero copies** for this exact call shape. The recommendation: this is a genuine trade-off, not a strict improvement — prefer **two overloads** (`process(const std::vector<Event>&)` and `process(std::vector<Event>&&)`, per the pattern in 03-P27) if both call shapes are common and the function can meaningfully take advantage of ownership in the rvalue case; if the function never actually needs to *keep* the data (just reads it), the by-value change was never justified in the first place and `const&` should be kept unconditionally; a single generic forwarding-reference template is usually *not* appropriate here specifically because `process` is described as a concrete, non-generic hot-path function — introducing a template for this narrows nothing and complicates overload visibility/error messages for no benefit over the two-overload approach.

---

### 03-P40

**Reference Solution:** The claim is true specifically when `identity` is called with an **rvalue** argument whose type is expensive to copy/move-construct *and* whose value category the caller was relying on being preserved through to whatever consumes `identity`'s result directly in the same expression. Diagnosis: `identity`'s return type is `T` (by value, not `decltype(auto)` or `T&&`) — for an rvalue-argument call, `T` deduces as the non-reference type, so `std::forward<T>(x)` inside produces an rvalue used to initialize the **return value** — this construction is a genuine move (not free), and it happens *in addition to* whatever the caller does with `identity`'s result afterward, whereas directly using the original expression (without routing it through `identity`) would have let mandatory elision or a direct move happen with one fewer hop. This is a real (if usually small) extra move introduced by `identity`'s by-value return, not a copy in the pessimistic case (rvalue in → move out), but it is a full extra copy specifically when the input was an **lvalue** the caller expected to be usable, category-preserved, afterward — since `T` deduces as a reference type for an lvalue argument, but the return type `T` in a function declared as returning `T` (a template parameter that might be a reference) is not straightforwardly well-formed either; assuming the realistic implementation returns `std::remove_reference_t<T>` by value (as it must, to be a sensible by-value return), an lvalue argument then forces a full **copy construction** into the return value, which is strictly worse than the caller having used the lvalue directly. **Proposed fix:** change the return type to `decltype(auto)` (`decltype(auto) identity(T&& x) { return std::forward<T>(x); }`) — this makes `identity` return exactly the reference type that `std::forward<T>(x)` produces (an lvalue reference for lvalue-argument calls, an rvalue reference for rvalue-argument calls), eliminating the extra copy/move entirely for both cases. **New hazard reintroduced:** returning a reference type from a function raises exactly 03-P29's dangling-reference question — but making the actual determination here: it does **not** apply in this specific case, because `identity` returns a reference to its *own parameter* `x`, which is itself a reference to whatever the caller passed — the returned reference's validity is exactly as long as the caller's original argument's validity, with no function-local intermediate object interposed (unlike 03-P28's local-variable case, and matching 03-P29's safe pattern exactly) — so for `identity` specifically, `decltype(auto)` is safe and strictly preferable to the by-value `T` return, with the caveat (worth stating explicitly, not glossing over) that this reasoning depends entirely on `identity` never introducing an intermediate local of its own, which is true today but would need re-verification if `identity`'s body ever grew beyond a single forwarding return.

---

## Integration Challenge Solution — 03-IC1

**Reference Solution / Predictions and Verification:**

1. `Derived d2 = d1;` (d1 is an lvalue) → **`Derived`'s copy constructor** runs once, which (per the implicit base-first rule) invokes **`Base`'s copy constructor** first, then copies `extra_`. Predicted and actual: 2 constructor calls total (`Base(const Base&)`, then `Derived(const Derived&)`'s own body/member-init).

2. `Derived d2 = std::move(d1);` → **`Derived`'s move constructor** runs once, invoking `Base`'s move constructor for the base part, then move-constructing `extra_`. 2 calls total (`Base(Base&&)`, `Derived(Derived&&)`).

3. `Derived make() { Derived d; return d; }` then `Derived d3 = make();` → returning named local `d` is the **NRVO case** (optional) — on virtually all mainstream compilers at normal optimization levels, NRVO elides this entirely: `d` is constructed directly in `d3`'s storage, **zero** copy/move constructor calls. This is the prediction most likely to be *wrong* if verified under `-O0`/Debug-without-optimizations, where some compilers don't apply NRVO — in that case, one move construction (`Derived(Derived&&)`) occurs instead. Correct prediction must flag this as non-guaranteed, unlike cases 1 and 2.

4. `v.reserve(1); v.push_back(Derived());` (fills capacity exactly) then a second `push_back` forcing reallocation → the first `push_back(Derived())`: prvalue argument, constructs in place, **zero** extra `Derived` copy/move (mandatory elision at the `push_back(Derived&&)` parameter binding — the prvalue temporary binds to the rvalue-reference parameter with no separate object created beyond what's needed, then that's used to construct the vector's stored element via `Derived`'s move constructor exactly once — so 1 move constructor call, not zero, since binding an rvalue reference parameter to a prvalue and then constructing a *separate* vector element from it is a genuine construction, not the same-object elision seen in direct initialization). The second `push_back` triggering reallocation: the *existing* stored `Derived` is relocated via `move_if_noexcept` — since both `Base` and `Derived`'s move constructors are declared `noexcept` here, this resolves to an actual **move**, 1 more `Derived` move constructor call (itself invoking `Base`'s move constructor for the base part).

5. `consume(Derived())` (prvalue argument to by-value parameter) → mandatory elision at the call boundary, constructs `d` in place from the prvalue with no extra copy/move beyond `Derived()`'s own initial construction — **zero** additional `Derived` copy/move calls. `Derived d; consume(d);` (lvalue argument) → **copy constructor**, 1 call. `consume(std::move(d))` (xvalue argument) → **move constructor**, 1 call.

**Where predictions commonly go wrong:** case 3 is the one most likely to surprise a first-time predictor who conflates NRVO with the C++17 mandatory-elision guarantee (Misconception 3) — the fix is recognizing that "returning a named local" and "a prvalue initializing a same-type object" are categorically different elision situations with different guarantee strength, even though both are colloquially called "elision." Case 4's first `push_back` is the second-most common misprediction — it's easy to assume "prvalue argument means zero constructor calls anywhere," but the vector's *stored* element is a genuinely separate object from the temporary bound to the parameter, so exactly one move construction is unavoidable there, distinct from the zero-call outcome in case 5's `consume(Derived())`, where the parameter itself *is* the only object (no separate container-owned storage to move into).
