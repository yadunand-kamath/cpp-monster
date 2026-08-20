# Chapter 07 — Solutions

## Quick Check Answers

**07-QC1.** `alignof(T)` reports the alignment *requirement* — the power-of-two byte boundary an object of type `T` must start on. `sizeof(T)` reports the total storage occupied, including any compiler-inserted padding needed to satisfy that alignment for the type itself and for elements of an array of `T`.

**07-QC2.** Informally: (1) a single access-control level for all non-static data members, (2) no virtual functions or virtual base classes, (3) no more than one class in the hierarchy contributing non-static data members, (4) that base class (if any) itself standard-layout and layout-compatible in the required sense. A type that is standard-layout but not trivial: a class with a user-provided (non-defaulted) constructor that still meets all the standard-layout conditions above — e.g. `struct S { int x; S() : x(0) {} };`.

**07-QC3.** The object's *dynamic* type — the actual most-derived type of the object being pointed/referred to — determines which override runs, entirely independent of the pointer's or reference's *static* type. This is resolved at runtime via the vptr into that dynamic type's vtable.

**07-QC4.** Because during a base class's constructor, the derived parts of the object haven't been constructed yet — the object's dynamic type, from the language's point of view, is still "under construction as the base," not yet the eventual derived type. Dispatching to a derived override on an object whose derived parts don't exist would call into uninitialized/nonexistent state, so the standard specifies that virtual calls during construction/destruction resolve to the current class's own version instead.

**07-QC5.** `std::launder` tells the compiler "treat this pointer as pointing to whatever object actually now occupies this storage, even though the pointer's provenance doesn't reflect that" — it doesn't change any bits, it only unblocks compiler optimizations/assumptions that would otherwise assume the pointer still refers to the original object. It's needed even when the new object's bit pattern is identical to the old one because `[basic.life]` cares about object *identity*, not byte content — the compiler is permitted to assume a pointer obtained before a placement-new still refers to the pre-existing object's type/value, an assumption that can be wrong even when nothing observably changed.

**07-QC6.** `T*` and `decltype(obj)*` are unrelated types (barring the standard's narrow aliasing exceptions), and strict aliasing forbids accessing an object through a pointer of an unrelated type regardless of matching size — matching size only means the *access wouldn't overrun memory*, it says nothing about whether the access is well-defined under the aliasing rules.

**07-QC7.** Multiple inheritance alone (two non-virtual paths to the same base) produces *two separate* base subobjects, each independently constructed and each at its own offset. Virtual inheritance additionally guarantees there is only *one* shared instance of that base regardless of how many derived paths lead to it — solving the diamond-duplication problem that plain multiple inheritance does not.

**07-QC8.** `std::bit_cast` performs a value-level reinterpretation defined by the standard to work correctly for any trivially-copyable, same-size pair of types — it never involves an aliasing pointer access at all. `reinterpret_cast`-and-dereference (or `memcpy`-into-a-local`-and-use`) risks strict-aliasing UB because it accesses memory through/as an unrelated type; `bit_cast` sidesteps that by not being a pointer-based aliasing operation in the first place.

## Problem Solutions

### Level 1 — Recognition

**07-P01.** Larger than 6 — typically `8` on a common 64-bit ABI. `int b` requires 4-byte alignment, so 3 padding bytes are inserted after `char a` to align `b`; then, because arrays of `S` must keep every element's `b` aligned too, 3 more padding bytes are added at the end after `char c` so that `sizeof(S)` is itself a multiple of `S`'s alignment (4). Net layout: `a`(1) + pad(3) + `b`(4) + `c`(1) + pad(3) = 8.

---

**07-P02.** False. `is_trivially_copyable_v<T>` requires *every* copy/move constructor and copy/move assignment operator that exists to be trivial (or deleted), and the destructor to be trivial — a user-defined (non-defaulted) copy constructor is, by definition, non-trivial, which alone disqualifies `T` from being trivially copyable regardless of what the destructor or move operations look like.

---

**07-P03.** Something larger — never `0`. Because `Animal` has a virtual function, it gains a vptr member, so `sizeof(Animal)` is at least the size of one pointer (commonly 8 bytes on a 64-bit ABI). The space is occupied by the hidden vptr the compiler inserts to support virtual dispatch.

---

**07-P04.** No — `dynamic_cast` to a related type in a polymorphic hierarchy fundamentally requires runtime type information, which only exists for polymorphic types (those with at least one virtual function). Attempting a `dynamic_cast` involving a non-polymorphic type is a compile error (for the cross-cast/downcast forms that need RTTI) precisely because there's no vtable to carry or look up the needed type identity at runtime.

---

**07-P05.** Only (a) is guaranteed well-defined by the standard. (b) is undefined behavior under strict aliasing — matching size doesn't exempt a `reinterpret_cast`-and-dereference from the aliasing rules. (c) is *not* guaranteed to produce a "fully-formed object of its own type" merely by copying bytes — for types with non-trivial invariants or where the type isn't trivially copyable, `memcpy`ing bytes into raw storage doesn't itself start a `T`'s lifetime; using `dest` as a genuine object of its type without something like placement-new or (C++23) `start_lifetime_as` is exactly the class of gap `std::launder`/`start_lifetime_as` exist to close (see 07-QC5/07-QC8).

### Level 2 — Prediction

**07-P06.** `sizeof(A)` is `24`, `sizeof(B)` is `16`. In `A`, `char a` (1 byte) forces 7 bytes of padding before `double b` (needs 8-byte alignment), then `char c` (1 byte) followed by 6 bytes of trailing padding to make the whole struct a multiple of 8 → `1+7+8+1+6=24`. In `B`, `double b` comes first (no leading padding needed), then `a` and `c` (1 byte each) pack together right after it with no gaps, and the total (`8+1+1=10`) only needs 6 bytes of trailing padding to reach a multiple of 8 → wait, recomputing: `8+1+1=10`, rounded up to the next multiple of 8 is `16`. So `B` saves 8 bytes purely by ordering the largest-alignment member first.

---

**07-P07.** `sizeof(Base)` is `16`: vptr (8 bytes) + `int x` (4 bytes) + 4 bytes trailing padding to keep the struct's size a multiple of the vptr's 8-byte alignment. `sizeof(Derived)` is also `16`: it reuses `Base`'s existing 4 bytes of trailing padding for `int y` (a "tail padding reuse," permitted since C++11) rather than adding a fresh 8-byte block — vptr(8) + x(4) + y(4) = 16, no additional padding needed since that's already a multiple of 8.

---

**07-P08.** Output is `Base`. `Derived`'s constructor first invokes `Base`'s constructor (base classes are always initialized before the derived class's own body runs), and at that point the object's dynamic type, from the virtual-dispatch machinery's point of view, is still `Base` — `Derived`'s override isn't reachable yet because the parts that make it a `Derived` haven't been constructed. This matches 07-QC4's rule exactly.

---

**07-P09.** `sizeof(Empty)` is `1` (never `0` — the standard requires every complete object type to have size ≥ 1, specifically so that no two distinct objects of that type could ever have the same address, e.g. in an array). `sizeof(HasEmpty)` is `8`: `Empty e` (1 byte, though "empty base optimization" tricks don't apply to non-base *members*) + 3 bytes padding to align `int x` (4-byte alignment) + `x` (4 bytes) = 8. Because `e` is a non-static data *member* (not a base class), it cannot be given zero additional footprint even though its own `sizeof` is minimal — it still needs its own distinct byte(s), unlike the empty-base-optimization case that can apply when an empty type is inherited from rather than held as a member.

---

**07-P10.** Output is `10` (true for `p1`, false for `p2`) is one very plausible outcome, though the *specific* which-one-matches is implementation-defined; the point the problem is testing is that it is **not guaranteed that both match**. In practice, the first base subobject (`Base1`) commonly sits at offset 0 (making `p1`'s address equal `&d`'s), while `Base2` sits at some non-zero offset after `Base1`'s bytes, making `p2`'s numeric address value *different* from `&d`'s even though both are "correct" pointers to the same `Derived` object — this is precisely why converting a `Derived*` to a `Base2*` may require the compiler to adjust the pointer's value, not just relabel its static type.

---

**07-P11.** `alignof(Vec4)` is `16`, `sizeof(Vec4)` is `16`. The natural layout of four `float`s is already `16` bytes with `4`-byte natural alignment; `alignas(16)` raises the *required alignment* to `16` without needing to add any padding *within* the struct (since `16` already divides evenly into the natural 16-byte size) — but note that a `sizeof` that wasn't already a multiple of the requested alignment would have to grow via trailing padding to satisfy it.

---

**07-P12.** `s1.a` prints `1`, not `2` — and this line's behavior is *not guaranteed* to observe the placement-new's effect, precisely because `s1` (the name) refers to lifetime tracking for the *original* object, and reading through that original name/reference after a new object has been constructed in its storage does not reliably see the new object per `[basic.life]`, even for a type this simple where `S`'s bit pattern for `{2}` is perfectly well-formed. To reliably observe `2`, the code would need to go through a pointer returned by (or laundered from) the placement-new expression itself — e.g. `p->a` or `std::launder(p)->a` — not the original name `s1`.

---

**07-P13.** Output is `B::f C::f `. Each call to `a.f()` is resolved via `a`'s vptr, which points at whichever vtable corresponds to the object's *actual* dynamic type (`B` for the first call, `C` for the second) — the parameter `a` is declared as `A&`, but that static type only determines what's *legal to call*, not which override actually executes. At the machine level, each `a.f()` reads the vptr from the referenced object's memory, indexes into that vtable for `f`'s slot, and calls through the resulting function pointer — the same code path runs both times, but the vptr it dereferences differs because `b` and `c` are different objects.

---

**07-P14.** This is undefined behavior in general C++ — the standard does not define what happens when you write through one union member and read through a *different* member unless that different member is part of a standard-layout common initial sequence with the written member (a narrow special case), or you're specifically using the `char`/`unsigned char`/`std::byte` exception. `int` and `float` share no such relationship, so reading `u.f` after writing `u.i` is UB by the general rule, even though many real compilers historically support "type punning through a union" as a widely-relied-upon, non-standard extension.

---

**07-P15.** `Bottom` contains exactly **one** shared `Base` subobject — that's precisely what the `virtual` keyword on `Mid1`/`Mid2`'s inheritance from `Base` guarantees, per 07-QC7. A *non-virtual* diamond (both `Mid1` and `Mid2` inheriting from `Base` ordinarily) would instead produce **two independent** `Base` subobjects inside `Bottom`, each separately constructed, and any attempt to refer to "the" `Base` part of a `Bottom` through an unqualified name would be ambiguous (requiring explicit `Mid1::Base` / `Mid2::Base` disambiguation).

---

**07-P16.** Output is `Base copy`. `Derived`'s copy constructor is explicitly defaulted, but "defaulted" doesn't mean "does nothing" — a defaulted special member function is defined by the compiler to correctly invoke the corresponding special member function of every base and member in turn. Since `Base` has a user-defined copy constructor, `Derived`'s defaulted copy constructor calls it as part of correctly copy-constructing the `Base` portion of `d2` from `d1`'s `Base` portion — "defaulted" composes the pieces correctly, it doesn't skip them.

---

**07-P17.** Output is `11 01` — i.e. `Trivial` is both trivial (`1`) and standard-layout (`1`); `NotQuiteTrivial` is not trivial (`0`) but is standard-layout (`1`). Giving `NotQuiteTrivial` a user-provided (non-defaulted) default constructor makes default-construction "do something" beyond leaving bytes alone, which disqualifies triviality (triviality specifically requires special member functions to be compiler-generated/do-nothing). But standard-layout's conditions (single access level, no virtuals, one class contributing data members, etc.) say nothing at all about what the constructors' *bodies* do — only about the type's *shape* — so a user-provided constructor has no bearing on standard-layout status. This is exactly why the two properties are independent axes rather than one implying the other.

### Level 3 — Implementation

**07-P18.**
```cpp
struct PackedRGBA {
    unsigned char r, g, b, a;   // 4 members, each 1-byte-aligned, no padding needed at all
};
static_assert(sizeof(PackedRGBA) == 4);

// double(8) first — no leading padding; char(1) + int(4) reordered to minimize gaps:
struct Measurement {
    double value;   // offset 0, 8 bytes
    int    count;   // offset 8, 4 bytes (already aligned, no padding needed before it)
    char   flag;    // offset 12, 1 byte
    // 3 bytes trailing padding to reach a multiple of 8 (double's alignment) -> total 16
};
static_assert(sizeof(Measurement) == 16);
```
Declaration order `double, char, int` would instead need padding *between* `char` and `int` (3 bytes, to align `int` to 4) in addition to trailing padding — reordering to put the largest-alignment member first, then the remaining members packed tightly, minimizes total padding to just the unavoidable trailing bytes.

---

**07-P19.**
```cpp
template<typename T>
bool has_vtable() { return std::is_polymorphic_v<T>; }

struct Plain { int x; };
struct Poly { virtual void f(); };
struct DerivedFromPoly : Poly {};

static_assert(!std::is_polymorphic_v<Plain>);
static_assert(std::is_polymorphic_v<Poly>);
static_assert(std::is_polymorphic_v<DerivedFromPoly>);   // inherits polymorphism
```
`std::is_polymorphic_v` is correct and portable because it queries the language-level property directly (as reported by the compiler's own semantic understanding of the type), independent of any particular ABI's vptr size or placement convention. Comparing `sizeof(T)` against a "hypothetical non-polymorphic version" is not even generally computable (there's no such hypothetical type to measure) and would in any case only be an indirect, ABI-dependent proxy for the real property.

---

**07-P20.**
```cpp
struct ShapeVTable { double (*area)(const void*); };

struct Circle { double radius; };
struct Square { double side; };

double circle_area(const void* self) { return 3.14159265 * static_cast<const Circle*>(self)->radius * static_cast<const Circle*>(self)->radius; }
double square_area(const void* self) { return static_cast<const Square*>(self)->side * static_cast<const Square*>(self)->side; }

inline constexpr ShapeVTable circle_vtable{circle_area};
inline constexpr ShapeVTable square_vtable{square_area};

struct ShapeRef { const void* obj; const ShapeVTable* vtable; };
ShapeRef make_shape_ref(const Circle& c) { return {&c, &circle_vtable}; }
ShapeRef make_shape_ref(const Square& s) { return {&s, &square_vtable}; }

Circle c{2.0}; Square s{3.0};
ShapeRef refs[] = {make_shape_ref(c), make_shape_ref(s)};
for (auto& r : refs) std::cout << r.vtable->area(r.obj) << " ";
```
Having built this by hand, the compiler-generated mechanism for ordinary `virtual` is doing exactly this: each polymorphic object gets a hidden pointer (the vptr) analogous to `ShapeRef::vtable`, pointing to a per-class table of function pointers (the compiler-generated vtable) analogous to `ShapeVTable`; a virtual call is compiled into "load the vptr, index into the vtable for this function's slot, call through the resulting pointer, passing `this` as the object" — precisely `r.vtable->area(r.obj)`'s shape, just done automatically and hidden from the programmer.

---

**07-P21.**
```cpp
template<typename Derived>
struct Countable {
    Countable() { ++count(); }
    Countable(const Countable&) { ++count(); }
    ~Countable() { --count(); }
    static int& count() { static int c = 0; return c; }
};

struct Widget : Countable<Widget> {};
struct Gadget : Countable<Gadget> {};

Widget w1, w2; Gadget g1;
std::cout << Widget::count() << " " << Gadget::count();   // 2 1
```
`Countable<Widget>` and `Countable<Gadget>` are two entirely separate template *instantiations* — each instantiation of a class template is its own distinct type with its own independently-generated code and its own independent set of static data members, even though both instantiations originated from the same template definition. This is exactly the CRTP idiom from Ch05: parameterizing the base on the derived type forces a fresh instantiation (and thus a fresh, independent `static int c`) per derived class.

---

**07-P22.**
```cpp
template<typename To, typename From>
To safe_reinterpret(const From& from) {
    static_assert(sizeof(To) == sizeof(From));
    static_assert(std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>);
    return std::bit_cast<To>(from);
}

float f = -0.0f;
std::uint32_t bits = safe_reinterpret<std::uint32_t>(f);
float back = safe_reinterpret<float>(bits);
// back == -0.0f (bit-identical round trip, including the sign bit of negative zero and any NaN payload)
```
This is well-defined because `std::bit_cast` is specified by the standard to produce a value of type `To` with the same bit representation as `from`, for any pair of same-size trivially-copyable types — no pointer aliasing occurs at all, it's a pure value-to-value reinterpretation the compiler is required to support correctly. `reinterpret_cast<std::uint32_t*>(&f)` followed by a dereference instead accesses the `float` object's storage *through* an unrelated pointer type, which is exactly the strict-aliasing violation this chapter's Crash Course and 07-QC6/07-QC8 describe.

---

**07-P23.**
```cpp
template<typename T>
class InPlaceOptional {
    alignas(T) unsigned char storage_[sizeof(T)];
    bool has_value_ = false;
public:
    template<typename... Args>
    void emplace(Args&&... args) {
        if (has_value_) reset();
        new (storage_) T(std::forward<Args>(args)...);
        has_value_ = true;
    }
    void reset() {
        if (has_value_) {
            std::launder(reinterpret_cast<T*>(storage_))->~T();
            has_value_ = false;
        }
    }
    InPlaceOptional(const InPlaceOptional& other) : has_value_(other.has_value_) {
        if (has_value_) new (storage_) T(*std::launder(reinterpret_cast<const T*>(other.storage_)));
    }
    ~InPlaceOptional() { reset(); }
    T* get() { return has_value_ ? std::launder(reinterpret_cast<T*>(storage_)) : nullptr; }
};
```
`std::launder` is required at every point where a pointer into `storage_` is used to *access* the currently-held `T` (in `reset()`, in the copy constructor's read of the source, and in `get()`) — specifically because `storage_` is raw `unsigned char[]` storage that has had a `T` placement-constructed into it, and the compiler is otherwise permitted to assume a `reinterpret_cast<T*>` of that array still points to "an array of `unsigned char`," not the `T` object now living there. Omitting `launder` would not necessarily crash — it could "work" under low optimization — but the compiler would be within its rights, especially at higher optimization levels, to reorder/eliminate accesses on the (technically licensed) assumption that no `T` object actually exists at that address, silently corrupting behavior.

---

**07-P24.**
```cpp
template<typename Base>
void print_vtable_address(const Base& obj) {
    // Reads the object's first pointer-sized slot, which on most major ABIs (Itanium C++ ABI,
    // MSVC ABI) is where the vptr lives for a polymorphic type. NOT standard-guaranteed.
    const void* const* vptr_slot = reinterpret_cast<const void* const*>(&obj);
    std::cout << *vptr_slot << "\n";
}

struct A { virtual ~A() = default; };
struct B : A {};
B b1, b2;
A& ref = b1;
print_vtable_address(ref);   // prints b1's vtable address
print_vtable_address(b2);    // same vtable address as b1 (same dynamic type), different from an unrelated hierarchy's
```
This is inherently non-portable: the standard says nothing about *where* within an object's storage the vptr lives, or whether a vptr is used at all to implement virtual dispatch (some conceivable, though uncommon, implementations could use different techniques). This code relies specifically on the widely-shared, but unspecified, convention of placing the vptr as the object's first member — a convention essentially all mainstream ABIs happen to follow, but nothing in the standard requires it.

---

**07-P25.**
```cpp
struct Base { int value = 0; };
struct Left  : virtual Base {};
struct Right : virtual Base {};
struct Diamond : Left, Right {};

Diamond d;
d.Left::value = 42;                          // mutate through the Left path
std::cout << d.Right::value << " " << d.value;   // both print 42 — same shared Base
```
Because `Left` and `Right` both inherit `Base` *virtually*, `Diamond` contains exactly one `Base` subobject (per 07-QC7/07-P15), so mutating `value` through `Left`'s path and reading it through `Right`'s path (or through `Diamond` directly) all observe the same underlying storage. If `Left`/`Right` instead inherited `Base` non-virtually, `Diamond` would contain two independent `Base` subobjects — mutating through one path would leave the other completely unaffected, and unqualified `d.value` would be ambiguous without `Left::`/`Right::` disambiguation.

---

**07-P26.**
```cpp
template<typename T>
struct AlignedBuffer {
    alignas(alignof(T)) unsigned char data[sizeof(T)];

    T* get() { return std::launder(reinterpret_cast<T*>(data)); }

    template<typename... Args>
    static AlignedBuffer construct(Args&&... args) {
        AlignedBuffer buf;
        new (buf.data) T(std::forward<Args>(args)...);
        return buf;
    }
};

static_assert(alignof(AlignedBuffer<double>) == alignof(double));
AlignedBuffer<double> buf = AlignedBuffer<double>::construct(3.14);
static_assert(reinterpret_cast<std::uintptr_t>(&buf) % alignof(double) == 0);
```
The `alignas(alignof(T))` on `data` is what forces the compiler to place the whole `AlignedBuffer<T>` object (and therefore the `data` array within it) at an address satisfying `T`'s alignment — a bare `unsigned char data[sizeof(T)]` without `alignas` would only be guaranteed 1-byte alignment, meaning placement-constructing a `double` (needing 8-byte alignment) into it would be undefined behavior at construction time whenever the buffer happened to land at a non-8-aligned address, exactly the class of bug in 07-P30. `get()` returns a `std::launder`'d pointer because `data` is raw storage the compiler is otherwise entitled to still consider "an array of `unsigned char`" even after a `T` has been placement-constructed into it.

---

**07-P27.**
```cpp
enum class TypeTag { Int, Double, String };

class TaggedUnion {
    TypeTag tag_;
    union { int i_; double d_; std::string s_; };
public:
    TaggedUnion(int v) : tag_(TypeTag::Int), i_(v) {}
    TaggedUnion(double v) : tag_(TypeTag::Double), d_(v) {}
    TaggedUnion(std::string v) : tag_(TypeTag::String) { new (&s_) std::string(std::move(v)); }

    TaggedUnion(const TaggedUnion& other) : tag_(other.tag_) {
        switch (tag_) {
            case TypeTag::Int:    i_ = other.i_; break;
            case TypeTag::Double: d_ = other.d_; break;
            case TypeTag::String: new (&s_) std::string(other.s_); break;
        }
    }
    TaggedUnion& operator=(const TaggedUnion& other) {
        if (this == &other) return *this;
        destroy_active();
        tag_ = other.tag_;
        switch (tag_) {
            case TypeTag::Int:    i_ = other.i_; break;
            case TypeTag::Double: d_ = other.d_; break;
            case TypeTag::String: new (&s_) std::string(other.s_); break;
        }
        return *this;
    }
    ~TaggedUnion() { destroy_active(); }

private:
    void destroy_active() {
        if (tag_ == TypeTag::String) s_.~basic_string();
    }
};
```
Because the union contains a non-trivial type (`std::string`), the compiler disables the union's own implicitly-generated constructor/destructor/copy-assignment — only the *active* member (tracked externally via `tag_`) may legitimately be constructed at any time, so every constructor, the copy constructor, copy assignment, and the destructor must dispatch on `tag_` to placement-construct or explicitly destroy exactly the member that is actually live, mirroring `std::variant`'s own internal discipline (previewed here, formalized in 07-P43's tradeoff discussion). No leak occurs because `destroy_active()` explicitly destroys the live `std::string` before any reassignment or at destruction; no double-destruction occurs because `int`/`double` members need no destructor call, and the `std::string` case is the only branch that ever calls one.

### Level 4 — Debugging

**07-P28.** [DEBUG] Output is `~Base` only — `Derived`'s destructor never runs. Deleting through a `Base*` calls the *static* type's destructor unless that destructor is `virtual`; `Base`'s destructor here is an ordinary (non-virtual) member function, so `delete p` only knows to call `~Base()`. The single missing keyword is `virtual` on `Base`'s destructor: `virtual ~Base() { ... }`. This is a **resource leak specific to the object model** — deleting a derived object through a base pointer with a non-virtual base destructor is explicitly undefined behavior per the standard (not merely "skips the derived destructor" — real implementations often do exactly that, but it's formally UB), and any resources `Derived`'s destructor would have released (that `Base`'s doesn't) leak.

---

**07-P29.** [DEBUG] This violates strict aliasing: `float*` and `int*` (the type of `&p.x`) are unrelated types, and dereferencing a pointer of one type to access an object actually stored as the other is undefined behavior regardless of matching size (per 07-QC6). At low optimization, this "might happen to work" by printing whatever garbage floating-point interpretation of the `int`'s bits happens to result, giving the illusion of well-defined (if semantically nonsensical) behavior; at higher optimization levels, the compiler is licensed to assume no aliasing occurred at all and may reorder, cache, or eliminate the read/write in ways that produce genuinely inconsistent or surprising results — "it printed some garbage float and didn't crash" only demonstrates the absence of an *observed* problem on that specific build, not the absence of UB.

---

**07-P30.** [DEBUG] The bug: `std::malloc` only guarantees alignment suitable for the *strictest fundamental alignment* the platform supports by default (commonly matching `alignof(std::max_align_t)`, often 16 on most modern platforms) — but `Widget` requires 32-byte alignment because of its `alignas(32)` member, which exceeds what plain `malloc` promises. This "happens to work" whenever `malloc`'s actual returned address coincidentally satisfies the stricter 32-byte requirement (common on some allocators/platforms for smaller allocations, purely by luck of the allocator's internal alignment choices) and misbehaves (as UB, potentially manifesting as crashes or, on some hardware, incorrect SIMD loads) whenever it doesn't. The correct call is `std::aligned_alloc(32, sizeof(Widget))` (with `sizeof(Widget)` rounded up to a multiple of the alignment, as `aligned_alloc` requires), or a platform-specific aligned allocation function.

---

**07-P31.** [DEBUG] `Interface` is an abstract class (it has a pure virtual function `process() = 0`), so `std::vector<Interface> items` fails to compile outright — a `vector` needs to be able to construct/store `Interface` objects by value, and an abstract class cannot be instantiated. Even setting that aside and imagining `Interface` were made concrete: storing `Impl{}` into a `vector<Interface>` would **object-slice** it — only the `Interface`-sized/shaped portion of `Impl{}` gets copied into the vector's storage, discarding any of `Impl`'s own additional state and, critically, resetting the stored object's vptr to `Interface`'s own vtable rather than `Impl`'s. `items[0].process()` would then dispatch to whatever `Interface`'s own (now-hypothetically-concrete) `process()` does, never reaching `Impl::process()`, because the stored object's actual dynamic type at that point genuinely is `Interface`, not `Impl` — slicing isn't a dispatch bug, it's a genuine loss of the derived object's identity at the moment of the copy.

---

**07-P32.** [DEBUG] Even though `Empty` has `sizeof(Empty) == 1` on its own, the standard's rule that no two *distinct objects* may have the same address means `e1` and `e2` — two separate non-static data member objects within the same `Container` — must be assigned different addresses; the compiler cannot let them overlap merely because each is individually "zero-content." So `Container` needs at least 1 byte for `e1` and a genuinely distinct 1 byte for `e2`, plus padding to align `x` and the struct's overall size. In practice, `sizeof(Container)` is typically `8`: `e1`(1) + `e2`(1) + 2 padding bytes to align `x`(4-byte alignment) + `x`(4) = 8. (Empty *base classes*, unlike empty *members*, can be given zero additional footprint via the empty-base-optimization the standard explicitly permits — but that optimization does not apply to ordinary data members like `e1`/`e2` here.)

---

**07-P33.** [DEBUG] One-line fix: add `override` to `Derived::f`'s declaration — `void f() override { ... }`. If `Derived::f`'s signature ever silently drifted from `Base::f`'s (e.g., a `const` qualifier dropped, a parameter type changed) due to an unsynchronized refactor, the *unfixed* code would still compile: `Derived::f` would simply become an unrelated, non-overriding function that happens to share a name, silently hiding `Base::f` from name lookup on `Derived` objects without actually participating in virtual dispatch — a call through a `Base*`/`Base&` would then still reach `Base::f`, and the mismatch would only become apparent at runtime (or never, if nobody tests the polymorphic call path directly). With `override` present, any such signature mismatch becomes an immediate, unambiguous **compile error**, because `override` explicitly asserts "this function overrides a virtual function from a base class," and the compiler verifies that assertion.

---

**07-P34.** [DEBUG] The `static_cast` at `(*)` compiles because `static_cast` between related pointer types (base ↔ derived) is always syntactically legal — the compiler trusts the programmer's assertion that the conversion is valid, and does not (and generally cannot, for a `static_cast`) verify at compile time or runtime that `b` is *actually* a `Derived` object. But `b` is genuinely and only ever a `Base` object — there is no `Derived::extra` member actually present in `b`'s storage at all. Accessing `d->extra` therefore reads memory beyond (or simply not belonging to) the real `Base b`'s storage — undefined behavior, commonly manifesting as reading garbage adjacent memory, but with no guarantee of any particular observable outcome. The compiler doesn't catch this because `static_cast`'s contract for base/derived pointer conversions is explicitly the programmer's responsibility to get right (unlike `dynamic_cast`, which *does* check, at the cost of requiring polymorphism and runtime overhead).

---

**07-P35.** [DEBUG] Constructing `p2`'s `S` object directly over `p1`'s storage, without first calling `p1`'s destructor, violates `[basic.life]`'s requirement that an object's lifetime must end (destructor run, or storage released/reused in an equally defined way) before a new object's lifetime legitimately begins in overlapping storage — even for a trivially-destructible type like this `S`, the *language-level* rule about object identity and lifetime is still violated, independent of whether skipping the (here, trivial, no-op) destructor would "cost" anything observable. Using `std::launder` on `p1` before this specific access would **not** fix the underlying problem: `launder` helps a pointer correctly refer to a *legitimately new* object occupying storage where an old one's lifetime has properly ended — it does nothing to retroactively make `p1` a valid way to access `p2`'s object when `p1`'s own object's lifetime was never properly ended in the first place. The actual fix is to call `p1->~S()` before placement-constructing `p2`.

### Level 5 — Integration

**07-P36.**
```cpp
struct Shape { virtual double area() const = 0; virtual ~Shape() = default; };
struct Circle : Shape { double r; double area() const override { return 3.14159265 * r * r; } };
struct Square : Shape { double s; double area() const override { return s * s; } };

class PolymorphicVector {
    std::vector<std::unique_ptr<Shape>> items_;
public:
    template<typename T, typename... Args> void add(Args&&... args) {
        items_.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }
    double total_area() const { double t = 0; for (auto& s : items_) t += s->area(); return t; }
};

template<std::size_t MaxSize>
class InPlacePolymorphicVector {
    struct Slot { alignas(std::max_align_t) unsigned char storage[MaxSize]; void (*destroy)(void*); Shape* (*get)(void*); };
    std::vector<Slot> slots_;
public:
    template<typename T, typename... Args> void add(Args&&... args) {
        static_assert(sizeof(T) <= MaxSize);
        Slot slot;
        new (slot.storage) T(std::forward<Args>(args)...);
        slot.destroy = [](void* p) { std::launder(reinterpret_cast<T*>(p))->~T(); };
        slot.get = [](void* p) -> Shape* { return std::launder(reinterpret_cast<T*>(p)); };
        slots_.push_back(slot);
    }
    double total_area() const {
        double t = 0;
        for (auto& slot : slots_) t += slot.get(const_cast<void*>(static_cast<const void*>(slot.storage)))->area();
        return t;
    }
    ~InPlacePolymorphicVector() { for (auto& slot : slots_) slot.destroy(slot.storage); }
};
```
**Trade-off:** the in-place version gives up the ability to store any `Shape`-derived type whose `sizeof` exceeds `MaxSize` — it has a hard per-element size ceiling baked into `Slot`'s fixed-size buffer. In exchange, it avoids a heap allocation per stored element (the `std::vector<std::unique_ptr<Shape>>` version allocates once for each `make_unique` call, plus the vector's own backing-array allocations); the in-place version's `slots_` vector still allocates for its own backing array, but each individual `Shape`-derived object lives inline within that array rather than behind its own separate heap block.

---

**07-P37.**
```cpp
template<std::size_t Size, std::size_t Align>
class InPlaceAny {
    alignas(Align) unsigned char storage_[Size];
    void (*destroy_)(void*) = nullptr;
    void (*move_)(void*, void*) = nullptr;
    std::type_index type_ = typeid(void);
    bool has_value_ = false;
public:
    template<typename T, typename... Args>
    void emplace(Args&&... args) {
        static_assert(sizeof(T) <= Size && alignof(T) <= Align,
                      "T does not fit InPlaceAny's fixed capacity");
        reset();
        new (storage_) T(std::forward<Args>(args)...);
        destroy_ = [](void* p) { std::launder(reinterpret_cast<T*>(p))->~T(); };
        type_ = typeid(T);
        has_value_ = true;
    }
    template<typename T> T& get() {
        if (!has_value_ || type_ != typeid(T)) throw std::bad_cast();
        return *std::launder(reinterpret_cast<T*>(storage_));
    }
    void reset() { if (has_value_) { destroy_(storage_); has_value_ = false; } }
    ~InPlaceAny() { reset(); }
};
```
**Demonstration:** `InPlaceAny<32, 8> a; a.emplace<int>(42); std::cout << a.get<int>();` stores and retrieves an `int` cleanly; a small `struct Pair { int a, b; };` similarly fits within 32 bytes. Attempting `a.emplace<std::array<double, 10>>(...)` (80 bytes) fails to compile via the `static_assert`, matching the exercise's stated choice of a compile-time rejection over a thrown exception — chosen here because oversized-type usage is a static property of the call site, knowable entirely at compile time, so failing early avoids ever reaching a runtime path at all.

---

**07-P38.**
```cpp
class Any {
    void* ptr_ = nullptr;
    void (*destroy_)(void*) = nullptr;
    std::type_index type_ = typeid(void);
public:
    template<typename T, typename... Args>
    void emplace(Args&&... args) {
        reset();
        ptr_ = new T(std::forward<Args>(args)...);
        destroy_ = [](void* p) { delete static_cast<T*>(p); };
        type_ = typeid(T);
    }
    template<typename T> T& get() {
        if (type_ != typeid(T)) throw std::bad_cast();
        return *static_cast<T*>(ptr_);
    }
    void reset() { if (ptr_) { destroy_(ptr_); ptr_ = nullptr; } }
    ~Any() { reset(); }
};
```
**Comparison with 07-P39's `InPlaceAny`:** for a small type (`sizeof(T) <= 16`-ish), `InPlaceAny` avoids a heap allocation entirely — the object lives inline in `storage_` — while this heap-based `Any` always allocates on `emplace`, regardless of how small `T` is. For a large type that wouldn't fit `InPlaceAny`'s fixed `Size`, only this heap-based `Any` still works at all — `InPlaceAny`'s `static_assert` would simply refuse to compile for an oversized `T`, whereas `Any`'s heap allocation scales to any size.

---

**07-P39.**
```cpp
struct Matrix3x3 { float m[9]; };

void add_restrict(float* __restrict dst, const float* __restrict a, const float* __restrict b) {
    for (int i = 0; i < 9; ++i) dst[i] = a[i] + b[i];
}

void add_plain(float* dst, const float* a, const float* b) {
    for (int i = 0; i < 9; ++i) dst[i] = a[i] + b[i];
}
```
Both functions compute the identical result for genuinely non-overlapping inputs. The compiler is permitted to vectorize/reorder `add_restrict` more aggressively because `__restrict` is an explicit promise from the programmer that `dst`, `a`, and `b` do not alias one another — under that promise, the compiler can safely load `a[i]`/`b[i]` and store `dst[i]` in any convenient order (or batch several iterations via SIMD) without worrying that a store to `dst[i]` could retroactively change a value it already read from `a[i+1]` in a later loaded batch. `add_plain`, lacking that promise, must conservatively assume `dst` could overlap `a` or `b`, forcing it to preserve strict per-iteration load/store ordering to remain correct in the (here hypothetical, but not ruled out by the signature) case where the pointers do alias — this is the direct performance cost of *not* being able to assume the strict-aliasing/no-overlap guarantee the `restrict` hint supplies.

---

**07-P40.**
```cpp
template<typename T>
class ScopedBytesAsObject {
    static_assert(std::is_trivially_copyable_v<T>);
    T* obj_;
public:
    explicit ScopedBytesAsObject(std::span<std::byte> bytes) {
        // C++23: obj_ = std::start_lifetime_as<T>(bytes.data());
        // Pre-C++23 emulation (explicitly an emulation, not equivalent in all edge cases):
        std::memmove(bytes.data(), bytes.data(), sizeof(T));  // no-op copy to make intent explicit
        obj_ = std::launder(reinterpret_cast<T*>(bytes.data()));
    }
    T& get() { return *obj_; }
};
```
**What `start_lifetime_as` solves:** the bytes "already looking like a valid `T`" bit-for-bit is a statement purely about *content*, but `[basic.life]`'s object-lifetime rules are about *identity/existence*, not content — raw bytes read from a file or socket were never actually constructed as a `T` at all, so accessing them as a `T` via a plain `reinterpret_cast<T*>` is accessing an object that, from the language's point of view, does not exist yet, independent of whether its bytes happen to already encode a valid value. `start_lifetime_as<T>` is the standard's explicit, defined mechanism for saying "begin a `T`'s lifetime here, using these existing bytes as its representation" without running an actual constructor — filling exactly the gap that plain `reinterpret_cast` cannot legitimately fill.

### Level 6 — Production

**07-P41.** **Proposed change:** split the current array-of-structures (`AoS`) layout into structure-of-arrays (`SoA`) — separate parallel arrays for `x,y,z`, `vx,vy,vz`, and `active`, or at minimum separate the frequently-together-accessed position/velocity floats from the rarely-co-accessed `active` flag. **Why it helps:** the update loop reads/writes every field of every *active* particle every frame — in the current `AoS` layout, each `Particle`'s 7 fields (6 floats + a bool, padded) are interleaved in memory, so even though the loop needs all of them, nothing is *wasted* per se for a fully-active set, but the `bool active` interleaved among the floats breaks natural 4-float (16-byte) SIMD alignment/packing and wastes bandwidth reading padding bytes; more importantly, if any code path only checks `active` without touching the floats, `AoS` forces loading entire cache lines' worth of float data just to read one bool. `SoA` lets the floats pack contiguously for genuinely vectorizable, bandwidth-efficient access, and lets an `active`-only scan touch a much smaller, separate region of memory. **Downside/migration cost:** every piece of code that currently treats a `Particle` as one cohesive object (constructing one, passing it by value, storing it in an unrelated container expecting a single struct) needs to be rewritten to work against parallel-array indices instead of object references — a broad, invasive refactor across the codebase, not a localized change, and it complicates any code that fundamentally needs "one particle's complete state" as a unit (e.g., serialization, spawning a single new particle) since that state is no longer contiguous.

---

**07-P42.** **What's guaranteed safe:** clients that only ever hold a `Handle*`/`Handle&` obtained from the library's own factory functions are unaffected by the layout change — they never see or depend on `Handle`'s `sizeof` or internal member offsets, since they only ever get pointers/references the library itself allocated and manages; the library's own compiled code (which knows the new layout) is the only code that ever actually touches `Handle`'s bytes. **What's silently broken:** clients that embed `Handle` *by value* — as a member of their own struct, or on the stack — compiled against the *old* `Handle` layout (`sizeof(Handle) == sizeof(void*)`) will have baked that old size into their own struct's layout, offsets of subsequent members, and stack frame sizes; if the library ships a new binary with the larger `Handle` (containing the added `bool valid`) without those client binaries being recompiled, the client's own compiled assumptions about sizes/offsets involving `Handle` no longer match what the new library's code expects when handling a `Handle` passed by value across the boundary — a classic by-value-across-a-binary-boundary layout mismatch. The by-pointer/by-reference-only client pattern is exactly what's safe; the by-value-embedding pattern is exactly what this change silently breaks for anyone who doesn't recompile.

### Level 7 — Principal Reasoning

**07-P43.** **Correctness risks the hand-rolled approach carries that `std::variant` eliminates by construction:** (1) a missed or incorrect destructor-dispatch branch in the tagged union's hand-written destructor/copy/move logic (exactly the class of bug rehearsed in 07-P29) — `std::variant` guarantees the active alternative's special member functions are always invoked correctly, by construction, with no hand-written dispatch code to get wrong; (2) a `std::launder`-omission or placement-new-over-still-live-object bug (07-P23/07-P35's territory) — `variant`'s implementation has already solved this once, correctly, so every consumer inherits that correctness rather than re-risking it per hand-rolled type. **Concrete cost `std::variant` would reintroduce:** its implementation (in most standard library vendors' versions) carries overhead the hand-rolled version was specifically built to avoid — commonly a larger footprint than a minimal hand-tuned union (extra bookkeeping for exception-safety during in-place type changes, e.g. via an internal "valueless by exception" fallback path) and, in some implementations, extra branching/indirection in `visit`/`get` compared to a hand-tuned, closed-set-of-two-types dispatch. **Decision process:** don't decide from principle alone in either direction — actually benchmark `std::variant<T, Error>` against the current hand-rolled type in the specific hot path under realistic load, and separately audit the current hand-rolled type's git history/bug tracker for *actual* past incidents of the correctness-risk categories named above. If the measured overhead is negligible relative to the hot path's total cost (i.e., switching wouldn't actually regress the metric the hand-rolled design was built to protect), correctness wins by default, since the original performance rationale turns out not to be load-bearing. If the hand-rolled type has, in practice, never had a launder/dispatch bug in its multi-year history and the measured `variant` overhead *is* significant relative to the hot path's budget, the risk `std::variant` would eliminate is currently theoretical rather than demonstrated, and keeping (or perhaps re-deriving, more carefully, with the specific bug classes named here as an explicit checklist) the hand-rolled version is defensible — the evidence that should change the recommendation is exactly "does this path's profiling budget tolerate the measured `variant` overhead," not an abstract preference for either safety or performance.

**07-P44.** **Layout:** `PackedFlags` packs `flag_a`(1) + `flag_b`(1) + `flag_c`(3) + `reserved`(3) = 8 bits into a single byte on most mainstream compilers when the underlying type is `unsigned` and the total fits within one allocation unit, but the standard leaves bit *order* (does `flag_a` occupy the low bit or the high bit of the byte?), whether adjacent bitfields of different declared widths get packed into the same storage unit at all, and the exact storage-unit size chosen entirely implementation-defined — MSVC, GCC, and Clang do not all agree, and even a single compiler's behavior can change across ABI-versioned releases. `reserved` cannot have its address taken (`&w.reserved` is ill-formed) precisely because it doesn't necessarily start on a byte boundary — there is no guarantee an addressable `unsigned char*`/pointer could validly point at just those bits. **`sizeof(Widget)` comparison:** with `Policy policy_;` as an ordinary member, `sizeof(Widget)` is typically larger than the sum of its "real" members alone, because even an empty class must have a nonzero size (at least 1 byte, per `[intro.object]`, so that two distinct objects have distinct addresses) — and that byte is not free, since padding around it can also grow the enclosing struct's alignment requirements. With `[[no_unique_address]] policy_;`, the compiler is permitted (though C++20 phrases this as a permission, not a mandate) to overlay `policy_`'s storage with other members or with the struct's padding/tail, so `sizeof(Widget)` after annotating typically drops back down to match a `Widget` that never had a `Policy` member at all — the empty tag contributes zero observable bytes.

**07-P45.** **Fix:** annotate the member as `[[no_unique_address]] Deleter deleter_;` (C++20) so the compiler is permitted to overlay `deleter_`'s storage with `UniqueHandle`'s other members (typically just the raw pointer) whenever `Deleter` is empty and stateless, bringing `sizeof(UniqueHandle<T, DefaultDeleter>)` back down to `sizeof(T*)`. On a pre-C++20 toolchain, the equivalent effect is achieved the older way — via Empty Base Optimization: have `UniqueHandle` privately inherit from `Deleter` instead of holding it as a data member (`class UniqueHandle : private Deleter`), since EBO (permitted, not mandated, by the standard, but universally implemented in practice for empty non-virtual bases) lets an empty base class occupy zero bytes within the derived object, in a way ordinary data members of empty type historically could not. **Why a stateful `Deleter` doesn't benefit:** both `[[no_unique_address]]` and EBO only ever eliminate the padding-for-distinct-addresses overhead of an *empty* type — they never compress or elide actual state. A `Deleter` holding a `FILE*` or a `std::function`-based callback has real data that must be stored somewhere and occupies exactly the bytes its own members require (plus ordinary padding), regardless of which of the two mechanisms is used; neither technique is a free-size-reduction trick for arbitrary types, only a zero-cost guarantee specifically for the empty-type case.

## Integration Challenge Solution — 07-IC1

```cpp
struct Drawable {
    virtual void draw() const = 0;
    virtual ~Drawable() = default;
};

template<std::size_t Size, std::size_t Align>
class PolyBox {
    alignas(Align) unsigned char storage_[Size];
    Drawable* held_ = nullptr;   // obtained via launder at construction time, retained (not re-derived per access)

public:
    template<typename T, typename... Args>
    void emplace(Args&&... args) {
        static_assert(sizeof(T) <= Size, "T does not fit PolyBox's fixed Size");
        static_assert(alignof(T) <= Align, "T's alignment exceeds PolyBox's fixed Align");
        static_assert(std::is_base_of_v<Drawable, T>, "T must derive from Drawable");
        reset();
        T* obj = new (storage_) T(std::forward<Args>(args)...);
        held_ = std::launder(static_cast<Drawable*>(obj));
    }

    void draw() const {
        if (held_) held_->draw();
    }

    void reset() {
        if (held_) {
            held_->~Drawable();   // virtual dispatch to the actual derived destructor
            held_ = nullptr;
        }
    }

    ~PolyBox() { reset(); }
};
```

**3(a) — why `std::launder`, not a fresh `reinterpret_cast` each call.** After `new (storage_) T(...)`, `storage_` (raw `unsigned char[]`) now has a `T` object living in it, but the compiler is otherwise entitled to assume a `reinterpret_cast<Drawable*>(storage_)` still refers to "an array of `unsigned char`," not the polymorphic `T` object now there — exactly the gap `std::launder` exists to close (07-QC5). Capturing the laundered pointer *once*, at construction time, and retaining it in `held_` (rather than re-deriving it via a fresh cast on every `draw()` call) additionally avoids repeatedly re-asserting a pointer-provenance fact the compiler might otherwise have opportunities to second-guess across separate expressions — retaining one already-laundered pointer keeps the "this really does point at a live `T`" fact anchored to a single, correctly-established pointer value.

**3(b) — why `alignas(Align)` on the raw array.** A plain `unsigned char storage_[Size]` only guarantees 1-byte alignment on its own; placement-constructing a `T` that requires stricter alignment (e.g., an 8-byte-aligned type) into insufficiently-aligned storage is undefined behavior at the moment of construction, independent of whether it happens to "work" on a given run — exactly 07-P30's `aligned_alloc`-vs-`malloc` lesson, applied to a stack/member array instead of a heap allocation. `alignas(Align)` forces the compiler to actually place `storage_` itself at an address satisfying the requested alignment, which is a precondition for any `T` with `alignof(T) <= Align` to be legitimately constructible there at all.

**3(c) — why placement-construction is required, not treating `storage_` as already holding a valid object.** Raw storage that has never had a constructor run over it does not contain a live object of any type, regardless of what bits happen to be sitting in it (07-P40's `start_lifetime_as` discussion makes the same point for trivially-copyable types read from bytes) — for a polymorphic, non-trivial `Drawable`-derived `T`, there is no defined mechanism *other than* actually running a constructor (via placement-new) that begins that object's lifetime, establishes its vptr correctly, and makes subsequent virtual dispatch through it well-defined. Treating `storage_` as pre-populated would mean calling `draw()` (or the destructor) on storage that never had a `T`'s vptr correctly installed at all — there is no vtable to dispatch through, and the access is undefined behavior from the first use.

**4 — demonstration sketch.**
```cpp
struct Circle : Drawable { double r; void draw() const override { std::cout << "circle r=" << r << "\n"; } };
struct Square : Drawable { double s; void draw() const override { std::cout << "square s=" << s << "\n"; } };

PolyBox<32, 8> box;
box.emplace<Circle>(2.0);
box.draw();          // "circle r=2"
box.reset();
box.emplace<Square>(3.0);
box.draw();          // "square s=3"

struct TooBig : Drawable { double data[10]; void draw() const override {} };
// box.emplace<TooBig>();   // fails to compile: static_assert(sizeof(T) <= Size) trips,
                             // rather than silently writing past storage_'s 32 bytes.
```
