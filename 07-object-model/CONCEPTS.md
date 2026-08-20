# Chapter 07 — The Object Model: Layout, Polymorphism, and Raw Memory

> Prerequisites: [Chapter 01](../01-core-semantics/CONCEPTS.md), [Chapter 02](../02-lifetime-raii/CONCEPTS.md), [Chapter 03](../03-value-categories/CONCEPTS.md).
> This is the pivot chapter from "language user" to "implementer." Every implementation problem here should be answerable by pointing at a specific standard rule — not just "because that's how it works," but *why* the standard permits or requires it.

## Crash Course

### Object Representation, Alignment, and Padding

Every object occupies a region of storage with a size and an alignment requirement. Alignment is a power-of-two constraint on the address at which an object of a given type may start; the compiler inserts padding bytes between members (and sometimes after the last member) so that every member satisfies its own alignment and so that arrays of the type keep every element correctly aligned. `alignof(T)` reports this requirement; `sizeof(T)` includes the padding. Reordering members from largest-alignment to smallest is the classic technique for shrinking a struct's padding footprint.

```cpp
struct Bad  { char c; double d; char c2; };   // 24 bytes on a typical ABI: padding around d
struct Good { double d; char c; char c2; };   // 16 bytes: c and c2 pack together after d
```

### Bitfields, EBO, and `[[no_unique_address]]`

A **bitfield** declares a member to occupy a specific number of bits rather than a whole byte/word, packing several small flags or narrow-range values into shared storage: `struct Flags { unsigned a : 1; unsigned b : 1; unsigned c : 6; };` typically occupies a single byte instead of three. The tradeoffs: bitfield member order, exact packing, and even whether adjacent bitfields share an underlying storage unit are largely implementation-defined (not standard-guaranteed) — two compilers may lay the same bitfield struct out differently — and taking the address of a bitfield member is not allowed at all (there's no addressable byte boundary to point at), which rules out passing one by reference or pointer.

```cpp
struct Flags {
    unsigned readable : 1;
    unsigned writable : 1;
    unsigned reserved : 6;   // pads out to a full byte on most implementations
};
static_assert(sizeof(Flags) == 1);   // true on most, but not guaranteed by the standard
```

**Empty Base Optimization (EBO)** lets a base class with no non-static data members contribute zero bytes to a derived class's size, even though `sizeof(EmptyBase) == 1` on its own (every object needs a nonzero size so distinct objects have distinct addresses) — the compiler is permitted (and virtually every implementation does this) to overlay an empty base's "phantom" byte within the derived class's own padding rather than adding a full byte for it.

```cpp
struct Empty {};
struct WithBase : Empty { int x; };
static_assert(sizeof(WithBase) == sizeof(int));   // EBO: Empty contributes 0 bytes here
```

**`[[no_unique_address]]`** (C++20) extends the same idea to a non-base *member*: an empty (or otherwise non-distinguishable) member marked `[[no_unique_address]]` may be overlaid into another member's padding rather than reserving its own space — useful for a stateless policy/allocator/comparator member (common in generic library design, per Ch05) that would otherwise silently cost a full byte (or more, with alignment) purely for existing as a distinct member with no actual data.

```cpp
struct Empty {};
struct Widget {
    int value;
    [[no_unique_address]] Empty policy;   // may add 0 bytes instead of a full byte (+ padding)
};
```

Neither EBO nor `[[no_unique_address]]` is a standard *guarantee* in the strictest sense — a conforming implementation is merely *permitted* to apply the optimization, and in practice virtually every mainstream implementation does, but portable code should not rely on an exact `sizeof` result derived from assuming it, only on the *direction* of the effect (smaller-or-equal, never larger).

### Triviality and Standard Layout

A type is **trivial** if it has trivial default construction, copy/move, and destruction — meaning these operations do nothing beyond copying bytes (no user-provided logic runs). A type is **standard-layout** if it has a single access-control level for all non-static data members, no virtual functions, no virtual base classes, and (for a derived type) all non-static data members in one class in the hierarchy — the layout guarantees that make interop with C, `memcpy`, and `offsetof` well-defined. A type can be one, both, or neither independently: standard-layout is about *shape*, trivial is about *what special member functions do*. `std::is_trivial_v<T>` and `std::is_standard_layout_v<T>` query each independently; `std::is_pod_v` (deprecated in C++20) required both simultaneously.

### Inheritance, Virtual Functions, Vtables and Vptrs

A class with at least one virtual function gains a hidden pointer member (the **vptr**) pointing to a per-class table of function pointers (the **vtable**). A call through a base-class pointer/reference to a virtual function is resolved at runtime by following the vptr into the object's *actual* dynamic type's vtable — this is how `Base*` calling an overridden `virtual` reaches `Derived`'s override without the caller knowing `Derived` exists. This costs one pointer per polymorphic object (typically) and one indirect call per virtual dispatch, relative to a non-virtual call.

### RTTI, Multiple and Virtual Inheritance

RTTI (`typeid`, `dynamic_cast`) relies on the vtable carrying (or pointing to) type-identifying information, so it's only available for polymorphic types. Multiple inheritance means a derived object can contain more than one base subobject, each potentially at a different offset — a pointer conversion between base and derived across multiple inheritance may require adjusting the pointer's *value*, not just its static type (this is why multiple-inheritance vtables sometimes need "thunks"). Virtual inheritance solves the diamond problem (two paths to a common base) by ensuring only one shared instance of that base exists, at the cost of an extra indirection (a virtual base pointer/offset) to locate it, since its offset can no longer be a compile-time constant relative to the most-derived object.

### Object Lifetime Rules (`[basic.life]`), Aliasing, and `std::launder`

An object's lifetime begins once its constructor completes and ends when its destructor starts (or its storage is reused/released) — code that accesses an object outside that window, even through an apparently-valid pointer, is undefined behavior, independent of whether the bytes "look right." **Strict aliasing** forbids accessing an object through a pointer/reference of an unrelated type (with narrow exceptions: `char`/`unsigned char`/`std::byte`, and similar-layout types) — violating it is UB even when the access "happens to work" on a given compiler/optimization level. `std::launder` tells the compiler "the pointer you're about to use may point to a *new* object that started its lifetime here, even though I got this pointer from somewhere that doesn't know that" — needed after placement-new'ing a new object over old storage when you want to use an old pointer/reference to reach it.

### `bit_cast`, `start_lifetime_as`, and Placement Construction

`std::bit_cast<To>(from)` reinterprets an object's bit pattern as a different type *by value*, safely and without UB, provided both types are trivially copyable and the same size — it replaces the old, UB-laden `reinterpret_cast`-and-dereference or `memcpy`-into-a-`To` idiom with something the standard actually defines. `std::start_lifetime_as<T>` (C++23) explicitly begins a `T`'s lifetime over existing raw storage (e.g., bytes read from a file or received over a socket) without running a constructor, for trivially-copyable `T` — filling a real gap left by `[basic.life]`'s stricter rules. **Placement new** (`new (ptr) T(args...)`) constructs an object at existing storage rather than allocating new storage — this is the mechanism underlying `std::optional`, `std::variant`, small-buffer optimization, and any hand-rolled container that separates allocation from construction.

## Common Misconceptions

1. **"A struct with only public members and no virtual functions is automatically standard-layout."** Not necessarily — if it inherits from a base that itself has non-static data members, and the derived class also has its own non-static data members, standard-layout is violated (the rule requires all non-static data members to live in only one class in the hierarchy). Standard-layout has several independent conditions; having no virtuals is necessary but far from sufficient.

2. **"`sizeof(Derived)` is always `sizeof(Base) + sizeof each new member`."** No — compiler-inserted padding, the vptr (if `Base` or `Derived` introduces virtual functions), and multiple/virtual inheritance's extra bookkeeping can all make `sizeof(Derived)` larger than the naive sum; conversely, "tail padding reuse" (a derived class's member reusing a base class's trailing padding, permitted since C++11) can sometimes make it *smaller* than the naive sum would suggest.

3. **"`reinterpret_cast`ing a pointer and dereferencing it is always at least well-defined, if sometimes 'wrong.'"** No — dereferencing a `reinterpret_cast<T*>` of an object that isn't actually (or compatibly) a `T` is undefined behavior under strict aliasing, not merely "implementation-defined" or "likely correct." It may work by accident on a specific compiler and optimization level and break silently at a higher optimization level, which is precisely what makes it dangerous rather than merely inelegant.

4. **"Placing a new object over old storage via placement-new automatically makes every existing pointer to the old storage valid for the new object."** No — pointers/references obtained *before* the placement-new generally do not automatically refer to the new object, per `[basic.life]`; you may need `std::launder` on such a pointer before using it to access the new object, specifically when the new object's type could occupy different storage or overlap subobjects in a way the compiler can't automatically track.

5. **"Virtual function calls are resolved by looking at the pointer's static type, so calling through a `Base*` should call `Base`'s version unless you use `virtual` keyword tricks at the call site."** No — dispatch for a `virtual` function is *always* based on the object's dynamic type via the vtable, entirely independent of the static type of the pointer/reference used to call it; that's the entire point of declaring the function `virtual` in the first place. (The one genuine exception: calling a virtual function *during* construction/destruction dispatches to the currently-under-construction class's own version, not the eventual most-derived override — because the derived parts don't exist yet.)

6. **"Two types with identical member layouts can always be `reinterpret_cast` between each other safely, since the bytes line up."** No — even with byte-for-byte identical layout, strict aliasing still generally forbids accessing one through a pointer to the other (with narrow standard-defined exceptions like similar/layout-compatible types in specific contexts, or `char`-family access); "the bytes happen to line up" and "the access is well-defined" are different questions, and only `std::bit_cast` (a value-level reinterpretation, not a pointer-aliasing one) is unconditionally safe for same-size trivially-copyable types.

7. **"A `dynamic_cast` that fails just returns some garbage pointer, so I should always check for null."** For pointer types, a failed `dynamic_cast` does return `nullptr` (so checking is correct) — but for *reference* types, a failed `dynamic_cast` throws `std::bad_cast` instead, since there's no null reference to return; conflating the two forms' failure modes is a common source of either an unchecked null-pointer dereference or an uncaught exception, depending on which form was actually used.

8. **"Two compilers will lay out the same bitfield struct identically, as long as the bit-widths add up the same way."** No — bitfield packing order, whether adjacent bitfields share a storage unit, and even the underlying storage unit's size are largely implementation-defined; portable code should not assume a specific `sizeof` or byte-level layout for a bitfield struct across compilers without checking each target's documented behavior.

9. **"EBO and `[[no_unique_address]]` are standard guarantees, so I can rely on the exact `sizeof` they imply."** No — both are optimizations an implementation is merely *permitted* to apply (virtually all mainstream ones do, but a conforming implementation need not); portable code should rely only on the *direction* of the effect (size no larger than without the optimization), not a specific guaranteed byte count.

## Quick Checks

**07-QC1.** What's the difference between what `alignof(T)` reports and what `sizeof(T)` reports?

**07-QC2.** Name the four conditions (informally) required for a type to be standard-layout, and give one type that is standard-layout but not trivial.

**07-QC3.** When a `Base*` pointing at a `Derived` object calls a virtual function, what determines which override actually runs — the pointer's static type or the object's dynamic type?

**07-QC4.** Why does calling a virtual function from inside a base class's constructor not dispatch to a derived class's override, even if the object being constructed is ultimately a `Derived`?

**07-QC5.** What does `std::launder` actually do, in one sentence — and why is it sometimes needed even when the new object's bit pattern is byte-for-byte the same as the old one it replaced?

**07-QC6.** Why is `reinterpret_cast<T*>(&obj)` followed by a dereference potentially undefined behavior, even when `T` and `decltype(obj)` have identical sizes?

**07-QC7.** What's the essential difference between what virtual inheritance solves and what ordinary multiple inheritance already provides on its own?

**07-QC8.** In what sense does `std::bit_cast` avoid the UB that a `reinterpret_cast`-and-dereference (or `memcpy`-into-a-local) approach to "reinterpret these bytes as a different type" would risk?

**07-QC9.** Why can't you take the address of an individual bitfield member?

**07-QC10.** What's the difference between what EBO does for an empty *base class* and what `[[no_unique_address]]` does for an empty *member*?

## Problems

### Level 1 — Recognition

**07-P01.** For `struct S { char a; int b; char c; };` on a typical ABI where `int` requires 4-byte alignment, is `sizeof(S)` equal to `1 + 4 + 1 = 6`, or something larger? State what it actually is and why.

**07-P02.** Is `std::is_trivially_copyable_v<T>` true, false, or "it depends on T's members" for a `T` that has a user-defined (non-defaulted) copy constructor but a defaulted destructor and defaulted move operations?

**07-P03.** Given `struct Animal { virtual void speak() const; };` and `struct Dog : Animal { void speak() const override; };`, does `Animal` alone (with no members other than the virtual function) have `sizeof(Animal) == 0`, or something larger? Explain what occupies that space.

**07-P04.** Is `dynamic_cast` usable on a type with no virtual functions at all? Explain what specifically makes `dynamic_cast` (to a related type down/across a hierarchy) require polymorphism.

**07-P05.** Which of the following is/are guaranteed well-defined by the standard: (a) `std::bit_cast<Dest>(src)` where `sizeof(Dest) == sizeof(Src)` and both are trivially copyable; (b) `*reinterpret_cast<Dest*>(&src)` under the same size/triviality conditions; (c) `std::memcpy(&dest, &src, sizeof(Src))` followed by using `dest` as a fully-formed object of its own type, for trivially-copyable types of matching size?

### Level 2 — Prediction

**07-P06.**
```cpp
struct A { char a; double b; char c; };
struct B { double b; char a; char c; };
std::cout << sizeof(A) << " " << sizeof(B);
```
Predict the two sizes (assume 8-byte `double` alignment, 1-byte `char`) and explain the difference purely from member ordering.

**07-P07.**
```cpp
struct Base { virtual ~Base() = default; int x = 1; };
struct Derived : Base { int y = 2; };
std::cout << sizeof(Base) << " " << sizeof(Derived);
```
Predict both sizes, accounting for the vptr's presence and its typical size (assume an 8-byte vptr on a 64-bit ABI) and any padding.

**07-P08.**
```cpp
struct Base {
    Base() { greet(); }
    virtual void greet() const { std::cout << "Base\n"; }
};
struct Derived : Base {
    void greet() const override { std::cout << "Derived\n"; }
};
Derived d;
```
Predict the output. Does constructing a `Derived` print `"Base"` or `"Derived"`, and why?

**07-P09.**
```cpp
struct Empty {};
struct HasEmpty { Empty e; int x; };
std::cout << sizeof(Empty) << " " << sizeof(HasEmpty);
```
Predict both sizes. Is `sizeof(Empty)` ever `0`, and what does that imply for `sizeof(HasEmpty)` given the "no two distinct objects may share an address" rule?

**07-P10.**
```cpp
struct Base1 { int a; };
struct Base2 { int b; };
struct Derived : Base1, Base2 { int c; };
Derived d;
Base2* p2 = &d;
Base1* p1 = &d;
std::cout << (static_cast<void*>(p1) == static_cast<void*>(&d)) << " "
          << (static_cast<void*>(p2) == static_cast<void*>(&d));
```
Predict the output — is a `Base2*` obtained from `&d` guaranteed to have the *same address value* as `&d` itself, or might it differ? Explain in terms of subobject layout under multiple inheritance.

**07-P11.**
```cpp
alignas(16) struct Vec4 { float x, y, z, w; };
std::cout << alignof(Vec4) << " " << sizeof(Vec4);
```
Predict both values — does an explicit `alignas(16)` that's *larger* than the natural alignment of the members change `sizeof`, and if so how?

**07-P12.**
```cpp
struct S { int a; };
S s1{1};
S* p = &s1;
new (p) S{2};                 // placement-new a new S over s1's storage
std::cout << s1.a;            // (*)
```
Predict what happens at the line marked `(*)` — is reading `s1.a` directly (via the name `s1`, not through `p` or a laundered pointer) well-defined here, and does it print `1` or `2`? (Hint: consider whether `s1`'s original lifetime and the new object's lifetime are actually the same "object" from the language's point of view, for a type this simple.)

**07-P13.**
```cpp
struct A { virtual void f() { std::cout << "A::f "; } };
struct B : A { void f() override { std::cout << "B::f "; } };
struct C : A { void f() override { std::cout << "C::f "; } };
void call(A& a) { a.f(); }
B b; C c;
call(b); call(c);
```
Predict the output, and explain what's actually happening at the machine level each time `call` executes `a.f()` — specifically, what determines which vtable is consulted.

**07-P14.**
```cpp
union U { int i; float f; };
U u;
u.i = 42;
std::cout << u.f;               // reading a different active member
```
Predict what the standard says about this — is reading `u.f` right after writing `u.i` undefined behavior, implementation-defined, or something else? (Note: this is asking about the *general* rule, not the common but non-portable "type punning through a union" compiler extension some toolchains informally support.)

**07-P15.**
```cpp
struct Base { virtual void f() {} virtual ~Base() = default; };
struct Mid1 : virtual Base {};
struct Mid2 : virtual Base {};
struct Bottom : Mid1, Mid2 {};
Bottom b;
std::cout << sizeof(Bottom) << " vs a naive non-virtual diamond";
```
Without computing an exact byte count (that's implementation-defined), predict the *qualitative* outcome: does `Bottom` contain one `Base` subobject or two, and how does that compare to what a non-virtual diamond inheritance (`Mid1`/`Mid2` inheriting from `Base` non-virtually) would produce?

**07-P16.**
```cpp
struct Base {
    Base(const Base&) { std::cout << "Base copy\n"; }
    Base() = default;
};
struct Derived : Base {
    Derived(const Derived&) = default;
};
Derived d1;
Derived d2 = d1;
```
Predict the output. Does `Derived`'s defaulted copy constructor invoke `Base`'s user-defined copy constructor, and why does a *defaulted* special member function still correctly compose whatever each base/member's own corresponding special member function does?

**07-P17.**
```cpp
struct Trivial { int a; int b; };
struct NotQuiteTrivial { int a; NotQuiteTrivial() : a(0) {} };
std::cout << std::is_trivial_v<Trivial> << std::is_standard_layout_v<Trivial> << " "
          << std::is_trivial_v<NotQuiteTrivial> << std::is_standard_layout_v<NotQuiteTrivial>;
```
Predict the four boolean values (as `0`/`1`). Explain specifically why giving `NotQuiteTrivial` a user-provided default constructor affects its triviality but not its standard-layout status — i.e., why these two properties are independent axes, not one implying or excluding the other.

### Level 3 — Implementation

**07-P18.** Write a `struct PackedRGBA` with four `unsigned char` members that is guaranteed `sizeof(PackedRGBA) == 4` (no padding), and a separate `struct Measurement` with a `double`, a `char`, and an `int` member, reordered specifically to minimize total padding versus declaration order `double, char, int`. State the resulting `sizeof` for `Measurement` under a typical ABI (8-byte `double` alignment, 4-byte `int` alignment) and show the byte-by-byte layout in a comment.

**07-P19.** Implement a function `template<typename T> bool has_vtable()` using `std::is_polymorphic_v` that reports whether `T` has any virtual functions (including inherited ones), and demonstrate it against a non-polymorphic `struct Plain`, a polymorphic `struct Poly { virtual void f(); }`, and a `struct DerivedFromPoly : Poly {}`. Explain why `std::is_polymorphic_v` — rather than manually checking `sizeof(T) > sizeof(a hypothetical non-polymorphic version)` — is the correct, portable way to answer this.

**07-P20.** Implement a minimal hand-rolled "vtable" for a small closed set of shape types (`Circle`, `Square`) *without* using C++'s `virtual` keyword at all — a `struct ShapeVTable { double (*area)(const void*); }`, a `struct ShapeRef { const void* obj; const ShapeVTable* vtable; }`, and free functions constructing a `ShapeRef` for each concrete type — then call `.vtable->area(.obj)` polymorphically through `ShapeRef` for both types. Explain, having built one by hand, what the compiler-generated vptr/vtable mechanism is actually doing under the hood when you write ordinary `virtual`.

**07-P21.** Implement `template<typename Derived> struct Countable` (a CRTP-style base, callback to Ch05) that tracks a live-instance count via a `static inline` counter incremented/decremented in its constructor/destructor, and demonstrate two unrelated classes `Widget : Countable<Widget>` and `Gadget : Countable<Gadget>` maintaining *independent* counts. Explain, in terms of template instantiation, why `Countable<Widget>` and `Countable<Gadget>` are different types with their own independent static data, even though they share the same class template definition.

**07-P22.** Write a function `template<typename To, typename From> To safe_reinterpret(const From& from)` using `std::bit_cast` (guarded with a `static_assert` on matching size and trivial-copyability for both types) that reinterprets a `float`'s bit pattern as a `std::uint32_t` and back. Demonstrate round-tripping a specific `float` value (including a non-trivial one like `-0.0f` or a NaN) through the `uint32_t` representation and back, and explain why this is well-defined where the equivalent `reinterpret_cast<std::uint32_t*>(&f)`-and-dereference approach would not be.

**07-P23.** Implement a small fixed-capacity `class InPlaceOptional` (your own minimal, single-purpose stand-in for `std::optional<T>`, built to internalize the mechanism) holding raw storage `alignas(T) unsigned char storage_[sizeof(T)]` and a `bool has_value_`, with `emplace(Args&&...)` using placement-new, a destructor that conditionally calls the stored `T`'s destructor via `std::launder`'d pointer access, and a copy constructor that placement-news a copy when the source has a value. Explain specifically where in your implementation `std::launder` is required and what would go wrong (in principle, even if it happened to "work" on your specific compiler) if it were omitted.

**07-P24.** Implement `template<typename Base> void print_vtable_address(const Base& obj)` that extracts and prints the vptr's raw address from a polymorphic object using a `reinterpret_cast<const void* const*>(&obj)` dereference (this specific narrow use — reading the *first* pointer-sized slot of a polymorphic object as a `void*` — is a common, if implementation-detail-dependent, debugging technique, not something to rely on in production code). Demonstrate it distinguishing two different dynamic types passed as the same static `Base&`, and explicitly caveat in a comment why this technique is inherently non-portable (relies on a specific, common but unspecified vptr placement convention) even though it happens to work on most major ABIs.

**07-P25.** Write `struct Diamond : virtual Left, virtual Right` (where both `Left` and `Right` virtually inherit from a common `Base`), give `Base` a data member, and implement code that accesses that member through a `Diamond` object, through a `Left&` reference to the same object, and through a `Right&` reference to the same object — demonstrating that all three access paths reach the *same* `Base` subobject (e.g., by mutating it through one path and reading it through another). Explain why this "exactly one shared `Base`" property is precisely what virtual inheritance guarantees that ordinary (non-virtual) multiple inheritance from `Base` via both `Left` and `Right` would not.

**07-P26.** Implement a `template<typename T> struct AlignedBuffer` wrapping `alignas(alignof(T)) unsigned char data[sizeof(T)]` with `T* get()` returning a laundered pointer into the buffer once a `T` has actually been constructed there, and a static factory `construct(Args&&... args)` that placement-news the `T` and returns the `AlignedBuffer`. Write a `static_assert`-based test demonstrating that `AlignedBuffer<double>`'s storage is correctly 8-byte aligned even though the raw `unsigned char[]` array on its own would only guarantee 1-byte alignment without the `alignas`.

**07-P27.** Implement `enum class TypeTag { Int, Double, String }` and a hand-rolled `class TaggedUnion` holding a `TypeTag` plus a `union { int i; double d; std::string s; }` — and correctly implement its destructor, copy constructor, and copy assignment operator, each dispatching on the tag to construct/destroy/copy the *active* member specifically (since a union containing a non-trivial type like `std::string` disables the union's own implicitly-generated special member functions, requiring you to manage construction/destruction by hand via placement-new/explicit destructor calls). Demonstrate correct behavior (no leak, no double-destruction) for both the `Int` and `String` cases.

### Level 4 — Debugging

**07-P28.** [DEBUG]
```cpp
struct Base {
    ~Base() { std::cout << "~Base\n"; }
};
struct Derived : Base {
    ~Derived() { std::cout << "~Derived\n"; }
};
Base* p = new Derived();
delete p;
```
Predict the actual output (not the "intended" one) and explain precisely why deleting through a `Base*` here does or does not run `Derived`'s destructor — identify the single missing keyword that would fix this if the intended behavior was for both destructors to run, and state what category of bug this is (a memory leak, resource leak, or something more specific to the object model).

**07-P29.** [DEBUG]
```cpp
struct Point { int x; int y; };
Point p{1, 2};
float* fx = reinterpret_cast<float*>(&p.x);
std::cout << *fx;
```
Identify precisely why this is undefined behavior (name the specific rule being violated), and explain what "might happen" in practice at various optimization levels versus what the standard actually guarantees (i.e., why "it printed some garbage float value, so at least it didn't crash" is not evidence that the code is acceptable).

**07-P30.** [DEBUG]
```cpp
struct Widget {
    alignas(32) char buffer[64];
};
Widget* w = static_cast<Widget*>(std::malloc(sizeof(Widget)));
new (w) Widget();
```
A reviewer flags that this code has a latent alignment bug. Identify it precisely (hint: what alignment guarantee does `std::malloc` actually provide, versus what `Widget` requires because of its `alignas(32)` member), explain under what circumstances it would silently "happen to work" versus actually misbehave, and propose the correct allocation call to use instead.

**07-P31.** [DEBUG]
```cpp
struct Interface {
    virtual void process() = 0;
    virtual ~Interface() = default;
};
struct Impl : Interface {
    void process() override { std::cout << "processing\n"; }
};
std::vector<Interface> items;   // (*)
items.push_back(Impl{});
items[0].process();
```
Identify precisely what's wrong at the line marked `(*)` (this is a real, subtle bug, not a syntax error — it compiles, sort of, or fails to compile depending on `Interface`'s abstractness) and, assuming `Interface` were made concrete for the sake of argument, explain what "object slicing" would occur and why `items[0].process()` would not call `Impl::process()` even if this did compile.

**07-P32.** [DEBUG]
```cpp
struct Empty {};
struct Container {
    Empty e1;
    Empty e2;
    int x;
};
std::cout << sizeof(Container);
```
A developer expects `sizeof(Container)` to equal `sizeof(int)` (reasoning: "empty classes take zero space"). Explain precisely why this expectation is wrong — specifically, why `e1` and `e2`, despite each individually being an empty type, cannot simply overlap each other's storage inside `Container` (tie your answer to the "no two distinct objects of the same type may have the same address" rule), and state what `sizeof(Container)` actually is as a result.

**07-P33.** [DEBUG]
```cpp
struct Base { virtual void f() { std::cout << "Base::f\n"; } };
struct Derived : Base {
    void f() { std::cout << "Derived::f\n"; }   // note: no "override"
};
```
A reviewer wants to confirm `Derived::f` actually overrides `Base::f` rather than accidentally introducing an unrelated, hiding function with a slightly different signature (a classic bug when refactoring a base class's virtual function signature and forgetting to update every override). Show a one-line fix that would turn a *silent* signature mismatch (if one existed) into a *compile error* instead, and explain why relying on the naming/behavior alone (without this fix) leaves the mismatch scenario undetectable until runtime.

**07-P34.** [DEBUG]
```cpp
struct Base {};
struct Derived : Base { int extra; };
Base b;
Derived* d = static_cast<Derived*>(&b);   // (*)
std::cout << d->extra;
```
Identify precisely why the `static_cast` at `(*)` compiles but the subsequent access to `d->extra` is undefined behavior — `b` is genuinely a `Base` object, not a `Derived` one, so what specifically goes wrong, and why does the compiler not (and largely cannot) catch this at compile time?

**07-P35.** [DEBUG]
```cpp
struct S {
    int a;
    S(int v) : a(v) {}
};
alignas(S) unsigned char buf[sizeof(S)];
S* p1 = new (buf) S(1);
S* p2 = new (buf) S(2);          // placement-new a second S over the first, without destroying it
std::cout << p1->a;               // (*)
```
Identify precisely why reading `p1->a` at `(*)` is problematic even though `p1` and `p2` point at the same address and `S` is a simple, trivially-destructible type — specifically, what rule does constructing `p2`'s object *over* `p1`'s still-live object (without first calling `p1`'s destructor) violate, and would using `std::launder` on `p1` before this access fix the underlying problem, or merely address a different, narrower issue?

### Level 5 — Integration

**07-P36.** Implement a small `class PolymorphicVector` that stores a heterogeneous sequence of objects derived from an abstract `Shape` base (each with a virtual `area()`), using a `std::vector<std::unique_ptr<Shape>>` internally — but *additionally* implement a second, from-scratch storage strategy inside the same exercise: a `class InPlacePolymorphicVector<MaxSize>` that stores each `Shape`-derived object directly in an `alignas(...) unsigned char[MaxSize]`-backed slot (no heap allocation per element), using placement-new/manual destructor calls and a stored function pointer (or small hand-rolled vtable, per 07-P20) to know how to destroy each slot's actual contained type. Compare the two in a short explanation: what does the in-place version give up (a hard per-element size cap) in exchange for what it gains (no per-element heap allocation)?

**07-P37.** Design and implement `template<std::size_t Size, std::size_t Align> class InPlaceAny` — a minimal, fixed-capacity type-erased container (a deliberately smaller rehearsal for this chapter's Integration Challenge and for Project P-2.2) that can hold *any* object whose size and alignment fit within `Size`/`Align`, using `alignas(Align) unsigned char storage_[Size]`, a stored function-pointer-based "manager" (construct-from-source via move, destroy, and get-typeid, at minimum) captured at the point of construction via a template `emplace<T>(Args&&...)`, and correct `std::launder`'d access back to the stored object. Demonstrate storing an `int`, a small `struct`, and correctly rejecting (via `static_assert` or a thrown exception, your choice, stated explicitly) an oversized type at the call site.

**07-P38.** Implement `class Any` (a further-simplified stand-in for `std::any`, for the exercise) using heap allocation for the erased object rather than in-place storage — a `std::unique_ptr<void, void(*)(void*)>` or a small hand-rolled concept/model pattern (recall Ch05's type-erasure treatment) storing a pointer to a heap-allocated copy of whatever was assigned, plus enough type information (`std::type_index` or a raw function pointer performing the correct destruction) to safely destroy it. Then explicitly compare it against 07-P37's `InPlaceAny`: for a `sizeof(T) <= 16`-ish small type, which approach avoids a heap allocation, and for a large type that wouldn't fit `InPlaceAny`'s fixed capacity, which approach still works at all?

**07-P39.** Take a `struct Matrix3x3` (nine `float`s in a flat array) and implement two functions that both compute the same result — element-wise addition of two matrices — but one operates through `float*` pointers with an explicit `restrict`-equivalent hint (MSVC's `__restrict`, or state the portable alternative if targeting GCC/Clang) and the other operates through unannotated `float*` pointers that the compiler must conservatively assume might alias. Explain concretely, referencing strict aliasing rules, why the compiler is permitted to vectorize/reorder the `restrict`-annotated version more aggressively than the unannotated one, even though both versions are logically doing the identical computation for non-overlapping inputs.

**07-P40.** Implement a `class ScopedBytesAsObject<T>` that takes a `std::span<std ::byte>` (raw bytes, e.g. as if just read from a file or socket) and, using `std::start_lifetime_as<T>` (or, if targeting a pre-C++23 toolchain, the `std::launder`-and-placement-new-based emulation of the same idea, stated explicitly as an emulation), begins a `T`'s lifetime over those bytes for a trivially-copyable `T`, exposing `T& get()`. Explain specifically what problem `start_lifetime_as` solves that a straightforward `reinterpret_cast<T*>(bytes.data())` does not — i.e., why the bytes "already looking like a valid `T`" bit-for-bit is not, on its own, sufficient under `[basic.life]` to let you access them as a `T` without first legitimately starting that `T`'s lifetime.

### Level 6 — Production

**07-P41.** A production codebase's hot data structure is a `struct Particle { float x, y, z; float vx, vy, vz; bool active; }` stored in a `std::vector<Particle>`, and profiling shows the update loop (which reads/writes every field of every active particle every frame) is memory-bandwidth-bound, not compute-bound. Propose a concrete layout change (referencing this chapter's alignment/padding material and previewing Ch12's SoA/false-sharing territory, referenced not owned here) that would plausibly improve cache behavior, explain precisely *why* the proposed layout change helps (in terms of what data actually needs to be read together per iteration versus what's currently interleaved in memory), and state what downside or migration cost the change introduces for the rest of the codebase that currently accesses `Particle` as a single cohesive struct.

**07-P42.** Your team maintains a public C++ library type `struct Handle { void* impl; }` that has been ABI-stable (same layout) for three years across two platforms; a proposed change would add a new `bool valid` member. Reasoning strictly from this chapter's object-representation and layout material (not yet Ch08's ABI-versioning policy, which is out of scope here), identify precisely what about adding this member is and is not guaranteed to be safe for existing compiled client binaries that embed `Handle` by value (e.g., as a member of their own structs, or on the stack) versus clients that only ever hold a `Handle*`/`Handle&` obtained from your library's own factory functions — and state which of the two client patterns this specific `sizeof`/layout change silently breaks.

### Level 7 — Principal Reasoning

**07-P43.** You're reviewing a proposed change to a widely-used internal library's core `Result<T>` type, which currently stores its payload using a hand-rolled tagged union with raw placement-new/manual destructor dispatch (similar in spirit to 07-P27) for performance reasons, avoiding `std::variant`'s reported overhead in a hot path. A new engineer on the team proposes replacing the entire hand-rolled mechanism with `std::variant<T, Error>` for safety and maintainability, citing that hand-rolled placement-new/launder-based code is exactly the kind of thing this chapter has spent an entire chapter warning is easy to get subtly wrong. Reason through the tradeoff: identify at least two concrete correctness risks the current hand-rolled approach carries that `std::variant` would eliminate by construction, identify at least one concrete cost `std::variant` would reintroduce that motivated the original hand-rolled design, and propose a decision process (not just a verdict) for resolving this — specifically addressing what evidence (benchmarks, specific measured overhead, specific historical bugs in the current code) would actually change your recommendation one way or the other, since neither "correctness always wins" nor "we already measured this once, don't relitigate it" is, on its own, a sufficient argument in either direction.

### Level 3 — Implementation (continued: bitfields, EBO, `[[no_unique_address]]`)

**07-P44.** Write a `struct PackedFlags` using bitfields (`unsigned flag_a : 1; unsigned flag_b : 1; unsigned flag_c : 3; unsigned reserved : 3;` or similar, totaling 8 bits) and a sibling `struct Policy {};` (empty class) used as a compile-time-only tag member in a `struct Widget` two ways: once as an ordinary data member (`Policy policy_;`) and once annotated `[[no_unique_address]] Policy policy_;`. Print `sizeof(Widget)` for both versions on your toolchain. Explain why the bitfield's exact memory layout (bit order, whether `reserved` is even addressable) is implementation-defined and must not be assumed portable across compilers, and explain concretely what changed (or didn't) in `sizeof(Widget)` between the two `Policy`-member versions and why.

### Level 6 — Production (continued: EBO vs. `[[no_unique_address]]`)

**07-P45.** A library type `template<typename T, typename Deleter> class UniqueHandle` currently stores its `Deleter` as an ordinary data member (`Deleter deleter_;`), and profiling/size-auditing on an embedded target flags that `sizeof(UniqueHandle<T, DefaultDeleter>)` is larger than `sizeof(T*)` alone even though `DefaultDeleter` is a stateless empty class. Propose the fix using `[[no_unique_address]]` (stating the C++20 requirement explicitly, and what fallback — e.g., private inheritance from `Deleter` to piggyback on EBO — would be needed on a pre-C++20 toolchain), and explain precisely why a *stateful* `Deleter` (one holding, say, a `FILE*` or a logging callback) would not benefit from either technique the same way, and would still add to `sizeof(UniqueHandle)` regardless of which mechanism is used for the stateless case.

## Integration Challenge — 07-IC1

Implement a small in-place polymorphic buffer, `class PolyBox<Size, Align>`, that can hold any object of any type derived from an abstract `Drawable` interface (`virtual void draw() const = 0; virtual ~Drawable() = default;`), provided the concrete type's size and alignment fit within `Size`/`Align` — with no heap allocation.

1. Design the storage: `alignas(Align) unsigned char storage_[Size]`, and a template `template<typename T, typename... Args> void emplace(Args&&... args)` that `static_assert`s `sizeof(T) <= Size && alignof(T) <= Align && std::is_base_of_v<Drawable, T>`, then placement-news a `T` into `storage_`.
2. Correctly implement `draw()` (dispatching polymorphically to whatever concrete `Drawable`-derived type currently occupies the buffer), the destructor (destroying the currently-held object, tracked via a stored `Drawable*` you obtain and retain via `std::launder` at construction time — not by reinterpreting the buffer fresh on every access), and a `reset()` that destroys the current object without constructing a new one.
3. Justify, against the specific `[basic.life]` and strict-aliasing rules from this chapter's Crash Course, exactly why each of the following is necessary: (a) why the stored `Drawable*` must be obtained via `std::launder` rather than a plain `reinterpret_cast<Drawable*>(storage_)` taken fresh each time `draw()` is called; (b) why `alignas(Align)` on the raw byte array is required rather than relying on the array's natural (1-byte) alignment; (c) why placement-construction, rather than treating `storage_` as if it already held a valid `Drawable`-derived object from the start, is the only correct way to begin the contained object's lifetime.
4. Demonstrate `PolyBox<32, 8>` correctly holding, drawing, and destroying two different concrete `Drawable` subtypes in sequence (via `reset()` then a fresh `emplace<OtherType>`), and demonstrate that attempting to `emplace` a type whose `sizeof` exceeds `Size` fails to compile (via the `static_assert`), rather than silently corrupting adjacent memory.

## Chapter Projects

This chapter feeds directly into:

- **[P-2.2](../PROJECT_ROADMAP.md) SBO Variant Storage (`inplace_any<N>`)** — draws directly on 07-IC1's `PolyBox` design, 07-P26's `AlignedBuffer` alignment-correctness technique, and 07-P37's `InPlaceAny` fixed-capacity type-erasure exercise.
