# Chapter 02 — Solutions

> Do not read ahead of the problem you're checking. If you needed to read a solution to finish, mark that problem `◐ Assisted` in [`PROGRESS.md`](../PROGRESS.md), not `☑ Done`.

---

## Quick Check Answers

**02-QC1.** No. Declaring a destructor (and nothing else) suppresses the implicit generation of the move constructor and move-assignment operator entirely — they simply don't exist. The copy constructor and copy-assignment operator are still implicitly generated in this case (though deprecated by the standard when a destructor is user-declared), so a copy happens where a move might have been expected.

**02-QC2.** Base classes construct first (in the order they're listed in the base-specifier list, not necessarily the order used in a derived constructor's initializer list), then members construct in **declaration order** (not initializer-list order). Destruction is the exact reverse.

**02-QC3.** An idiom, not a language feature. It rests entirely on one actual language guarantee: automatic-duration local objects are destroyed at scope exit, including via the exception path.

**02-QC4.** No. `weak_ptr<T>` has no `operator->` or `operator*` — you must call `.lock()` to obtain a (possibly empty) `shared_ptr<T>` first, and check that before use.

**02-QC5.** One. `make_shared<T>(...)` allocates the control block and the `T` object together in a single allocation, which is precisely the efficiency argument for preferring it over `shared_ptr<T>(new T(...))` (which allocates the object, then a separate control block).

**02-QC6.** No. A stateful custom deleter is stored inside the `unique_ptr` object itself, so `sizeof(unique_ptr<T, MyStatefulDeleter>)` can exceed `sizeof(T*)`. Only a stateless deleter (the default `std::default_delete<T>`, or a captureless-lambda/empty-functor deleter) reliably keeps it pointer-sized in practice.

**02-QC7.** Yes. If a constructor's body (or a later member initializer) throws, every base class and member that has *already finished* constructing is destroyed, in reverse order of construction, before the exception propagates out of the failed constructor. Only the not-yet-constructed remainder is left unconstructed (and hence not destroyed, since it never existed).

**02-QC8.** Storage duration is about when the underlying memory exists (automatic/static/thread/dynamic, from Ch01). Lifetime is about when a *specific object* actually exists within that storage — beginning when its initialization completes, ending when its destructor begins. Storage can exist without a live object in it (e.g. an empty `std::optional<T>`'s internal buffer).

---

## Problem Solutions

### 02-P01

**Approach:** Trace the delegating constructor call.

**Reference Solution:** `A()` delegates to `A(0)`, so `x_ == 0`.

**Why It Works:** delegating constructors (C++11) let one constructor call a peer constructor of the same class as its entire initialization, rather than duplicating logic.

---

### 02-P02

**Approach:** Destruction order is the exact reverse of construction order (base, then members in declaration order).

**Reference Solution:** `~D()`'s body runs first, then `m`'s destructor, then `B`'s destructor — reverse of construction (`B` constructs, then `m`, then `D()`'s body runs).

---

### 02-P03

**Approach:** Apply the rule from QC1: a user-declared destructor suppresses implicit generation of move members.

**Reference Solution:** **Suppressed.** `Resource` declares a destructor, so the compiler does not implicitly generate a move constructor for it — attempting to move a `Resource` falls back to copy (if the copy constructor is still available, which it is here since only the destructor was user-declared) rather than actually moving.

**C++ Considerations:** this class is a textbook Rule-of-5 violation in progress — it manages a raw `int* data` with a destructor but no copy/move members written, meaning the implicitly-generated copy constructor will shallow-copy `data`, setting up a double-free identical to 02-P29's.

---

### 02-P04

**Approach:** Distinguish "compiles" from "safe to run."

**Reference Solution:** Well-formed at compile time — `operator->` on `unique_ptr` is a normal member function call, syntactically fine regardless of whether the pointer is null. **Not safe to execute** — dereferencing a null `unique_ptr` (via `->` or `*`) is undefined behavior, exactly analogous to dereferencing a null raw pointer.

---

### 02-P05

**Approach:** Copying a `shared_ptr` increments the shared refcount.

**Reference Solution:** Reference count is **2** after `b = a;`. It's decremented when either `a` or `b` is destroyed or reset, and the underlying `int` is destroyed only when the count reaches 0.

---

### 02-P06

**Approach:** `weak_ptr` observes without extending lifetime; `lock()` after the observed object is destroyed returns empty.

**Reference Solution:** Legal. `w` observes `sp`'s managed `int`, but does not keep it alive — when the inner block ends, `sp` is destroyed, the refcount reaches 0, and the `int` is destroyed. `w.lock()` afterward returns an **empty** `shared_ptr<int>` (not a dangling one) — `weak_ptr` correctly detects that its target no longer exists.

---

### 02-P07

**Approach:** Compare initializer-list *order written* against declaration order.

**Reference Solution:** `hi_(hi)` is written before `lo_(lo)` in the initializer list, but members initialize in **declaration order** (`lo_` first, then `hi_`) regardless of list order — most compilers emit a `-Wreorder` warning here. In this specific example there's no actual bug (neither initializer depends on the other), but the pattern is flagged because it's one edit away from a real one — see 02-P08 for the case where it actually breaks something.

---

### 02-P08 [DEBUG]

**Approach:** `size_` is declared *after* `data_`, so it initializes after `data_`, regardless of the initializer-list's written order.

**Reference Solution:** `data_(new int[size_])` runs **before** `size_(n)` has executed, because `data_` is declared before `size_` in the class body — so `new int[size_]` reads `size_`'s indeterminate pre-initialization value (undefined behavior; often a garbage large number, causing an intermittent crash or huge allocation, but sometimes accidentally "working" if the garbage happens to be small).

**Fix:** either reorder the *declarations* so `size_` precedes `data_`, or don't depend on one member's initializer to read another's value at all — take `n` directly: `Buffer(int n) : data_(new int[n]), size_(n) {}`.

**Why It Works:** this is 02-P07's warning made real — declaration order, not list-written order, governs initialization order, unconditionally.

---

### 02-P09

**Approach:** Determine whether `g_logger`'s construction in TU1 is guaranteed to precede `g_startup_code`'s dynamic initializer in TU2, which reads it.

**Reference Solution:** Compiles (assuming proper linkage/declaration), but the **relative order of dynamic initialization of namespace-scope objects across translation units is unspecified**. If TU2's dynamic initializers run before TU1's, `g_logger.get_status()` is called on a not-yet-constructed `Logger` — undefined behavior. This is the static-initialization-order fiasco named in the Crash Course.

**Fix (conceptually, formalized more fully once you have all the tools):** avoid cross-TU dynamic-initialization dependencies; use a function-local `static` (lazy-initialized on first use, guaranteed-safe per C++11) instead of a plain namespace-scope global with a non-trivial constructor.

---

### 02-P10

**Approach:** `lock_guard`'s destructor runs during stack unwinding just like any other local's.

**Reference Solution:** `m` is correctly **unlocked** even though the exception propagates out of `f()` — `lock_guard`'s destructor (which calls `m.unlock()`) runs during unwinding, exactly like any RAII type's destructor, regardless of *why* the scope is being exited.

**Why It Works:** this is RAII's entire value proposition demonstrated directly — the unlock isn't a `catch`+cleanup path someone had to remember to write; it's structurally guaranteed by scope exit.

---

### 02-P11

**Approach:** Deleting the copy constructor/assignment doesn't suppress move; only *declaring* (including `= delete`) affects generation rules per-category. But no destructor and no move members are written at all here.

**Reference Solution:** Copy constructor and copy-assignment: user-declared (as deleted) — not generated, explicitly unusable. Destructor: implicitly generated (trivial, since `int fd_` needs no special cleanup at the language level — though this is itself a design smell, since `fd_` is presumably meant to be closed). Move constructor and move-assignment: **still implicitly generated**, because deleting the *copy* members doesn't suppress move generation the way declaring a *destructor* does — the rule that suppresses move generation is specifically about a user-declared destructor, copy constructor, or copy-assignment operator being present as a genuine declaration, and per the standard, an explicitly-deleted function still counts as "user-declared" for this purpose... which means the move members here are actually **also suppressed**, because both copy members are user-declared (as deleted).

**Why It Works:** this is the correction of a subtle trap — "deleted" still counts as "declared" for the purpose of suppressing other special members' implicit generation. `Handle` in this exact form is neither copyable nor movable; see 02-P12 for the direct continuation of this exact scenario.

---

### 02-P12 [DEBUG]

**Approach:** Continue directly from 02-P11's correction — user-declaring the copy members (even as deleted) suppresses implicit move generation, and a user-declared destructor does too.

**Reference Solution:** `Connection` is intended non-copyable-but-movable, but as written it is **neither copyable nor movable**. Two independent reasons compound: (1) the copy constructor and copy-assignment operator are explicitly deleted, which counts as user-declaring them — and (2) the destructor is also user-declared. Either alone suppresses implicit generation of the move constructor/move-assignment operator; here both are present, so there's no ambiguity that move generation is fully suppressed. Attempting `Connection b = std::move(a);` falls back to trying to copy — which fails to compile, since copy is deleted — so the class is simply immovable, not "movable via a slow copy fallback."

**Fix:** explicitly write (or `= default`) the move constructor and move-assignment operator:
```cpp
Connection(Connection&& other) noexcept : socket_fd_(other.socket_fd_) { other.socket_fd_ = -1; }
Connection& operator=(Connection&& other) noexcept { /* ... */ return *this; }
```

---

### 02-P13

**Approach:** Copying an empty (default-constructed) `shared_ptr` yields another empty one.

**Reference Solution:** `b` is also empty (`nullptr`-equivalent), and the refcount is unaffected — there's no control block to increment a count in, since `a` never owned anything. `b`'s use count is `0`.

---

### 02-P14 [DEBUG]

**Approach:** `weak_ptr` has no dereference operator.

**Reference Solution:** Does **not** compile. `weak_ptr<T>` has no `operator*`, precisely to prevent exactly this pattern — dereferencing without first checking whether the observed object still exists. The fix is `if (auto locked = w.lock()) { int v = *locked; }`.

---

### 02-P15

**Approach:** Trace base/member construction order, then reverse it for destruction.

**Reference Solution:** Output, in order:
```
Inner()      (a constructs)
Inner()      (b constructs)
Outer()      (Outer's body runs)
~Outer()     (Outer's body runs on destruction)
~Inner()     (b destructs — reverse declaration order)
~Inner()     (a destructs)
```

---

### 02-P16 [DEBUG]

**Approach:** Trace what happens if `connect()` throws — but note it's declared to return `bool`, not throw, so re-examine the actual failure path.

**Reference Solution:** As written, `connect()` returns `bool` and doesn't throw — the `throw` happens in `Session`'s own constructor body, *after* `buffer_` has already been fully constructed (the member-initializer `buffer_(new char[1024])` completed before the constructor body even starts running). Since `buffer_` finished constructing before the throw, and it's a member (not a base) of the object under construction, its destructor... **does not run**, because the *object being constructed* (`Session` itself) never finishes construction — and a not-fully-constructed object's destructor is never invoked. Only fully-constructed bases/members are destroyed during unwind of a failed constructor; `buffer_` is a fully-constructed `char*` value, but `char*` has no destructor to run (it's not RAII — it's a raw pointer). The leak occurs precisely because `buffer_` is a **raw pointer member**, not an RAII-owning type: there's no destructor to call on it during the unwind, so the 1024 bytes it points to are never freed.

**Fix:** wrap `buffer_` in `std::unique_ptr<char[]>` (or `std::vector<char>`), so that even though `Session`'s own constructor never completes, the already-constructed `unique_ptr` member's destructor *does* run during unwind (as established in QC7) and frees the buffer correctly.

---

### 02-P17

**Approach:** Move-only RAII wrapper: disable copy, implement move as a transfer with the source left in a well-defined "no resource" state.

**Reference Solution:**
```cpp
class ScopedFd {
public:
    explicit ScopedFd(int fd) noexcept : fd_(fd) {}
    ~ScopedFd() { if (fd_ >= 0) ::close(fd_); }

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    ScopedFd& operator=(ScopedFd&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) ::close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const noexcept { return fd_; }
private:
    int fd_ = -1;
};
```

**Why It Works:** `-1` is the conventional POSIX "invalid fd" sentinel; leaving a moved-from `ScopedFd` holding it makes its destructor a safe no-op, and move-assignment closes any fd it previously owned before taking over the source's.

---

### 02-P18

**Approach:** Write all five explicitly; identify the destructor as the one whose omission causes a double-free (via the implicitly-generated shallow-copy path).

**Reference Solution:**
```cpp
class Buffer {
public:
    explicit Buffer(size_t n) : data_(new int[n]), size_(n) {}

    ~Buffer() { delete[] data_; }

    Buffer(const Buffer& other) : data_(new int[other.size_]), size_(other.size_) {
        std::copy(other.data_, other.data_ + size_, data_);
    }
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            int* new_data = new int[other.size_];   // allocate-then-swap: exception-safe
            std::copy(other.data_, other.data_ + other.size_, new_data);
            delete[] data_;
            data_ = new_data;
            size_ = other.size_;
        }
        return *this;
    }

    Buffer(Buffer&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr; other.size_ = 0;
    }
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_; size_ = other.size_;
            other.data_ = nullptr; other.size_ = 0;
        }
        return *this;
    }
private:
    int* data_;
    size_t size_;
};
```

**Explanation:** if the **destructor** were omitted, the compiler-generated one would do nothing to `data_` (a raw pointer has no destructor of its own) — every `Buffer` would leak its array. If the **copy constructor** were omitted instead (destructor kept), the implicitly-generated copy constructor would shallow-copy `data_` (copy the pointer value, not the pointee), producing two `Buffer`s whose destructors both eventually call `delete[]` on the *same* pointer — a double-free.

**Common Wrong Approaches:** writing copy-assignment as `delete[] data_;` immediately followed by allocating new memory — if the new allocation throws, `data_` has already been freed and the object is left in a broken half-destroyed state. The allocate-then-swap ordering shown above avoids this.

---

### 02-P19

**Approach:** Rule of 0 — compose from self-managing members, write no special member functions.

**Reference Solution:**
```cpp
class Person {
public:
    Person(std::string name, std::vector<int> scores)
        : name_(std::move(name)), scores_(std::move(scores)) {}
    // No destructor, copy, or move members written.
private:
    std::string name_;
    std::vector<int> scores_;
};
// Justification: std::string and std::vector already correctly manage their
// own heap memory, including correct copy/move/destroy semantics. The
// compiler-generated special members for Person simply call each member's
// corresponding special member in turn, which is exactly correct here —
// there is no raw resource for Person itself to own or mismanage.
```

---

### 02-P20

**Approach:** `unique_ptr` can only be moved into the vector, since it has no copy constructor at all.

**Reference Solution:**
```cpp
std::unique_ptr<Widget> make_widget(int id) {
    return std::make_unique<Widget>(id);
}

std::vector<std::unique_ptr<Widget>> widgets;
widgets.push_back(make_widget(1));   // move — the return value is already an rvalue
```

**Explanation:** `make_widget`'s return value is a temporary (rvalue), so `push_back` binds it to its rvalue-reference overload and move-constructs the `unique_ptr` into the vector's storage. Copy isn't "chosen over" move here — copy simply **doesn't exist** for `unique_ptr` (its copy constructor is deleted), so move is the only available option, not a preference.

---

### 02-P21

**Approach:** A stateless deleter (function pointer or captureless-lambda-derived) can, in practice, avoid growing the `unique_ptr`'s size — but a raw function pointer deleter is itself a pointer-sized piece of *state* stored in the object.

**Reference Solution:**
```cpp
using FcloseDeleter = int(*)(FILE*);
std::unique_ptr<FILE, FcloseDeleter> f1(std::fopen("x", "r"), &std::fclose);
// sizeof(f1) is typically 2 pointers (the FILE* and the deleter function pointer).

struct FcloseFunctor { void operator()(FILE* fp) const { std::fclose(fp); } };
std::unique_ptr<FILE, FcloseFunctor> f2(std::fopen("x", "r"));
// sizeof(f2) is typically 1 pointer — an empty, stateless functor type
// participates in the "empty base optimization"-like treatment unique_ptr
// applies to its deleter, so it adds no storage.
```

**Why It Works:** a function-pointer deleter is itself a value that must be stored (it could point to any of several functions), whereas an empty class type carries no per-instance data at all — `unique_ptr` is specified to take advantage of this when the deleter type is empty.

---

### 02-P22

**Approach:** Use `shared_ptr`'s custom-deleter constructor to redirect "delete" into "return to pool."

**Reference Solution:**
```cpp
class ObjectPool {
public:
    std::shared_ptr<Data> acquire() {
        if (!free_.empty()) {
            auto obj = std::move(free_.back());
            free_.pop_back();
            return std::shared_ptr<Data>(obj.release(), [this](Data* p) { release(p); });
        }
        return std::shared_ptr<Data>(new Data(), [this](Data* p) { release(p); });
    }
private:
    void release(Data* p) { free_.emplace_back(p); }
    std::vector<std::unique_ptr<Data>> free_;
};
```

**Explanation:** `shared_ptr`'s deleter is invoked when the refcount reaches zero, but nothing requires that deleter to actually call `delete` — here it calls `release`, which returns the raw pointer to the pool's free list instead of destroying it. Every caller of `acquire()` just sees an ordinary `shared_ptr<Data>` and correctly has no idea its "destruction" is actually recycling.

**C++ Considerations:** this pool is not thread-safe as written (Ch11 territory) — the point here is purely the deleter-mechanism repurposing.

---

### 02-P23 [DEBUG]

**Approach:** The cache holds `shared_ptr`s itself, so `evict()` removing the map's entry does *not* free the `Data` if some caller is still holding a `shared_ptr` returned earlier — but the actual design bug is the reverse: the cache's *own* map entry keeps every cached item alive forever regardless of whether any caller still wants it, since the map itself is a permanent owning reference.

**Reference Solution:** The design bug: the cache holds a **strong** (`shared_ptr`) reference to every entry it has ever cached, so memory is never reclaimed by any mechanism other than an explicit `evict()` call naming the exact key — there's no way for the cache to opportunistically free entries nobody outside is using anymore. Fix: store `weak_ptr<Data>` in the map instead, and have `get()` `lock()` it, re-populating (re-`make_shared`) if the lock fails because the last external owner already let it go:
```cpp
std::shared_ptr<Data> get(int key) {
    if (auto it = map_.find(key); it != map_.end()) {
        if (auto sp = it->second.lock()) return sp;
    }
    auto data = std::make_shared<Data>(key);
    map_[key] = data;   // stores a weak_ptr
    return data;
}
std::unordered_map<int, std::weak_ptr<Data>> map_;
```
Now the cache genuinely observes rather than owns, and entries are freed as soon as the last external `shared_ptr` to them goes away, without needing an explicit `evict()` at all (though `evict()` remains useful for forcing invalidation of a still-referenced entry).

---

### 02-P24

**Approach:** A generic RAII type storing any callable, invoked on destruction unless dismissed.

**Reference Solution:**
```cpp
template <typename F>
class ScopeGuard {
public:
    explicit ScopeGuard(F f) : f_(std::move(f)), active_(true) {}
    ~ScopeGuard() { if (active_) f_(); }
    void dismiss() noexcept { active_ = false; }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
private:
    F f_;
    bool active_;
};
template <typename F> ScopeGuard<F> make_scope_exit(F f) { return ScopeGuard<F>(std::move(f)); }

void multi_step_operation() {
    step_one();
    auto guard1 = make_scope_exit([] { undo_step_one(); });
    step_two();  // if this throws, guard1 fires and undoes step_one
    auto guard2 = make_scope_exit([] { undo_step_two(); });
    step_three();  // if this throws, both guard2 and guard1 fire, in reverse order
    guard2.dismiss();
    guard1.dismiss();  // operation fully succeeded — nothing to roll back
}
```

**Why It Works:** each guard's destructor runs at scope exit — normal or exceptional — exactly like `lock_guard` in 02-P10; dismissing marks success so the rollback becomes a no-op instead of firing.

---

### 02-P25

**Approach:** Show the broken non-virtual-destructor case explicitly, then the fix.

**Reference Solution:**
```cpp
// BROKEN:
struct Base {
    ~Base() { /* no virtual */ }
};
struct Derived : Base {
    int* data = new int[100];
    ~Derived() { delete[] data; }
};
Base* p = new Derived();
delete p;   // UB: only ~Base() runs, ~Derived() never called — data leaks,
            // and technically deleting through a base pointer without a
            // virtual destructor when the dynamic type differs is UB outright.

// FIXED:
struct Base {
    virtual ~Base() = default;
};
// Derived unchanged.
Base* p = new Derived();
delete p;   // correct: ~Derived() runs first, then ~Base() — data is freed.
```

**Why It Works:** `delete` through a pointer only calls the *statically* known destructor unless that destructor is virtual — a virtual destructor makes `delete p` dispatch dynamically to the actual most-derived object's destructor, exactly like any other virtual call.

---

### 02-P26

**Approach:** `unique_ptr` has no copy constructor, so a defaulted copy constructor for the owning class has nothing to call.

**Reference Solution:**
```cpp
class Widget {
public:
    Widget(int id) : impl_(std::make_unique<Impl>(id)) {}
    Widget(const Widget& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}  // deep copy
private:
    std::unique_ptr<Impl> impl_;
};
// Widget(const Widget&) = default;  // would fail: unique_ptr<Impl> has no
// copy constructor to invoke, so the compiler-generated copy constructor
// (which just calls each member's copy constructor) cannot be formed.
```

**Explanation:** `= default` asks the compiler to generate a copy constructor that copy-constructs each member in turn; since `unique_ptr<Impl>`'s copy constructor doesn't exist (it's deleted, by design, since unique_ptr models exclusive ownership), that generated function would be ill-formed, so `= default` on a class with a `unique_ptr` member as-is fails to compile — you must write the deep-copy logic yourself, deciding explicitly what "copying" a `unique_ptr`-owned resource should mean.

---

### 02-P27 [DEBUG]

**Approach:** `parent` and `children` form a reference cycle — parent keeps children alive, children keep parent alive, so refcount never reaches 0 for either.

**Reference Solution:** The cycle: a node's `children` vector holds `shared_ptr<Node>` to each child, and each child's `parent` holds a `shared_ptr<Node>` back to it — neither side's refcount ever drops to zero as long as the other side exists, even if nothing outside the graph references any of them, so the entire subtree leaks.

**Fix:** make the *upward* link (`parent`) a `weak_ptr<Node>`, since ownership naturally flows downward (a parent owns its children, a child does not own its parent):
```cpp
struct Node {
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
};
```

**Why It Works:** breaking exactly one direction of a two-directional strong reference is sufficient to break the cycle — the remaining direction (`children`) still correctly keeps a node's descendants alive as long as the node itself is reachable from a root.

---

### 02-P28

**Approach:** Hand-implement the control-block-plus-refcount mechanism `shared_ptr` provides, single-threaded, to demonstrate understanding of the underlying model.

**Reference Solution:**
```cpp
template <typename T>
class SimpleShared {
public:
    explicit SimpleShared(T* p) : ptr_(p), count_(new int(1)) {}

    SimpleShared(const SimpleShared& other) : ptr_(other.ptr_), count_(other.count_) {
        ++(*count_);
    }
    SimpleShared& operator=(const SimpleShared& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_; count_ = other.count_;
            ++(*count_);
        }
        return *this;
    }
    ~SimpleShared() { release(); }

    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }

private:
    void release() {
        if (--(*count_) == 0) { delete ptr_; delete count_; }
    }
    T* ptr_;
    int* count_;
};
```

**Why It Works:** every copy increments a shared count; every destruction decrements it and frees both the managed object and the count itself only when it reaches zero — this is exactly the model `shared_ptr`'s control block implements, minus atomicity (deliberately out of scope — that's Ch11), minus custom-deleter support, and minus `weak_ptr` interoperation.

**Complexity:** copy is O(1); destruction is O(1).

---

### 02-P29 [DEBUG]

**Approach:** The hand-written copy constructor shallow-copies the raw pointer, so both `a` and `b` end up owning (and eventually `delete`-ing) the same `int*`.

**Reference Solution:** `Widget b = a;` copy-constructs `b.data_` as a copy of `a.data_` — the *pointer value*, not a new allocation. When `a` and `b` both go out of scope, each destructor calls `delete data_` on the same address — the second `delete` is a double-free.

**Fix:** either deep-copy in the copy constructor (`data_(new int(*other.data_))`), or — better, matching the Rule-of-0 philosophy — disable copy entirely and require explicit move/clone semantics if shared raw ownership was never actually intended.

---

### 02-P30 [DEBUG]

**Approach:** `reset()` only affects the `shared_ptr` object it's called on, not other `shared_ptr`s sharing the same control block.

**Reference Solution:** The caller's expectation is **wrong**. `a.reset()` only makes `a` itself empty and decrements the shared refcount by one; `b` is unaffected and still owns the `int` (refcount is now 1, owned solely by `b`). `reset()` is not "destroy the managed object" — it's "release *this* `shared_ptr`'s ownership," and the object is only actually destroyed once the refcount reaches zero, i.e., once *every* owning `shared_ptr` has released it.

---

### 02-P31 [DEBUG]

**Approach:** If the second `new[]` throws, the constructor never completes, so the destructor (which would free `a_`) never runs — `a_` leaks. Prefer RAII members over manual paired new/delete.

**Reference Solution:** If `new int[m]` throws, `a_` (already successfully allocated) leaks, because `Pair`'s own constructor never finishes, so `~Pair()` (which would have freed `a_`) is never invoked — exactly the mechanism from 02-P16, applied to a constructor's own body rather than one specific member.

**Fix — RAII-first, not manual `catch`+`delete`:**
```cpp
class Pair {
public:
    Pair(size_t n, size_t m) : a_(std::make_unique<int[]>(n)), b_(std::make_unique<int[]>(m)) {}
private:
    std::unique_ptr<int[]> a_;
    std::unique_ptr<int[]> b_;
};
```
Now if the second allocation throws, `a_` (already a fully-constructed `unique_ptr` member) is destroyed correctly during unwind per QC7 — no `catch` block needed at all.

**Common Wrong Approaches:** wrapping the constructor body in `try { ... } catch (...) { delete[] a_; throw; }` — this works but reintroduces exactly the manual-bookkeeping fragility RAII exists to eliminate; every new resource added to the class requires remembering to extend the catch block.

---

### 02-P32 [DEBUG]

**Approach:** This isn't RAII at all — it's manual `new`/`delete`, and `doWork()` throwing skips the `delete`.

**Reference Solution:** This leaks whenever `doWork()` throws, because `delete resource;` is a statement that simply never executes if the line before it throws — there is no RAII here; `resource` is a plain local variable holding a raw pointer, not an object whose destructor does anything. The code *looks* structured like RAII (acquire, use, release) but the "release" step is an ordinary statement, not a destructor, so it has no special relationship to the exception path at all.

**Fix:** `auto resource = std::make_unique<Resource>(); resource->doWork();` — now the freeing genuinely happens in a destructor, which runs regardless of how the scope is exited.

---

### 02-P33

**Approach:** Apply the special-member generation rules for a class with a user-declared copy constructor only.

**Reference Solution:**
- Copy-assignment operator: still implicitly generated (declaring the copy constructor alone doesn't suppress it), but **deprecated** by the standard when a copy constructor is user-declared and copy-assignment isn't (Annex D deprecation — most compilers still generate it, with a possible warning).
- Destructor: implicitly generated as usual (user-declaring a copy constructor doesn't affect destructor generation).
- Move constructor and move-assignment operator: **not generated at all** — declaring *any* of destructor/copy-constructor/copy-assignment suppresses implicit generation of both move members.

**Why It Works:** the suppression rule cares about which categories are user-declared, and a user-declared copy constructor alone is enough to trigger it for the move pair, even though copy-assignment and the destructor are left to their implicit (if deprecated in one case) defaults.

---

### 02-P34

**Approach:** `shared_ptr<T[]>`'s *default* deleter calls `delete[]`; overriding it with a scalar-`delete`-based custom deleter mismatches allocation and deallocation forms.

**Reference Solution:** If a `shared_ptr<T[]>` is constructed with a custom deleter that calls plain `delete p` (scalar delete) instead of `delete[] p` (array delete) on an array that was allocated with `new T[n]`, the result is undefined behavior — mismatched `new[]`/`delete` is exactly as broken as it is outside of any smart pointer, and `shared_ptr`'s array support (C++17) exists specifically so the *default* deleter gets this right automatically; supplying your own deleter opts back into needing to get it right yourself.

---

### 02-P35

**Approach:** A `unique_ptr<Impl>` member's destructor requires `Impl`'s complete definition to be visible at the point the *owning class's* destructor is instantiated — even an implicitly-defaulted one.

**Reference Solution:** `unique_ptr<T>`'s destructor calls `delete` on the managed pointer, and `delete` on an incomplete type is undefined behavior (if it happens to compile at all, which it often doesn't). If the outer class's destructor is implicitly defaulted **in the header**, and `Impl` is only forward-declared in that header (its full definition living in a `.cpp` file, which is the entire point of the pointer-to-implementation pattern), then the implicitly-generated destructor is instantiated wherever the header is included — at a point where `Impl` is still incomplete — causing either a compile error or, worse on some compilers/configurations, silently wrong behavior. This is specifically a *destructor* problem, not a general "incomplete type" problem, because incomplete types are otherwise fine to hold behind a pointer; it's calling `delete` on one that's the issue.

**Fix:** declare (don't define) the outer class's destructor in the header, and define it (even as `= default`) in the `.cpp` file, at a point where `Impl`'s full definition is visible.

---

### 02-P36

**Approach:** A destructor that throws during stack unwinding (i.e., while another exception is already propagating) causes `std::terminate`.

**Reference Solution:** `ScopeGuard`'s destructor must not let an exception escape it — if the guarded callable `f_()` throws during the destructor's execution, and that destructor is itself running *during unwinding from a different exception*, having a second exception in flight simultaneously is precisely the condition that calls `std::terminate()`, immediately and unconditionally, per the language rule (not merely "the program is in an ill-defined state" — `terminate` is guaranteed). If the callable's own operation could plausibly throw, the guard's destructor should catch and swallow (or log) any exception rather than letting it propagate.

**C++ Considerations:** this is precisely why the recommended practice is that RAII cleanup logic (destructors generally) should be `noexcept`, or at minimum defensively wrapped — 02-P10's `lock_guard::~lock_guard()` is `noexcept` for exactly this reason.

---

### 02-P37

**Approach:** Snapshot on construction; commit is a no-op default; rollback happens in the destructor only if not explicitly committed.

**Reference Solution:**
```cpp
template <typename State>
class TransactionalUpdate {
public:
    explicit TransactionalUpdate(State& target)
        : target_(target), snapshot_(target), committed_(false) {}

    void commit() noexcept { committed_ = true; }

    ~TransactionalUpdate() {
        if (!committed_) target_ = snapshot_;   // roll back
    }
private:
    State& target_;
    State snapshot_;
    bool committed_;
};

// Success path:
State s{/* ... */};
{
    TransactionalUpdate<State> txn(s);
    mutate(s);
    txn.commit();
}  // s keeps mutate()'s changes

// Failure path:
{
    TransactionalUpdate<State> txn(s);
    mutate(s);
    throw std::runtime_error("failed midway");
    // txn.commit() never reached
}  // s is restored to its pre-mutate snapshot during unwind
```

---

### 02-P38

**Approach:** A simple counting registry that RAII types increment/decrement in their constructor/destructor.

**Reference Solution:**
```cpp
class ResourceRegistry {
public:
    static ResourceRegistry& instance() { static ResourceRegistry r; return r; }
    void register_live() { ++live_count_; }
    void unregister_live() { --live_count_; }
    int live_count() const { return live_count_; }
private:
    int live_count_ = 0;
};

// ScopedFd's constructor calls ResourceRegistry::instance().register_live();
// ScopedFd's destructor calls ResourceRegistry::instance().unregister_live();
// (same pattern wired into a ScopedLock-style type)
```

**Invariant to check at program exit:** `ResourceRegistry::instance().live_count() == 0`. Any nonzero value at the point right before program termination indicates at least one RAII-wrapped resource was leaked (constructed but never destroyed) — a cheap, structural leak-detection mechanism that doesn't require external tooling.

---

### 02-P39

**Approach:** Ownership crossing a module boundary through a base-class pointer is structurally identical to 02-P25's virtual-destructor scenario.

**Reference Solution:**
```cpp
// factory module
std::unique_ptr<Base> create_plugin() { return std::make_unique<ConcretePlugin>(); }

// caller module — only ever sees Base
struct Base { virtual ~Base() = default; virtual void run() = 0; };
auto plugin = create_plugin();
plugin->run();
// when `plugin` goes out of scope, unique_ptr<Base>'s destructor calls
// delete on a Base*, whose *dynamic* type is ConcretePlugin.
```

**Why It Works:** the caller module never even has `ConcretePlugin`'s definition available — it only ever holds `unique_ptr<Base>`. This is exactly 02-P25's scenario, but the type-erasure is now structural (the caller *cannot* know the derived type, not merely *doesn't*), which makes the virtual destructor non-optional rather than merely good practice: there's no code path by which the caller could correctly destroy the right type manually even if it wanted to.

---

### 02-P40 [DEBUG]

**Approach:** Same cycle shape as 02-P27, applied to parent/child sessions; break the child→parent link specifically.

**Reference Solution:**
```cpp
struct Session {
    std::weak_ptr<Session> parent;              // child -> parent: weak
    std::vector<std::shared_ptr<Session>> children;  // parent -> child: strong
};
```
**Why specifically child→parent, not parent→child:** ownership here flows naturally downward — a parent session is responsible for its children's lifetime (when the parent goes away, its children should too), so `children` being `shared_ptr` correctly models "parent keeps children alive." The reverse link exists purely so a child can *navigate to* its parent, not to *keep the parent alive* — a child session outliving its parent conceptually makes little sense, and even if it did, the child shouldn't be the one deciding the parent stays alive. Making the parent link `weak_ptr` breaks the cycle while preserving both navigation directions (`lock()` on the child's `parent` still works as long as the parent is genuinely still alive via some other strong reference, e.g., a top-level session registry).

---

### 02-P41

**Approach:** Move transfers ownership, leaving the source `nullptr`; using the moved-from `unique_ptr`'s value (not just its existence) is where the actual hazard is.

**Reference Solution:**
```cpp
std::tuple<std::unique_ptr<Widget>, std::unique_ptr<Widget>, std::unique_ptr<Widget>> make_widget_family() {
    return { std::make_unique<Widget>(1), std::make_unique<Widget>(2), std::make_unique<Widget>(3) };
}

void consume_one(std::unique_ptr<Widget> w) { /* takes ownership */ }

auto [a, b, c] = make_widget_family();
consume_one(std::move(a));
// a is now guaranteed to be nullptr — well-defined to check:
assert(a == nullptr);        // fine — comparing to nullptr is well-defined
// a->foo();                 // UB — dereferencing a moved-from (null) unique_ptr
```

**Why It Works:** `unique_ptr`'s move constructor is specified to leave the moved-from object holding `nullptr` — checking *that fact* (`== nullptr`, or its `bool` conversion) is perfectly well-defined, since it's just inspecting the pointer value. Dereferencing it is undefined behavior for the same reason dereferencing any null pointer is — moved-from-ness doesn't change that rule, it just explains *why* it's null here specifically.

---

### 02-P42

**Approach:** Removing the user-declared destructor/copy-constructor/copy-assignment (previously deleted) changes what's implicitly generated for the class as a whole, not just for the one member being refactored.

**Reference Solution:** Before the refactor, copy was explicitly `= delete`d, so the class was clearly, deliberately non-copyable. After the refactor — owning a `unique_ptr<int>` instead of a raw `int*`, with the destructor/copy-constructor/copy-assignment removed entirely rather than kept as deletions — the class becomes **whatever the compiler implicitly generates for a class with a `unique_ptr` member**: copy construction and copy-assignment are **still not available** (since `unique_ptr` itself has no copy constructor, so the implicitly-generated ones would be ill-formed and are therefore not generated — same reasoning as 02-P26), but move construction and move-assignment are now **implicitly generated and available**, where before they were suppressed by the user-declared destructor. This is very likely the *intended* outcome (the refactor's whole point is usually "let unique_ptr make this movable"), but it's a genuine, observable behavior change — code that previously failed to compile when trying to move an instance of this class will now compile and do something — and the way to find out whether it's desired rather than accidental is to check whether any existing code relied on the type being immovable (e.g., stored by value in a container that assumed stable addresses, or deliberately used as a non-movable sentinel), not merely to assume "more capable" is strictly better.

---

### 02-P43

**Approach:** A `shared_ptr` copy is an independent reference to the *same* control block pointing at the *same* object — reseating one holder's copy doesn't affect any other holder's copy.

**Reference Solution (design note):** Reseating the "master" `shared_ptr<Config>` that the hot-reload mechanism holds to point at a newly-loaded `Config` does **not** propagate to any subsystem that already holds its own **copy** of the old `shared_ptr<Config>` — each copy is an independent reference to the same control block, and reseating one copy has no effect on any other copy; it only changes what *that one* `shared_ptr` variable points to. Every subsystem that already copied the pointer keeps observing the old `Config` object forever, since nothing about their copy changed. Achieving actual hot-reload requires an added layer of indirection: subsystems should hold a `shared_ptr<std::atomic<std::shared_ptr<Config>>>` (or equivalent — a shared, mutable *slot* that itself contains the current `shared_ptr<Config>`, updated atomically on reload) rather than holding the `Config` pointer directly, so that reseeding the slot is visible to every subsystem that reads through it. The new lifetime hazard this indirection introduces: a subsystem might read the slot once at startup and cache the resulting `shared_ptr<Config>` locally for the rest of its lifetime, silently opting itself out of ever seeing a reload — the indirection only helps callers who *re-read the slot* on each use, and auditing which callers actually do that becomes its own ongoing maintenance burden.

---

## Integration Challenge Solution — 02-IC1

**Reference Solution:** Combine `ScopedFd` (02-P17, move-only) and `SimpleShared`-style refcounting (02-P28, shared) with the `ResourceRegistry` (02-P38):

```cpp
class ScopedFdTracked {
public:
    explicit ScopedFdTracked(int fd) noexcept : fd_(fd) {
        ResourceRegistry::instance().register_live();
    }
    ~ScopedFdTracked() {
        if (fd_ >= 0) { ::close(fd_); ResourceRegistry::instance().unregister_live(); }
    }
    ScopedFdTracked(const ScopedFdTracked&) = delete;
    ScopedFdTracked& operator=(const ScopedFdTracked&) = delete;
    ScopedFdTracked(ScopedFdTracked&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    // move-assignment omitted for brevity — same pattern as 02-P17
private:
    int fd_;
};

class SharedResourceTracked {
public:
    explicit SharedResourceTracked(int id) : ptr_(new int(id)), count_(new int(1)) {
        ResourceRegistry::instance().register_live();
    }
    SharedResourceTracked(const SharedResourceTracked& other)
        : ptr_(other.ptr_), count_(other.count_) { ++(*count_); }
    ~SharedResourceTracked() {
        if (--(*count_) == 0) {
            delete ptr_; delete count_;
            ResourceRegistry::instance().unregister_live();
        }
    }
private:
    int* ptr_;
    int* count_;
};

// Exception-during-construction path, mirroring 02-P31's fix:
class Pair {
public:
    Pair(int a, int b) : first_(a), second_(b) {
        if (b < 0) throw std::invalid_argument("negative");  // thrown AFTER both members constructed
    }
private:
    ScopedFdTracked first_;
    ScopedFdTracked second_;
};
// If the constructor body throws, first_ and second_ (already fully
// constructed) are destroyed during unwind — the registry's live count
// correctly drops back by 2, proving no leak occurred despite the throw.
```

**Testing Strategy:** assert `ResourceRegistry::instance().live_count() == 0` before the program exits, and additionally assert it returns to its pre-test value after each individual test case (construct-destroy, construct-move-destroy, construct-throw-during-another-constructor, copy-then-drop-one-of-two-shared-owners) — a live count that fails to return to baseline after any one of these paths pinpoints exactly which path leaks.

**Why It Works:** every one of the four required paths (normal, copy, move, exception-during-construction) ultimately reduces to the same underlying guarantee exercised throughout this chapter — a fully-constructed object's destructor runs when its scope ends, regardless of *why* that scope ends — and the registry's live-count invariant turns "did that guarantee actually hold" from an assumption into something you can assert on.
