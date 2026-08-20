# Chapter 02 — Object Lifetime, Ownership, and RAII

> Scope: tying resource validity to object lifetime, and expressing ownership in types. Solutions live in the sibling file [`SOLUTIONS.md`](SOLUTIONS.md) — don't open it until you've attempted a problem.

**Prerequisites:** [Chapter 01](../01-core-semantics/CONCEPTS.md) (initialization, `const`/`constexpr`, scope vs. storage duration, overload resolution).

**Concepts owned here:** constructors, destructors, initialization order, RAII, rule of 0/3/5, ownership, `unique_ptr`, `shared_ptr`, `weak_ptr`, custom deleters, custom resources, exception safety (introduced as a lifetime property), object lifetime.

**Referenced but not owned here:** exception guarantees formalized (Ch06), move-semantics interaction with these types (Ch03), `shared_ptr` control-block layout (Ch07).

---

## Crash Course

### Constructors and Destructors

A constructor's job is to establish a class invariant; a destructor's job is to release whatever the constructor acquired. Constructors can delegate to one another (delegating constructors, C++11):

```cpp
struct Widget {
    Widget() : Widget(0) {}          // delegates
    explicit Widget(int id) : id_(id) {}
    ~Widget() { /* release id_-related resource, if any */ }
    int id_;
};
```

Member initializer-list order always follows **declaration order** in the class, never the order written in the initializer list — a mismatch between the two is a common source of `-Wreorder` warnings and, when one member's initializer depends on another, a real bug.

### Initialization Order

Base classes construct before members; members construct in declaration order; destruction runs in the exact reverse of construction order. Across translation units, the relative order of *dynamic* initialization of namespace-scope objects is unspecified — this is the static-initialization-order fiasco alluded to in Ch01's `constinit` material, and it's why cross-TU global objects with interdependent constructors are a design smell.

### RAII

RAII (Resource Acquisition Is Initialization) is not a language feature — it's an idiom that leans on one guarantee the language *does* provide: a local object's destructor runs when it goes out of scope, unconditionally, including when the scope is exited via an exception. Tie a resource's lifetime to an object's lifetime, and "forgetting to clean up" becomes structurally impossible rather than a discipline problem.

```cpp
class FileHandle {
public:
    explicit FileHandle(const char* path) : fp_(std::fopen(path, "r")) {
        if (!fp_) throw std::runtime_error("open failed");
    }
    ~FileHandle() { if (fp_) std::fclose(fp_); }
    FileHandle(const FileHandle&) = delete;             // see Rule of 5 below
    FileHandle& operator=(const FileHandle&) = delete;
private:
    std::FILE* fp_;
};
```

### Rule of 0 / 3 / 5

**Rule of 0**: prefer to write *no* special member functions at all — compose your class from members that already manage their own resources (a `unique_ptr`, a `std::vector`, a `std::string`), and let the compiler generate correct copy/move/destroy for free.

**Rule of 3** (pre-C++11 shape): if you write a destructor, a copy constructor, or a copy-assignment operator, you almost certainly need all three, because the compiler-generated ones are almost certainly wrong for a class that owns a raw resource.

**Rule of 5** (C++11+): the same reasoning extended to move constructor and move-assignment operator. Critically: **declaring any one of the five suppresses some of the compiler-generated others** — for instance, declaring a destructor suppresses the implicitly-generated move constructor and move-assignment operator (they simply aren't generated at all, not generated-and-then-wrong), which silently downgrades every move of that type to a copy if a copy constructor happens to still be available, or to a compile error if it isn't.

The rule of 0 is the actual goal; the rule of 3/5 exists because, once you can't achieve rule of 0 (you're the one writing a low-level resource-owning type), the compiler's defaults are unsafe the moment you've touched any one of the five.

### Ownership

Ownership is a design decision about *which piece of code is responsible for destroying an object*, and modern C++ encodes that decision in the type itself rather than in a comment: unique ownership (`unique_ptr`), shared ownership (`shared_ptr`), or non-owning observation (a raw pointer/reference, or `weak_ptr`). Choosing the wrong one is not a syntax error — it's a design bug that surfaces as a leak, a double-free, or a dangling access.

### `unique_ptr`, `shared_ptr`, `weak_ptr`

- `unique_ptr<T>` — move-only, exactly one owner at a time, near-zero overhead over a raw pointer for the default deleter (universal practice, not a strict standard mandate).
- `shared_ptr<T>` — reference-counted shared ownership; the control block (refcount + deleter) is typically a *separate* heap allocation unless constructed via `make_shared`, which allocates the control block and the object together. Increment/decrement of the refcount is atomic — a real, measurable cost under contention, not a free abstraction.
- `weak_ptr<T>` — a non-owning observer of a `shared_ptr`-managed object that can detect whether the object is still alive; you cannot dereference a `weak_ptr` directly — you must `lock()` it into a temporary `shared_ptr` first, which is the only safe way to promote observation into (possibly brief) ownership.

### Custom Deleters and Custom Resources

`unique_ptr<T, Deleter>`'s second template parameter lets you manage *any* resource with handle-like semantics — a `FILE*`, a Win32 `HANDLE`, an OS-level lock — not just heap-allocated `T*`. A stateful custom deleter is stored *inside* the `unique_ptr` object, which means it changes the type's size — a `unique_ptr` with a captureless-lambda or function-pointer deleter is not necessarily pointer-sized. This is the direct precursor to Chapter 09's fd/HANDLE wrapping.

### Exception Safety (Introduced Here)

At the level this chapter introduces it: does a `throw` during construction of an object (or during one step of a multi-step operation) leak a resource, or leave something double-freed? RAII is the primary tool for making the answer "no" by construction rather than by careful bookkeeping. The *formal* guarantee levels (no-throw / strong / basic / none) are Chapter 06's job — here, the concern is purely "does this specific piece of code leak or corrupt state if an exception crosses it."

### Object Lifetime

An object's **lifetime** begins once its initialization completes and ends when its destructor begins running — a subtly different span than its **storage duration** (Ch01), which is about when the underlying memory exists. A `std::optional<T>`'s storage exists for the `optional`'s entire storage duration, but the contained `T`'s *lifetime* only spans the interval between it being constructed (via `emplace`, assignment, etc.) and being destroyed (via `reset()`, reassignment, or the `optional` itself being destroyed) — the storage can outlive, or exist without ever containing, a live object.

---

## Common Misconceptions

1. **"If I write a destructor, the compiler still generates a correct move constructor for me."** No — declaring a destructor suppresses the *implicit generation* of the move constructor and move-assignment operator entirely. You get silent copy-fallback or a compile error, never a "wrong" move — the move members simply don't exist unless you write them (or `= default` them) yourself.
2. **"`shared_ptr` is just a `unique_ptr` you can copy."** The refcounting isn't free — every copy/destroy of a `shared_ptr` is an atomic increment/decrement, and the control block is very often a separate allocation. It solves a real ownership problem; it is not a drop-in "safer" replacement for `unique_ptr` with no cost.
3. **"A `weak_ptr` can be dereferenced like a pointer, it's just 'weaker.'"** You cannot dereference a `weak_ptr` at all — you must `lock()` it first, precisely because the object it observes may have already been destroyed between the time you checked and the time you'd use it; `lock()` makes that check-and-use atomic with respect to destruction.
4. **"RAII is a C++ keyword or built-in feature."** It's an idiom built entirely on one guarantee: destructors of automatic-duration objects run at scope exit, including on the exception path. Nothing more is required by the language.
5. **"A class with no explicit destructor can't leak."** It absolutely can, if one of its members is a raw resource handle (an `int` fd, a raw `T*` from `new`) with no owning wrapper — the compiler-generated destructor destroys members, but destroying a raw pointer *member* does not free what it points to.
6. **"Throwing from a constructor always leaks whatever the constructor allocated so far."** Not if earlier-constructed members/bases are themselves RAII types — their destructors still run during the unwind of the partially-constructed object. It only leaks the parts that *aren't* RAII-managed, which is exactly why "prefer members that own their own resources" (Rule of 0) is also an exception-safety technique, not just a boilerplate-reduction one.

---

## Quick Checks

- **02-QC1.** If a class declares only a destructor (no other special member function), does the compiler still generate a move constructor?
- **02-QC2.** In what order do base classes and members construct, relative to each other?
- **02-QC3.** Is RAII a language feature, or an idiom built on top of one?
- **02-QC4.** Can you call a member function directly on a `weak_ptr<T>`'s pointee without any intermediate step?
- **02-QC5.** Does `make_shared<T>(...)` perform one heap allocation or two, in the typical case?
- **02-QC6.** If a `unique_ptr<T, MyStatefulDeleter>` is used instead of plain `unique_ptr<T>`, is `sizeof` guaranteed to stay the same as a raw pointer?
- **02-QC7.** If an exception is thrown partway through a constructor's body, do already-constructed base classes and members get destroyed?
- **02-QC8.** What's the difference between an object's storage duration and its lifetime?

---

## Problems

### Level 1 — Recognition (6)

**02-P01.** Given:
```cpp
struct A {
    A() : A(0) {}
    explicit A(int x) : x_(x) {}
    int x_;
};
```
What is the value of `x_` after `A a;`?

**02-P02.** State the order in which the following run, given `struct D : B { M m; ~D(); };` is destroyed: `~D()`'s body, `m`'s destructor, `B`'s destructor.

**02-P03.** Is the following class's implicitly-generated move constructor available, or suppressed? Justify from the rule stated in the Crash Course.
```cpp
struct Resource {
    ~Resource() { /* release */ }
    int* data;
};
```

**02-P04.** Given a `std::unique_ptr<Widget> p;` that is default-constructed (holding no object), is calling `p->foo()` well-formed at compile time? Is it safe to execute?

**02-P05.** Given `std::shared_ptr<int> a = std::make_shared<int>(5); std::shared_ptr<int> b = a;`, what is the reference count after this line, and what event decrements it?

**02-P06.** Is the following legal, and what does it do?
```cpp
std::weak_ptr<int> w;
{
    auto sp = std::make_shared<int>(42);
    w = sp;
}
auto locked = w.lock();
```

### Level 2 — Prediction (10)

**02-P07.** Predict any warning and any bug in:
```cpp
struct Range {
    Range(int lo, int hi) : hi_(hi), lo_(lo) {}
    int lo_, hi_;
};
```

**02-P08. [DEBUG]** This is intended to initialize `size_` before allocating a buffer of that size, but crashes intermittently. Explain why, referencing declaration order specifically.
```cpp
class Buffer {
public:
    Buffer(int n) : data_(new int[size_]), size_(n) {}
    ~Buffer() { delete[] data_; }
private:
    int* data_;
    int size_;
};
```

**02-P09.** Predict whether the following compiles, and if so, what happens at runtime, given two independent translation units each defining one of these globals with the shown initializers:
```cpp
// TU1
Logger g_logger("app.log");
// TU2
extern Logger g_logger;
int g_startup_code = g_logger.get_status();  // dynamic init, depends on g_logger already being constructed
```

**02-P10.** Predict the output and explain in terms of RAII:
```cpp
void f() {
    std::lock_guard<std::mutex> lock(m);
    throw std::runtime_error("oops");
}
// is m left locked after f() propagates the exception?
```

**02-P11.** Predict which of the compiler-generated special member functions exist for this class, and which are suppressed:
```cpp
class Handle {
public:
    Handle(int fd) : fd_(fd) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
private:
    int fd_;
};
```
(No destructor, no move members written.)

**02-P12. [DEBUG]** This class is intended to be non-copyable but movable. Explain why it's actually neither, as written.
```cpp
class Connection {
public:
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    ~Connection() { /* close socket */ }
private:
    int socket_fd_;
};
```

**02-P13.** Given `std::shared_ptr<Node> a; std::shared_ptr<Node> b = a;` where `a` is default-constructed (empty), what is `b` afterward, and does the refcount increment?

**02-P14.** Predict whether this compiles, and why, focusing on `weak_ptr`'s interface:
```cpp
std::weak_ptr<int> w = std::make_shared<int>(5);
int v = *w;  // ?
```

**02-P15.** Predict the sequence of constructor/destructor calls (by name) for:
```cpp
struct Inner { Inner() { puts("Inner()"); } ~Inner() { puts("~Inner()"); } };
struct Outer {
    Inner a;
    Inner b;
    Outer() { puts("Outer()"); }
    ~Outer() { puts("~Outer()"); }
};
{ Outer o; }
```

**02-P16. [DEBUG]** A reviewer flags this as a leak under one specific circumstance. Identify it.
```cpp
class Session {
public:
    Session() : buffer_(new char[1024]) {
        if (!connect()) throw std::runtime_error("connect failed");
    }
    ~Session() { delete[] buffer_; }
private:
    char* buffer_;
    bool connect();
};
```

### Level 3 — Implementation (12)

**02-P17.** Implement a minimal RAII wrapper `ScopedFd` around a POSIX-style `int` file descriptor: constructor takes ownership of an already-open fd, destructor closes it, copy is disabled, move transfers ownership (leaving the moved-from object holding an invalid fd sentinel).

**02-P18.** Implement `Rule of 5` explicitly (not `= default`, write the bodies) for a class `Buffer` that owns a `int* data_` and `size_t size_` allocated with `new int[]`, and explain in a comment which of the five would produce a double-free if omitted.

**02-P19.** Implement a class composed entirely of `std::string` and `std::vector<int>` members (Rule of 0) and, in a comment, justify why you do *not* need to write any of the five special member functions.

**02-P20.** Implement a `unique_ptr`-based factory function `make_widget(int id) -> std::unique_ptr<Widget>` and a caller that transfers ownership into a `std::vector<std::unique_ptr<Widget>>`. State which operation (copy or move) inserts it into the vector, and why the alternative isn't available.

**02-P21.** Implement a custom deleter for `unique_ptr<FILE, ???>` wrapping `std::fclose`, using a function pointer deleter, then again using a stateless lambda-derived deleter, and compare `sizeof` of the two resulting `unique_ptr` types.

**02-P22.** Implement a small object pool that hands out `shared_ptr<T>` instances whose *custom deleter* returns the object to the pool instead of destroying it. Explain how this reuses `shared_ptr`'s deleter mechanism for a purpose other than "release the resource."

**02-P23. [DEBUG]** The following "cache" is intended to let callers hold onto cached values without preventing eviction. Identify the design bug (not a crash — a design bug) and fix it using the concepts from this chapter.
```cpp
class Cache {
public:
    std::shared_ptr<Data> get(int key) {
        auto it = map_.find(key);
        if (it != map_.end()) return it->second;
        auto data = std::make_shared<Data>(key);
        map_[key] = data;
        return data;
    }
    void evict(int key) { map_.erase(key); }  // intended to free memory
private:
    std::unordered_map<int, std::shared_ptr<Data>> map_;
};
```

**02-P24.** Implement a `ScopeGuard` (a generic `scope_exit`-style RAII type) that takes any callable in its constructor and invokes it in the destructor, unless explicitly dismissed. Demonstrate it rolling back a partially-completed multi-step operation when an exception is thrown midway.

**02-P25.** Implement a class hierarchy `Base` / `Derived` where `Base` has a virtual destructor, and demonstrate — with a comment showing the fixed version — the specific broken variant of this code where `Base`'s destructor is *not* virtual and a `Derived` object is deleted through a `Base*`.

**02-P26.** Implement a `unique_ptr`-owning class `Widget` whose copy constructor is deliberately deep-copying (not defaulted, since `unique_ptr` has no copy constructor to default to). Write the explicit deep-copy logic and explain why `= default` would fail to compile here.

**02-P27. [DEBUG]** The following graph structure leaks. Identify the cycle and fix it with the appropriate smart pointer choice.
```cpp
struct Node {
    std::shared_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
};
```

**02-P28.** Implement a minimal thread-unsafe (single-threaded, this chapter doesn't cover synchronization yet) reference-counted handle type from scratch — not using `shared_ptr` — to demonstrate understanding of what `shared_ptr` actually does internally: a control block with a count, incremented on copy, decremented and conditionally-deleting on destruction.

### Level 4 — Debugging & Deeper Prediction (8, interleaved above via [DEBUG] tags; standalone continuation below)

**02-P29. [DEBUG]** This code intermittently double-frees. Find the bug.
```cpp
class Widget {
public:
    Widget(int* data) : data_(data) {}
    ~Widget() { delete data_; }
    Widget(const Widget& other) : data_(other.data_) {}  // ?
private:
    int* data_;
};
Widget a(new int(5));
Widget b = a;  // copy
```

**02-P30. [DEBUG]** A caller reports that after calling `reset()` on a `shared_ptr`, a *different* `shared_ptr` they thought was independent also became empty. Diagnose the misunderstanding.
```cpp
std::shared_ptr<int> a = std::make_shared<int>(1);
std::shared_ptr<int> b = a;
a.reset();
// caller expected b to also be empty now — is that correct?
```

**02-P31. [DEBUG]** This constructor is intended to be exception-safe but isn't. Identify exactly which allocation leaks if the second one throws, and fix it using an RAII-first approach rather than a manual `catch`+`delete`.
```cpp
class Pair {
public:
    Pair(size_t n, size_t m) {
        a_ = new int[n];
        b_ = new int[m];  // if this throws, a_ leaks
    }
    ~Pair() { delete[] a_; delete[] b_; }
private:
    int* a_;
    int* b_;
};
```

**02-P32. [DEBUG]** Explain why the following, despite looking like proper RAII, still leaks under one specific call pattern.
```cpp
void process() {
    auto* resource = new Resource();
    resource->doWork();  // may throw
    delete resource;
}
```

**02-P33.** Given a class with a user-declared copy constructor but no other special members written, list every other special member function and state, for each, whether it is implicitly generated, generated-but-deprecated, or not generated at all, per the standard's rules as of C++17/20.

**02-P34.** A `shared_ptr<T[]>`-style array-managing shared pointer (C++17 array support) is used with a custom, non-array `delete`-based deleter by mistake. Explain the resulting bug precisely (not just "it's wrong") in terms of what the default vs. custom deleter each actually does for array types.

**02-P35.** Explain, precisely, why the following does not achieve what a Rule-of-0 design would: a class stores a `unique_ptr<Impl>` (pointer to an implementation) but the outer class's *destructor* is implicitly defaulted and defined in the header, where `Impl`'s definition isn't visible. What breaks, and why does it relate to lifetime/destructor rules rather than just "incomplete type" alone?

**02-P36.** Given the `ScopeGuard` from 02-P24, explain precisely what must be true about the guard's *destructor* with respect to `noexcept` for it to be safely usable during stack unwinding from another exception, and what happens if that guarantee is violated.

### Level 5 — Integration-Adjacent (5)

**02-P37.** Design and implement a `TransactionalUpdate` RAII type: on construction it snapshots a piece of state; if the guarded scope completes normally, changes are committed (no-op on destruction); if the scope exits via exception, the destructor rolls back to the snapshot. Demonstrate both paths with a test-like `main()`.

**02-P38.** Implement a small "resource registry" (a global or singleton-like tracker of currently-live handles, for debugging leak detection) and wire at least two different RAII wrapper types (e.g. `ScopedFd` from 02-P17 and a `ScopedLock`-style type) to register/unregister themselves on construction/destruction. State what invariant, checked at program exit, would prove no leaks occurred.

**02-P39.** Design a `unique_ptr`-based plugin-ish ownership boundary: a factory function in one "module" returns `unique_ptr<Base>` to a "caller module," where `Base` has a virtual destructor. Explain, referencing 02-P25, why the virtual destructor is not optional here specifically because ownership crosses a type-erasure-like boundary (the caller only sees `Base`).

**02-P40. [DEBUG]** A production incident: a `shared_ptr<Session>` cycle (parent/child sessions, mirroring 02-P27) causes sessions to never be cleaned up, but only under a specific traffic pattern (sessions that create children but whose children never explicitly disconnect). Write the fix, and explain why `weak_ptr` is the correct tool for exactly the *child→parent* link specifically, not the *parent→child* link.

**02-P41.** Implement `make_widget_family()` returning three related `unique_ptr<Widget>` objects, and a consuming function that takes ownership of exactly one of them (by value, i.e. move) while the other two remain in the caller's scope. Demonstrate, via `= std::move(...)`, that attempting to use the moved-from `unique_ptr` afterward is well-defined (holds `nullptr`) but attempting to dereference it is not.

### Level 6 — Production Judgment (2)

**02-P42.** You're reviewing a PR that changes a class from owning a raw `int*` (with an explicit destructor doing `delete`) to owning a `unique_ptr<int>` instead, and removes the now-unnecessary destructor, copy constructor, and copy-assignment operator (previously `= delete`d) entirely. Identify what changes in the class's copyability as an *observable* consequence of this refactor (not just "it's cleaner code"), and state whether that's likely to be a desired or an accidental behavior change, and how you'd find out which without guessing.

**02-P43.** A codebase has a long-lived `shared_ptr<Config>` held by many subsystems, and a request comes in to add a `weak_ptr<Config>`-backed "hot reload" feature that swaps in a new `Config` at runtime. Write a short design note (a paragraph) on why simply reseating the *existing* `shared_ptr<Config>` that subsystems already hold does *not* achieve hot-reload for those subsystems, what indirection would actually be needed, and what new lifetime hazard that indirection introduces.

---

## Integration Challenge

**02-IC1.** Implement a family of RAII wrappers over a shared "resource registry," and prove no leak or double-free occurs under every construction path: normal construction, copy (where applicable), move, and exception during construction. Specifically:

- Build on `ScopedFd` (02-P17) or an equivalent handle wrapper, plus the resource registry from 02-P38.
- Add at least one type in the family that is move-only (mirroring `unique_ptr`) and at least one that is shared (mirroring `shared_ptr`, using an actual `shared_ptr<ControlBlock>` internally or your own hand-rolled refcounting from 02-P28).
- Construct at least one instance via a path that throws partway through (mirroring 02-P31's exception-safety fix), and demonstrate — via the registry's live-count invariant — that no resource was left registered as "live" after the throw unwinds.
- Demonstrate a copy of the shared-ownership type and a move of the move-only type, and show the registry's live count is correct after each.

This deliberately exercises RAII, ownership design, the Rule of 5, exception safety, and object lifetime together — the chapter's five hardest-to-separate concepts.

---

## Chapter Projects

- **P-1.2 — Scoped Resource Handle Family.** See [`PROJECT_ROADMAP.md`](../PROJECT_ROADMAP.md); full statement in `projects/level-1/scoped-resource/STATEMENT.md` once generated. Direct continuation of the Integration Challenge above, generalized into a reusable library (`scoped_file`, `scoped_timer`, a generic `scope_exit`).
- **P-1.4 — Copy/Move Instrumentation Harness.** See [`PROJECT_ROADMAP.md`](../PROJECT_ROADMAP.md); requires Ch03 concepts too (copy elision, NRVO) — attempt after Ch03, tracked here since its ownership-instrumentation half belongs to this chapter's material.

---

## Next

Solutions for every Quick Check and Problem above are in [`SOLUTIONS.md`](SOLUTIONS.md). Don't open it until you've attempted the problem.
