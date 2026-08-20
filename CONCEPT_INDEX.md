# CONCEPT_INDEX.md — Random-Access Concept Lookup

> Derived from [`CURRICULUM.md`](CURRICULUM.md). Use this file to study by *concept*, not by chapter order. Every concept from `PROMPT.md:254-494` appears exactly once, grouped under its PROMPT.md coverage group, with a link to its owning chapter.

## How to Use This Index

1. Weak on something specific? Find the concept row, jump straight to its `Related Problems`, skip the surrounding chapter prose.
2. Don't know what's wrong, just that something is? Use the **Reverse Index: Symptom → Concept** at the bottom.
3. `Status` here should mirror `PROGRESS.md`'s Concept Mastery table — update both together.
4. `Review Checkpoint` is a promise, not a suggestion: those chapters *will* contain a problem that re-tests this concept.

## Legend

**Difficulty** (1–7): the level at which the concept is first genuinely exercised, not merely mentioned. **Status:** ☐ not started · ◐ in progress / assisted-only · ☑ complete (per `PROGRESS.md`'s Definitions of Done — solved unaided, not merely read). **Notes:** flags standard-guarantee vs. implementation-behavior vs. ABI-behavior vs. observed-behavior distinctions (`PROMPT.md:752-756`), and MSVC/GCC/Clang divergence.

---

## Group: Language & Core Semantics (owning chapter: 01)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Initialization (default/copy/direct/list/aggregate) | [01](01-core-semantics/CONCEPTS.md#initialization) | — | 1 | 01-P01..P06 | P-1.1 | — | ☐ | narrowing-conversion rules are standard guarantees; brace-elision for aggregates is a frequent MSVC/GCC diagnostic difference |
| `const` | [01](01-core-semantics/CONCEPTS.md#const) | Initialization | 1 | 01-P07, 01-P08 | P-1.1 | — | ☐ | top-level vs low-level const; const propagation through pointers/references |
| `constexpr` | [01](01-core-semantics/CONCEPTS.md#constexpr) | `const` | 2 | 01-P09..P11 | P-1.1 | — | ☐ | compile-time evaluability is a standard guarantee; UB in a constexpr context is diagnosed at compile time only if actually evaluated |
| `consteval` | [01](01-core-semantics/CONCEPTS.md#consteval) | `constexpr` | 3 | 01-P12 | — | — | ☐ | C++20; immediate function, must be evaluable at compile time in every call |
| `constinit` | [01](01-core-semantics/CONCEPTS.md#constinit) | `constexpr`, storage duration | 3 | 01-P13 | — | — | ☐ | solves static-init-order fiasco for constants; does not imply const |
| UB / unspecified / implementation-defined | [01](01-core-semantics/CONCEPTS.md#undefined-unspecified-and-implementation-defined-behavior) | — | 2 | 01-P48, P49 | — | 07, 11 (xref: aliasing, data races) | ☐ | the three-way taxonomy behind the "UB" annotations used throughout this workbook; a compiler may assume UB never occurs and optimize accordingly |
| Scope | [01](01-core-semantics/CONCEPTS.md#scope) | — | 1 | 01-P14, 01-P15 | — | — | ☐ | block/namespace/class scope; name hiding rules |
| Storage duration | [01](01-core-semantics/CONCEPTS.md#storage-duration) | Scope | 2 | 01-P16..P18 | — | 02 (lifetime split) | ☐ | automatic/static/thread/dynamic — deliberately taught separately from lifetime (Ch02) |
| References | [01](01-core-semantics/CONCEPTS.md#references) | — | 1 | 01-P19, 01-P20 | — | 03 (value categories) | ☐ | reference binding rules; reference to temporary extends lifetime only in specific cases |
| Pointers | [01](01-core-semantics/CONCEPTS.md#pointers) | — | 1 | 01-P21, 01-P22 | — | 07 (object model) | ☐ | pointer arithmetic UB boundaries |
| `auto` | [01](01-core-semantics/CONCEPTS.md#auto) | — | 2 | 01-P23, 01-P24 | — | — | ☐ | deduction drops references/cv-qualifiers by default; `auto&&` differs |
| `decltype` | [01](01-core-semantics/CONCEPTS.md#decltype) | `auto` | 2 | 01-P25, 01-P26 | — | — | ☐ | decltype(expr) vs decltype((expr)) — parenthesization changes the result |
| `decltype(auto)` | [01](01-core-semantics/CONCEPTS.md#decltype-auto) | `decltype` | 3 | 01-P27 | — | — | ☐ | preserves reference-ness, unlike plain `auto` |
| Overload resolution | [01](01-core-semantics/CONCEPTS.md#overload-resolution) | Implicit conversions | 3 | 01-P28..P32 | P-1.1 | — | ☐ | standard-mandated ranking of conversion sequences; ambiguity is a compile error, not UB |
| Implicit conversions | [01](01-core-semantics/CONCEPTS.md#implicit-conversions) | — | 2 | 01-P33, P34 | P-1.1 | — | ☐ | standard vs user-defined conversion sequences; at most one user-defined conversion per sequence |
| Explicit conversions | [01](01-core-semantics/CONCEPTS.md#explicit-conversions) | Implicit conversions | 2 | 01-P35 | P-1.1 | — | ☐ | `explicit` on constructors and conversion operators; `explicit(bool)` (C++20) |
| Operator overloading | [01](01-core-semantics/CONCEPTS.md#operator-overloading) | Overload resolution | 3 | 01-P36..P39 | P-1.1 | — | ☐ | member vs free-function overloads |
| Three-way comparison `<=>` | [01](01-core-semantics/CONCEPTS.md#the-three-way-comparison-operator-and-c20) | Operator overloading | 3 | 01-P46, P47 | — | 05 (concepts, `std::totally_ordered`) | ☐ | C++20; defaulted `<=>` deduces the narrowest correct comparison category; does not itself declare `==` |
| Lambdas | [01](01-core-semantics/CONCEPTS.md#lambdas) | Function objects | 3 | 01-P40..P43 | — | 05 (generic) | ☐ | capture-by-value vs reference; `mutable`; generic lambdas are templates in disguise |
| Function objects | [01](01-core-semantics/CONCEPTS.md#function-objects) | Operator overloading | 2 | 01-P44 | — | 05 | ☐ | `operator()` overload set participates in ordinary overload resolution |
| `std::function` | [01](01-core-semantics/CONCEPTS.md#stdfunction) | Function objects | 3 | 01-P45 | P-2.3 (contrast) | 05, 13 | ☐ | type-erased; always allocates unless SBO-eligible — implementation-defined threshold |
| `std::invoke` | [01](01-core-semantics/CONCEPTS.md#stdinvoke) | Function objects | 3 | (bundled with 01-P44/P45) | P-2.3 | 05 | ☐ | uniform call syntax for callables incl. pointers-to-member |

## Group: Object Lifetime & Resource Management (owning chapter: 02)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Constructors | [02](02-lifetime-raii/CONCEPTS.md#constructors) | Initialization | 1 | 02-P01..P04 | P-1.2 | — | ☐ | delegating constructors; member init list order follows declaration order, not list order |
| Destructors | [02](02-lifetime-raii/CONCEPTS.md#destructors) | Constructors | 1 | 02-P05, P06 | P-1.2 | — | ☐ | throwing from a destructor during unwinding calls `std::terminate` |
| Initialization order | [02](02-lifetime-raii/CONCEPTS.md#initialization-order) | Constructors, Storage duration | 2 | 02-P07..P09 | — | — | ☐ | base classes before members; static init order across TUs is unspecified unless `constinit`/function-local statics |
| RAII | [02](02-lifetime-raii/CONCEPTS.md#raii) | Destructors | 2 | 02-P10..P14 | P-1.2 | 06, 09, 11, 12 | ☐ | idiom, not a language feature; the entire exception-safety story rests on it |
| Rule of 0/3/5 | [02](02-lifetime-raii/CONCEPTS.md#rule-of-035) | Destructors, RAII | 3 | 02-P15..P18 | P-1.2 | — | ☐ | rule of 0 is the goal; rule of 5 exists because the compiler's implicit rules are unsafe once you write any one of the five |
| Ownership | [02](02-lifetime-raii/CONCEPTS.md#ownership) | RAII | 3 | 02-P19..P22 | P-1.2 | 05, 08, 11, 13 | ☐ | unique vs shared vs non-owning is a design decision encoded in the type |
| `unique_ptr` | [02](02-lifetime-raii/CONCEPTS.md#unique_ptr) | Ownership | 2 | 02-P23..P26 | P-1.2 | — | ☐ | move-only; zero overhead over a raw pointer with the default deleter (standard does not strictly guarantee this, but it is universal practice) |
| `shared_ptr` | [02](02-lifetime-raii/CONCEPTS.md#shared_ptr) | Ownership | 3 | 02-P27..P30 | — | 07, 11, 12 | ☐ | control block is a separate allocation unless `make_shared`; refcount ops are atomic — a real, measurable cost |
| `weak_ptr` | [02](02-lifetime-raii/CONCEPTS.md#weak_ptr) | `shared_ptr` | 3 | 02-P31, P32 | — | — | ☐ | breaks ownership cycles; `lock()` is the only safe access path |
| Custom deleters | [02](02-lifetime-raii/CONCEPTS.md#custom-deleters) | `unique_ptr` | 3 | 02-P33, P34 | P-1.2 | — | ☐ | changes `unique_ptr`'s size (stateful deleter) — an object-representation consequence, xref Ch07 |
| Custom resources | [02](02-lifetime-raii/CONCEPTS.md#custom-resources) | RAII, Custom deleters | 4 | 02-P35..P37 | P-1.2 | 09 | ☐ | wrapping fd/HANDLE — the direct precursor to Ch09 |
| Exception safety | [02](02-lifetime-raii/CONCEPTS.md#exception-safety) | RAII | 4 | 02-P38..P41 | P-1.4 | 06 (formalized) | ☐ | introduced here as "does a throw during construction leak or double-free"; formal guarantee levels are Ch06's job |
| Object lifetime | [02](02-lifetime-raii/CONCEPTS.md#object-lifetime) | Destructors, Storage duration | 4 | 02-P42, P43 | — | 07 ([basic.life] formal rules) | ☐ | lifetime begins after construction completes, ends when destruction begins — subtly not the same as storage duration |

## Group: Value Categories (owning chapter: 03)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| lvalue / xvalue / prvalue / glvalue / rvalue | [03](03-value-categories/CONCEPTS.md#value-category-taxonomy) | References | 2 | 03-P01..P08 | — | — | ☐ | standard-defined taxonomy; commonly conflated with "is it movable," which is a separate question |
| Temporary materialization | [03](03-value-categories/CONCEPTS.md#temporary-materialization) | prvalue | 3 | 03-P09, P10 | — | — | ☐ | C++17 rule: a prvalue only becomes a temporary object at the point it is needed to |
| Move semantics | [03](03-value-categories/CONCEPTS.md#move-semantics) | rvalue, Destructors | 3 | 03-P11..P16 | P-1.3 | 04, 05, 06, 12 | ☐ | moving leaves the source in a valid-but-unspecified state, not a guaranteed-empty one, unless the type documents otherwise |
| Copy elision | [03](03-value-categories/CONCEPTS.md#copy-elision) | prvalue | 3 | 03-P17..P19 | — | 08, 12 | ☐ | mandatory in some C++17 contexts (return of a prvalue); NRVO remains optional even in C++20/23 |
| NRVO | [03](03-value-categories/CONCEPTS.md#nrvo) | Copy elision | 4 | 03-P20, P21 | P-1.4 | 08 | ☐ | named-object elision is a compiler *optimization*, standard permits but never mandates it — the single most misunderstood item in this chapter |
| Forwarding references | [03](03-value-categories/CONCEPTS.md#forwarding-references) | `auto`, Templates (forward-ref) | 4 | 03-P22..P26 | P-2.3 | — | ☐ | `T&&` in a deduced context only, not the same as an rvalue-reference parameter on a non-template function |
| Reference collapsing | [03](03-value-categories/CONCEPTS.md#reference-collapsing) | Forwarding references | 4 | 03-P27, P28 | — | — | ☐ | `& &`→`&`, `& &&`→`&`, `&& &`→`&`, `&& &&`→`&&` — the only four rules, standard-mandated |
| `std::move` | [03](03-value-categories/CONCEPTS.md#stdmove) | Move semantics | 2 | 03-P29, P30 | P-1.3 | — | ☐ | pure cast to rvalue reference, performs no action itself — a naming trap for beginners |
| `std::forward` | [03](03-value-categories/CONCEPTS.md#stdforward) | Reference collapsing | 4 | 03-P31..P34 | P-2.3 | — | ☐ | conditionally casts based on the deduced type's reference-ness |

## Group: STL (owning chapter: 04)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Containers (overview) | [04](04-stl/CONCEPTS.md#containers) | — | 1 | 04-P01, P02 | P-2.1 | — | ☐ | sequence vs associative vs unordered vs container adapters |
| Iterator categories | [04](04-stl/CONCEPTS.md#iterator-categories) | Pointers | 2 | 04-P03..P06 | — | — | ☐ | input/output/forward/bidirectional/random-access/contiguous (C++20) form a strict hierarchy of guarantees |
| Iterator invalidation | [04](04-stl/CONCEPTS.md#iterator-invalidation) | Iterator categories | 3 | 04-P07..P13 | P-2.1 | 07, 11, 12 | ☐ | per-container rules differ significantly; `vector` reallocation invalidates everything, `list`/`map` erase invalidates only the erased element's iterator |
| Algorithms | [04](04-stl/CONCEPTS.md#algorithms) | Iterator categories | 3 | 04-P14..P18 | P-2.1 | — | ☐ | complexity guarantees are standard-mandated; some (`std::sort`) have no guaranteed algorithm, only a complexity bound |
| `vector` | [04](04-stl/CONCEPTS.md#vector) | Containers | 2 | 04-P19..P22 | P-1.3, P-2.1 | — | ☐ | growth factor is implementation-defined (commonly 1.5x MSVC, 2x libstdc++) |
| `deque` | [04](04-stl/CONCEPTS.md#deque) | Containers | 2 | 04-P23 | — | — | ☐ | not contiguous; stable references under push_front/back |
| `list` | [04](04-stl/CONCEPTS.md#list) | Containers | 2 | 04-P24 | — | — | ☐ | O(1) splice; iterator stability under insert/erase (except the erased element) |
| Associative containers | [04](04-stl/CONCEPTS.md#associative-containers) | Containers | 2 | 04-P25..P28 | P-2.1 | — | ☐ | `map`/`set` ordering requires `Compare`; node-based, pointer/iterator stability under insert |
| Unordered containers | [04](04-stl/CONCEPTS.md#unordered-containers) | Associative containers | 3 | 04-P29..P32 | P-2.1 | — | ☐ | bucket-based; rehashing invalidates iterators but not references/pointers to elements (standard guarantee) |
| `string` | [04](04-stl/CONCEPTS.md#string) | `vector` | 2 | 04-P33..P35 | P-2.1 | — | ☐ | SSO is universal in practice but not standard-mandated; `data()` is null-terminated since C++11 |
| `string_view` lifetime traps | [04](04-stl/CONCEPTS.md#string_view) | `string`, References | 4 | 04-P36..P39 | P-2.1 | — | ☐ | non-owning — the single most common source of dangling in modern C++ code review |
| `std::span` | [04](04-stl/CONCEPTS.md#stdspan-borrowing-without-owning-generalized-to-contiguous-ranges) | `string_view` lifetime traps, Iterator invalidation | 4 | 04-P49, P51 | — | — | ☐ | C++20; non-owning view generalized to any contiguous `T`, not just `char`; dangles under the same conditions as `string_view` |
| `std::format` | [04](04-stl/CONCEPTS.md#stdformat-type-safe-compile-time-checked-text-formatting) | — | 3 | 04-P50, P52 | — | — | ☐ | C++20; format-string/argument agreement checked at compile time, unlike `printf`; extensible via `std::formatter<T>` specialization |
| `optional` | [04](04-stl/CONCEPTS.md#optional) | — | 2 | 04-P40, P41 | P-2.2 | 06, 07, 13 | ☐ | distinct from a null pointer: always holds storage for `T`, contextually converts to bool |
| `variant` | [04](04-stl/CONCEPTS.md#variant) | — | 3 | 04-P42, P43 | P-2.2 | 06, 07, 13 | ☐ | never empty except the rare "valueless by exception" state — a real, testable edge case |
| `any` | [04](04-stl/CONCEPTS.md#any) | `variant` | 3 | 04-P44 | P-2.2 | — | ☐ | type-erased, requires `any_cast` with exact type match (no implicit conversion) |
| Ranges (introduction) | [04](04-stl/CONCEPTS.md#ranges-introduction) | Iterator categories, Algorithms | 3 | 04-P45..P48 | — | 13 (deep) | ☐ | C++20; lazy evaluation of views is the key departure from classic algorithms |

## Group: Generic Programming (owning chapter: 05)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Function templates | [05](05-generic-programming/CONCEPTS.md#function-templates) | `auto` | 2 | 05-P01..P04 | — | — | ☐ | template argument deduction rules differ subtly from `auto` deduction |
| Class templates | [05](05-generic-programming/CONCEPTS.md#class-templates) | Function templates | 2 | 05-P05..P07 | P-2.5 | — | ☐ | CTAD (C++17) and user-provided deduction guides |
| Specialization | [05](05-generic-programming/CONCEPTS.md#specialization) | Class templates | 3 | 05-P08, P09 | — | — | ☐ | full specialization only for classes and function templates at namespace scope |
| Partial specialization | [05](05-generic-programming/CONCEPTS.md#partial-specialization) | Specialization | 4 | 05-P10..P13 | — | — | ☐ | class templates only — functions cannot be partially specialized, only overloaded |
| Variadic templates | [05](05-generic-programming/CONCEPTS.md#variadic-templates) | Function templates | 3 | 05-P14..P17 | P-2.5 | — | ☐ | parameter packs must be expanded, not indexed, pre-C++26 |
| Parameter packs | [05](05-generic-programming/CONCEPTS.md#parameter-packs) | Variadic templates | 3 | 05-P18, P19 | P-2.5 | — | ☐ | `sizeof...`, pack expansion contexts |
| Fold expressions | [05](05-generic-programming/CONCEPTS.md#fold-expressions) | Parameter packs | 4 | 05-P20..P22 | P-2.5 | — | ☐ | C++17; unary vs binary fold, left vs right associativity |
| Type traits | [05](05-generic-programming/CONCEPTS.md#type-traits) | Class templates | 3 | 05-P23..P26 | — | — | ☐ | `<type_traits>` is a standard-mandated compile-time introspection library |
| SFINAE | [05](05-generic-programming/CONCEPTS.md#sfinae) | Type traits, Partial specialization | 5 | 05-P27..P30 | — | — | ☐ | substitution failure in the *immediate context* only — a frequent source of confusing errors when misapplied |
| Two-phase lookup & dependent names | [05](05-generic-programming/CONCEPTS.md#two-phase-name-lookup-and-dependent-names) | Function templates | 5 | 05-P51, P52, P55 | — | — | ☐ | `typename`/`template` disambiguators; a non-dependent name error is a hard error at definition, no rescue at instantiation |
| Template template parameters & tag dispatch | [05](05-generic-programming/CONCEPTS.md#template-template-parameters-and-tag-dispatch) | Class templates, Type traits | 5 | 05-QC6 | — | 04 | ☐ | pre-`if constexpr` idiom; still used internally by the standard library's iterator-category dispatch |
| ADL & hidden friends | [05](05-generic-programming/CONCEPTS.md#argument-dependent-lookup-adl-and-hidden-friends) | Two-phase lookup & dependent names | 5 | 05-P51, P56 | — | — | ☐ | hidden friends are only findable via ADL, never via qualified lookup — a deliberate visibility restriction |
| CTAD & deduction guides | [05](05-generic-programming/CONCEPTS.md#class-template-argument-deduction-ctad-and-deduction-guides) | Class templates | 4 | 05-P53, P54 | — | — | ☐ | deduction guides correct constructor-based deduction when it's technically consistent but semantically wrong |
| Concepts | [05](05-generic-programming/CONCEPTS.md#concepts) | SFINAE (contrast) | 4 | 05-P31..P36 | P-2.3 | 06, 13 | ☐ | C++20; named, composable, produce far better diagnostics than SFINAE |
| Constraints | [05](05-generic-programming/CONCEPTS.md#constraints) | Concepts | 4 | 05-P37, P38 | P-2.3 | — | ☐ | `requires` clauses/expressions; subsumption rules affect overload resolution |
| Compile-time programming | [05](05-generic-programming/CONCEPTS.md#compile-time-programming) | `constexpr`, Templates | 5 | 05-P39..P41 | P-2.5 | — | ☐ | `constexpr` functions, `if constexpr`, template metaprogramming as two eras of the same goal |
| CRTP | [05](05-generic-programming/CONCEPTS.md#crtp) | Class templates, Inheritance (xref 07) | 5 | 05-P42..P44 | — | 07, 13 | ☐ | static polymorphism; the base class is instantiated per-derived-type, not shared |
| Policy-based design | [05](05-generic-programming/CONCEPTS.md#policy-based-design) | Class templates, CRTP | 6 | 05-P45, P46 | — | 13 | ☐ | composing behavior via template parameters rather than runtime configuration |
| Type erasure | [05](05-generic-programming/CONCEPTS.md#type-erasure) | Class templates, Virtual functions (xref 07) | 6 | 05-P47..P50 | P-2.3 | 07, 11, 13 | ☐ | `std::function`/`std::any`/custom vtables all implement the same idiom differently |

## Group: Error Handling (owning chapter: 06)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Exceptions | [06](06-error-handling/CONCEPTS.md#exceptions) | RAII | 2 | 06-P01..P04 | P-2.4 | — | ☐ | thrown by value, caught by reference (const ref for read-only) is the idiomatic pattern |
| Stack unwinding | [06](06-error-handling/CONCEPTS.md#stack-unwinding) | Exceptions, Destructors | 3 | 06-P05, P06 | — | — | ☐ | destructors run in reverse construction order during unwinding; a second exception during unwinding calls `std::terminate` |
| `noexcept` | [06](06-error-handling/CONCEPTS.md#noexcept) | Exceptions | 3 | 06-P07..P11 | — | 03 (stub), 11, 12 | ☐ | affects overload resolution for move (esp. in `vector` reallocation) and is checked, not just documentation — violating it calls `std::terminate` |
| Exception guarantees | [06](06-error-handling/CONCEPTS.md#exception-guarantees) | `noexcept`, Exception safety (xref 02) | 4 | 06-P12..P18 | P-2.4 | — | ☐ | no-throw / strong / basic / no guarantee — a standard taxonomy, not an implementation detail |
| Error codes | [06](06-error-handling/CONCEPTS.md#error-codes) | — | 2 | 06-P19, P20 | P-2.4 | 09 (errno/GetLastError) | ☐ | `std::error_code`/`error_category` are extensible, type-safe replacements for raw `errno` |
| `optional` as error channel | [06](06-error-handling/CONCEPTS.md#optional-as-error-channel) | `optional` (xref 04) | 3 | 06-P21, P22 | P-2.4 | — | ☐ | loses the *reason* for failure — only appropriate when "absent" is the whole story |
| `variant` as error channel | [06](06-error-handling/CONCEPTS.md#variant-as-error-channel) | `variant` (xref 04) | 3 | 06-P23 | P-2.4 | — | ☐ | manual precursor to `expected` |
| `std::expected` | [06](06-error-handling/CONCEPTS.md#stdexpected) | `variant` as error channel | 4 | 06-P24..P28 | P-2.4 | — | ☐ | C++23; monadic `and_then`/`or_else`/`transform` chain error handling without exceptions |
| Error propagation | [06](06-error-handling/CONCEPTS.md#error-propagation) | Exception guarantees, `std::expected` | 5 | 06-P29..P33 | P-2.4, P-3.5 | 09 | ☐ | propagation strategy must be chosen consistently across a module boundary — mixing is the BC-2 scenario |
| API error design | [06](06-error-handling/CONCEPTS.md#api-error-design) | Error propagation | 6 | 06-P34..P38 | P-3.5 | 13 | ☐ | when to throw vs return, and how to document the contract so callers don't have to read the implementation |

## Group: Object Model (owning chapter: 07)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Object representation | [07](07-object-model/CONCEPTS.md#object-representation) | Storage duration (xref 01) | 3 | 07-P01, P02 | — | — | ☐ | the standard specifies behavior, not bit layout, except where explicitly noted (e.g. `[basic.types]`) |
| Alignment | [07](07-object-model/CONCEPTS.md#alignment) | Object representation | 3 | 07-P03..P06 | P-2.2 | 09, 11, 12 | ☐ | `alignof`, `alignas`; over-aligned types have allocation implications (implementation-defined support pre-C++17, standard-required after) |
| Padding | [07](07-object-model/CONCEPTS.md#padding) | Alignment | 3 | 07-P07, P08 | P-2.2 | 12 | ☐ | compiler-inserted, not guaranteed identical across ABIs — a real MSVC/GCC divergence point |
| Bitfields | [07](07-object-model/CONCEPTS.md#bitfields-ebo-and-no_unique_address) | Padding | 3 | 07-P44 | — | — | ☐ | packing, bit order, and storage-unit choice are all implementation-defined; cannot take the address of a bitfield member |
| EBO / `[[no_unique_address]]` | [07](07-object-model/CONCEPTS.md#bitfields-ebo-and-no_unique_address) | Padding, Inheritance | 4 | 07-P44, P45 | — | — | ☐ | permitted, not guaranteed; only ever elides the "distinct-address" byte of an *empty* type, never actual state |
| Triviality | [07](07-object-model/CONCEPTS.md#triviality) | Constructors, Destructors (xref 02) | 4 | 07-P09..P12 | P-2.2 | — | ☐ | trivially copyable enables `memcpy`; standard-defined predicate, checkable via `<type_traits>` |
| Standard layout | [07](07-object-model/CONCEPTS.md#standard-layout) | Triviality, Inheritance | 4 | 07-P13, P14 | — | 08 | ☐ | required for safe `reinterpret_cast` between related types and for C-ABI interop |
| Inheritance | [07](07-object-model/CONCEPTS.md#inheritance) | Class templates (xref 05) | 2 | 07-P15..P18 | — | — | ☐ | public/protected/private inheritance change accessibility, not the layout in general |
| Virtual functions | [07](07-object-model/CONCEPTS.md#virtual-functions) | Inheritance | 3 | 07-P19..P23 | — | 08, 12, 13 | ☐ | dynamic dispatch via vtable is the near-universal implementation, not a standard mandate |
| Vtables/vptrs | [07](07-object-model/CONCEPTS.md#vtables-vptrs) | Virtual functions | 5 | 07-P24..P27 | — | 08 (ABI) | ☐ | implementation detail — layout is compiler/ABI-specific, itanium and MSVC vtable layouts differ |
| RTTI | [07](07-object-model/CONCEPTS.md#rtti) | Virtual functions | 3 | 07-P28, P29 | — | — | ☐ | `dynamic_cast`/`typeid` require at least one virtual function; disableable (`-fno-rtti`/`/GR-`) with real cost tradeoffs |
| Multiple inheritance | [07](07-object-model/CONCEPTS.md#multiple-inheritance) | Inheritance | 5 | 07-P30, P31 | — | — | ☐ | name ambiguity resolution rules; diamond problem sets up virtual inheritance |
| Virtual inheritance | [07](07-object-model/CONCEPTS.md#virtual-inheritance) | Multiple inheritance | 6 | 07-P32, P33 | — | 08 | ☐ | solves the diamond problem at the cost of a more complex (and ABI-fragile) object layout |
| Object lifetime rules | [07](07-object-model/CONCEPTS.md#object-lifetime-rules) | Object lifetime (xref 02), Triviality | 5 | 07-P34..P37 | P-2.2 | 09, 11, 12 | ☐ | `[basic.life]` — placement new, implicit-lifetime types, `std::launder` requirements |
| Aliasing / strict aliasing | [07](07-object-model/CONCEPTS.md#aliasing) | Object representation | 5 | 07-P38..P40 | — | — | ☐ | violating strict aliasing is UB even when "it works" — a canonical "works in Debug, breaks in Release" case |
| Placement construction | [07](07-object-model/CONCEPTS.md#placement-construction) | Object lifetime rules | 4 | 07-P41, P42 | P-2.2 | 09, 11, 12 | ☐ | placement `new` does not allocate; caller owns storage lifetime management |
| Low-level object manipulation | [07](07-object-model/CONCEPTS.md#low-level-object-manipulation) | Aliasing, Placement construction | 7 | 07-P43 | P-2.2 | — | ☐ | `bit_cast` (C++20), `start_lifetime_as` (C++23) as the standard-sanctioned alternative to aliasing violations |

## Group: Compilation & Linking (owning chapter: 08)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Preprocessing | [08](08-compilation-abi/CONCEPTS.md#preprocessing) | — | 1 | 08-P01, P02 | — | — | ☐ | textual substitution before the compiler proper ever sees tokens |
| Translation units | [08](08-compilation-abi/CONCEPTS.md#translation-units) | Preprocessing | 2 | 08-P03 | — | — | ☐ | one .cpp + everything it #includes, post-preprocessing |
| Declarations vs definitions | [08](08-compilation-abi/CONCEPTS.md#declarations-definitions) | Translation units | 2 | 08-P04, P05 | — | — | ☐ | a definition is also a declaration; most-vexing-parse arises from this ambiguity |
| ODR | [08](08-compilation-abi/CONCEPTS.md#odr) | Declarations vs definitions | 4 | 08-P06..P10 | P-3.1 | 10, 13 | ☐ | violations are frequently *not* diagnosed — silent UB across TUs is the standard's own explicit warning |
| `inline` | [08](08-compilation-abi/CONCEPTS.md#inline) | ODR | 3 | 08-P11, P12 | — | — | ☐ | permits multiple identical definitions across TUs; no longer primarily about inlining advice to the compiler |
| Templates and instantiation | [08](08-compilation-abi/CONCEPTS.md#templates-instantiation) | ODR, Class templates (xref 05) | 5 | 08-P13..P17 | — | 10, 12 | ☐ | implicit vs explicit instantiation; `extern template` suppresses redundant instantiation across TUs |
| Symbol resolution | [08](08-compilation-abi/CONCEPTS.md#symbol-resolution) | Translation units | 4 | 08-P18..P21 | P-3.1 | — | ☐ | linker-level; MSVC `dumpbin /symbols`, GCC/Clang `nm -C` |
| Name mangling | [08](08-compilation-abi/CONCEPTS.md#name-mangling) | Symbol resolution, Overload resolution (xref 01) | 4 | 08-P22..P24 | P-3.1 | — | ☐ | Itanium ABI (GCC/Clang) vs MSVC mangling are different, non-interoperable schemes — a real ABI-behavior distinction |
| Static libraries | [08](08-compilation-abi/CONCEPTS.md#static-libraries) | Symbol resolution | 3 | 08-P25, P26 | P-3.1 | 10 | ☐ | linked at build time; duplicated across every binary that links them |
| Shared libraries | [08](08-compilation-abi/CONCEPTS.md#shared-libraries) | Static libraries | 4 | 08-P27..P30 | P-3.1 | 09, 10 | ☐ | `.so`/`.dll`; loaded once, shared across processes at the OS level |
| Dynamic linking | [08](08-compilation-abi/CONCEPTS.md#dynamic-linking) | Shared libraries | 4 | 08-P31, P32 | P-3.1 | 09 | ☐ | link-time (import library/`.lib` stub) vs load-time (`dlopen`/`LoadLibrary`) — xref Ch09 for the latter |
| ABI | [08](08-compilation-abi/CONCEPTS.md#abi) | Object representation (xref 07), Name mangling | 6 | 08-P33..P37 | P-3.1 | 09, 10, 13 | ☐ | encompasses calling convention, layout, exception-handling model — none of it is standardized, all of it is compiler/platform-defined |
| Binary compatibility | [08](08-compilation-abi/CONCEPTS.md#binary-compatibility) | ABI | 6 | 08-P38, P39 | P-3.1 | 13 | ☐ | adding a virtual function, reordering members, or changing a default argument can all silently break it |

## Group: Systems Programming — Linux & Windows Paired (owning chapter: 09)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Processes | [09](09-systems-programming/CONCEPTS.md#processes) | RAII (xref 02) | 3 | 09-P01..P04 | P-4.6 | — | ☐ | `fork`/`exec` vs `CreateProcess` — no Windows equivalent of `fork`'s copy-on-write semantics |
| Threads (OS-level) | [09](09-systems-programming/CONCEPTS.md#threads-os-level) | RAII | 3 | 09-P05, P06 | — | 11 | ☐ | `pthread_create` vs `CreateThread`/`_beginthreadex`; `std::thread` wraps whichever the platform provides |
| Virtual memory | [09](09-systems-programming/CONCEPTS.md#virtual-memory) | Object representation (xref 07) | 4 | 09-P07..P10 | — | 12 | ☐ | page tables, TLB; both platforms use paging but expose different tuning knobs (huge pages vs large pages) |
| System calls | [09](09-systems-programming/CONCEPTS.md#system-calls) | Processes | 3 | 09-P11, P12 | — | — | ☐ | syscall vs Win32 API layered over NT native API — the Windows "syscall" is rarely called directly by application code |
| File descriptors vs HANDLEs | [09](09-systems-programming/CONCEPTS.md#file-descriptors-vs-handles) | RAII, Error codes (xref 06) | 3 | 09-P13..P18 | P-1.2, P-3.2 | — | ☐ | see paired table in CURRICULUM.md §5 Ch09 |
| Files | [09](09-systems-programming/CONCEPTS.md#files) | File descriptors vs HANDLEs | 3 | 09-P19..P22 | P-3.4 | — | ☐ | buffered stdio vs unbuffered syscalls on both platforms |
| Sockets | [09](09-systems-programming/CONCEPTS.md#sockets) | File descriptors vs HANDLEs | 4 | 09-P23..P27 | P-3.7, P-4.1, P-5.3 (xref 11) | 11 | ☐ | Winsock requires explicit `WSAStartup`/`WSACleanup`; POSIX sockets need none |
| TCP | [09](09-systems-programming/CONCEPTS.md#tcp) | Sockets | 4 | 09-P28..P30 | P-3.7, P-5.3 | 11 | ☐ | protocol itself is platform-independent; the framing/backpressure problem is application-level |
| Memory mapping | [09](09-systems-programming/CONCEPTS.md#memory-mapping) | Virtual memory, Placement construction (xref 07) | 5 | 09-P31..P35 | P-3.1, P-5.4 | 12 | ☐ | see paired table in CURRICULUM.md §5 Ch09 |
| Shared-library runtime loading | [09](09-systems-programming/CONCEPTS.md#shared-library-runtime-loading) | Dynamic linking (xref 08) | 5 | 09-P36..P39 | P-5.6 | 10, 13 | ☐ | see paired table in CURRICULUM.md §5 Ch09 |
| Signals | [09](09-systems-programming/CONCEPTS.md#signals) | Processes | 5 | 09-P40..P44 | P-4.6 | — | ☐ | see paired table in CURRICULUM.md §5 Ch09 |
| Debugging tools | [09](09-systems-programming/CONCEPTS.md#debugging-tools) | — | 3 | 09-P45..P50 | P-3.1 (uses these tools) | — | ☐ | see paired table in CURRICULUM.md §5 Ch09; gdb/strace/perf vs WinDbg/ETW/ProcMon |

## Group: Build Systems, Testing, CI (owning chapter: 10)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| CMake basics | [10](10-build-systems/CONCEPTS.md#cmake-basics) | Static/shared libraries (xref 08) | 2 | 10-P01..P04 | P-3.3 | — | ☐ | out-of-source builds are strongly recommended, not enforced by the tool |
| Targets | [10](10-build-systems/CONCEPTS.md#targets) | CMake basics | 3 | 10-P05..P08 | P-3.3 | — | ☐ | modern CMake models everything as targets with properties, not global variables |
| `PRIVATE`/`PUBLIC`/`INTERFACE` | [10](10-build-systems/CONCEPTS.md#private-public-interface) | Targets | 4 | 10-P09..P13 | P-3.3 | — | ☐ | directly models the "what do I need to build me" vs "what do my consumers need" distinction |
| Generator expressions | [10](10-build-systems/CONCEPTS.md#generator-expressions) | Targets | 5 | 10-P14, P15 | P-3.3 | — | ☐ | evaluated at generate time, not configure time — a frequent source of confusion when debugging with `message()` |
| Toolchains | [10](10-build-systems/CONCEPTS.md#toolchains) | Targets | 4 | 10-P16, P17 | P-3.3 | — | ☐ | MSVC vs GCC vs Clang flag translation; toolchain files for cross-compilation |
| Dependency management | [10](10-build-systems/CONCEPTS.md#dependency-management) | Targets | 4 | 10-P18..P21 | P-3.3 | — | ☐ | `FetchContent` vs `find_package` vs vendoring — tradeoffs in build time, reproducibility, offline builds |
| Testing (GoogleTest + CTest) | [10](10-build-systems/CONCEPTS.md#testing) | Dependency management | 3 | 10-P22..P25 | P-3.3 | 11 (TSan gate) | ☐ | `gtest_discover_tests` vs `add_test`; CTest labels for selective runs |
| Installation | [10](10-build-systems/CONCEPTS.md#installation) | `PRIVATE`/`PUBLIC`/`INTERFACE` | 5 | 10-P26, P27 | P-3.3 | — | ☐ | `install(TARGETS ... EXPORT ...)` and the generated config file consumers `find_package` against |
| Packaging | [10](10-build-systems/CONCEPTS.md#packaging) | Installation | 5 | 10-P28, P29 | P-3.3 | — | ☐ | CPack; versioning policy ties directly to Ch08/13's ABI-compatibility material |
| CI | [10](10-build-systems/CONCEPTS.md#ci) | Testing, Toolchains | 4 | 10-P30..P33 | P-3.3 | — | ☐ | matrix builds across MSVC/WSL-clang/WSL-gcc; sanitizer jobs as a separate, slower CI lane |
| Sanitizers | [10](10-build-systems/CONCEPTS.md#sanitizers) | Toolchains | 4 | 10-P34, P35 | P-3.3 | 11, 12 | ☐ | **TSan/MSan are unavailable under MSVC** — the `wsl-clang-tsan` preset is mandatory infrastructure, not optional |
| Fuzzing | [10](10-build-systems/CONCEPTS.md#fuzzing) | Sanitizers | 5 | (bundled with 10-P34/35) | — | 13 | ☐ | libFuzzer via Clang; MSVC has no first-party equivalent as of this writing |

## Group: Concurrency (owning chapter: 11)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Threads | [11](11-concurrency/CONCEPTS.md#threads) | Threads (OS-level, xref 09) | 2 | 11-P01..P04 | P-4.3 | — | ☐ | `std::thread` is not joinable-by-default-safe — forgetting `join()`/`detach()` terminates the program |
| `jthread`/`stop_token` | [11](11-concurrency/CONCEPTS.md#jthread-stop_token) | Threads | 3 | 11-P05..P07 | P-4.3 | — | ☐ | C++20; auto-joins on destruction and carries cooperative cancellation |
| Mutexes | [11](11-concurrency/CONCEPTS.md#mutexes) | RAII (xref 02) | 2 | 11-P08..P11 | P-4.2, P-4.3 | — | ☐ | `std::mutex` is not recursive; `recursive_mutex` exists but signals a design smell |
| Locks | [11](11-concurrency/CONCEPTS.md#locks) | Mutexes | 3 | 11-P12..P14 | P-4.2 | — | ☐ | `lock_guard` vs `unique_lock` vs `scoped_lock` (variadic, deadlock-avoiding multi-lock) |
| Condition variables | [11](11-concurrency/CONCEPTS.md#condition-variables) | Mutexes, Locks | 4 | 11-P15..P18 | P-4.2, P-4.3 | — | ☐ | spurious wakeup is real and standard-permitted — always wait on a predicate |
| Futures / promises / async | [11](11-concurrency/CONCEPTS.md#futures-promises-async) | Threads | 3 | 11-P19..P22 | P-4.3, P-5.2 | — | ☐ | `std::async`'s launch policy is implementation-defined unless specified explicitly |
| Atomics | [11](11-concurrency/CONCEPTS.md#atomics) | Object representation (xref 07) | 5 | 11-P23..P28 | P-4.2 | 12, 13 | ☐ | lock-free-ness is queryable (`is_lock_free()`) but implementation/platform-dependent |
| Data races | [11](11-concurrency/CONCEPTS.md#data-races) | Atomics | 5 | 11-P29..P33 | P-4.2 | 12, 13 | ☐ | standard-defined UB, not just "unlikely to be correct" — the whole reason TSan exists |
| Happens-before | [11](11-concurrency/CONCEPTS.md#happens-before) | Data races | 6 | 11-P34, P35 | — | — | ☐ | the formal relation the memory model uses to define "safe" |
| Synchronization | [11](11-concurrency/CONCEPTS.md#synchronization) | Happens-before | 5 | 11-P36..P38 | P-4.2, P-4.3 | — | ☐ | mutexes/atomics/condition variables are all mechanisms; synchronization is the property they establish |
| Memory ordering | [11](11-concurrency/CONCEPTS.md#memory-ordering) | Happens-before | 6 | 11-P39..P44 | P-4.2 | 12, 13 | ☐ | `relaxed`/`consume`/`acquire`/`release`/`acq_rel`/`seq_cst` — all six, not just the two most-cited |
| Lock-free programming | [11](11-concurrency/CONCEPTS.md#lock-free-programming) | Memory ordering | 7 | 11-P45..P48 | P-4.2 | — | ☐ | ABA problem; a tagged/versioned pointer mitigates ABA specifically, not safe reclamation (see next row) |
| Safe memory reclamation (hazard pointers, EBR) | [11](11-concurrency/CONCEPTS.md#safe-memory-reclamation-hazard-pointers-and-epoch-based-reclamation) | Lock-free programming | 7 | 11-P36, P56 | P-4.2 | — | ☐ | answers "when is it safe to free an unlinked node" — a genuinely separate hazard from ABA, often conflated with it |
| Thread pools | [11](11-concurrency/CONCEPTS.md#thread-pools) | Futures, Synchronization | 5 | 11-P49, P50 | P-4.3 | 12, 13 | ☐ | work-stealing vs fixed-queue designs trade contention for locality differently |
| Work queues / producer-consumer | [11](11-concurrency/CONCEPTS.md#work-queues-producer-consumer) | Condition variables | 5 | 11-P51, P52 | P-4.2, P-4.3 | — | ☐ | bounded vs unbounded queue backpressure semantics |
| Deadlocks / starvation / livelock | [11](11-concurrency/CONCEPTS.md#deadlocks-starvation-livelock) | Locks | 5 | 11-P53, P54 | — | — | ☐ | lock ordering discipline and `std::scoped_lock` as the standard-provided mitigation for deadlock specifically |
| False sharing (correctness/contention angle) | [11](11-concurrency/CONCEPTS.md#false-sharing) | Atomics, Alignment (xref 07) | 6 | 11-P55 | P-4.4 | 12 (measured) | ☐ | correctness is unaffected; performance is not — the measured half lives in Ch12 |

## Group: Performance (owning chapter: 12)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Algorithmic complexity (measured) | [12](12-performance/CONCEPTS.md#algorithmic-complexity) | Algorithms (xref 04) | 2 | 12-P01, P02 | P-5.1 | — | ☐ | asymptotic guarantees are standard-mandated where stated; constants are not |
| Allocations | [12](12-performance/CONCEPTS.md#allocations) | `unique_ptr`/`shared_ptr` (xref 02) | 3 | 12-P03..P06 | P-4.4, P-5.1 | — | ☐ | the allocator call itself is often the dominant cost, more than the memcpy that follows |
| Memory locality | [12](12-performance/CONCEPTS.md#memory-locality) | Object representation (xref 07) | 4 | 12-P07..P09 | P-5.1 | — | ☐ | spatial vs temporal locality; cache-line size is implementation-defined (commonly 64B, not standard-guaranteed) |
| Cache behavior | [12](12-performance/CONCEPTS.md#cache-behavior) | Memory locality | 4 | 12-P10..P13 | P-4.5, P-5.1 | — | ☐ | L1/L2/L3, associativity — observed behavior, verified via `perf`/VTune, not standard-specified |
| Branch prediction | [12](12-performance/CONCEPTS.md#branch-prediction) | — | 4 | 12-P14, P15 | — | — | ☐ | `[[likely]]`/`[[unlikely]]` (C++20) are hints, not guarantees |
| Data layout (AoS/SoA) | [12](12-performance/CONCEPTS.md#data-layout) | Alignment/Padding (xref 07) | 5 | 12-P16..P19 | P-4.5, P-5.1 | 13 | ☐ | SoA trades ergonomics for locality — a real design tradeoff, not a strict improvement |
| False sharing (measured) | [12](12-performance/CONCEPTS.md#false-sharing-measured) | False sharing (xref 11), Data layout | 5 | 12-P20, P21 | P-4.2 (revisit), P-5.1 | 13 | ☐ | `hardware_destructive_interference_size` (C++17) as the standard-provided (but hardware-hinting, not guaranteed) constant |
| Profiling | [12](12-performance/CONCEPTS.md#profiling) | — | 3 | 12-P22, P23 | P-5.1 | — | ☐ | sampling (perf/VTune) vs instrumented (valgrind/callgrind) — different overhead/accuracy tradeoffs |
| Benchmarking | [12](12-performance/CONCEPTS.md#benchmarking) | Profiling | 4 | 12-P24..P28 | P-5.1 | 13 | ☐ | warmup, repetition, statistical reporting, defeating the optimizer (`volatile`, `DoNotOptimize`-style barriers) |
| Compiler optimization | [12](12-performance/CONCEPTS.md#compiler-optimization) | — | 4 | 12-P29..P31 | — | — | ☐ | `/O2` (MSVC) vs `-O2`/`-O3` (GCC/Clang) are not equivalent flag sets |
| Inlining | [12](12-performance/CONCEPTS.md#inlining) | `inline` (xref 08), Compiler optimization | 4 | 12-P32, P33 | — | — | ☐ | the keyword is a *linkage* directive; the optimization decision is entirely heuristic and implementation-defined |
| Vectorization | [12](12-performance/CONCEPTS.md#vectorization) | Compiler optimization | 5 | 12-P34, P35 | — | — | ☐ | auto-vectorization is fragile to aliasing assumptions and data layout; observed via disassembly, not guaranteed |
| Move/copy costs (measured) | [12](12-performance/CONCEPTS.md#movecopy-costs-measured) | Move semantics (xref 03) | 3 | 12-P36 | P-1.4 (revisit) | — | ☐ | the empirical companion to Ch03's semantic-only treatment |
| Allocators (model) | [12](12-performance/CONCEPTS.md#allocators-model) | Object lifetime rules (xref 07) | 6 | 12-P37..P40 | P-4.4 | 13 | ☐ | `std::pmr` polymorphic allocators vs the classic `Allocator` template parameter model |
| Object pools | [12](12-performance/CONCEPTS.md#object-pools) | Allocators (model) | 5 | 12-P41 | P-4.4 | — | ☐ | fixed-size reuse; must handle destruction/reconstruction correctly under the object-lifetime rules |
| Arenas | [12](12-performance/CONCEPTS.md#arenas) | Allocators (model), Placement construction (xref 07) | 5 | (bundled with 12-P37..40) | P-4.4 | 13 | ☐ | bump allocation; the tradeoff is giving up per-object deallocation entirely |

## Group: Modern C++ & Architecture (owning chapter: 13)

| Concept | Chapter | Prerequisites | Diff | Related Problems | Related Projects | Review Checkpoint | Status | Notes |
|---|---|---|---|---|---|---|---|---|
| Ranges (deep composition) | [13](13-modern-cpp-architecture/CONCEPTS.md#ranges-deep) | Ranges introduction (xref 04) | 5 | 13-P01..P04 | — | — | ☐ | custom `view_interface`-derived views; C++20 borrowed/dangling-range rules |
| Views | [13](13-modern-cpp-architecture/CONCEPTS.md#views) | Ranges (deep) | 5 | 13-P05, P06 | — | — | ☐ | lazy, non-owning; composing views chains laziness, evaluated only on iteration |
| Coroutines | [13](13-modern-cpp-architecture/CONCEPTS.md#coroutines) | Move semantics (xref 03), Type erasure (xref 05) | 6 | 13-P07..P12 | P-5.2 | — | ☐ | C++20 low-level facility; no standard `task<T>`/`generator<T>` ships until C++23's `std::generator` |
| `co_await` / `co_yield` | [13](13-modern-cpp-architecture/CONCEPTS.md#co_await-co_yield) | Coroutines | 6 | 13-P13..P17 | P-5.2 | — | ☐ | awaiter protocol (`await_ready`/`await_suspend`/`await_resume`) is the actual mechanism under the keywords |
| Coroutine frames | [13](13-modern-cpp-architecture/CONCEPTS.md#coroutine-frames) | `co_await`/`co_yield` | 7 | 13-P18..P21 | P-5.2 | — | ☐ | heap-allocated unless HALO (heap allocation elision optimization) applies — implementation-defined when it does |
| Modules | [13](13-modern-cpp-architecture/CONCEPTS.md#modules) | Translation units (xref 08) | 5 | 13-P22, P23 | — | — | ☐ | C++20; MSVC and Clang support maturity differs and is changing — verify current tooling state before relying on this in production |
| Modern error handling (synthesis) | [13](13-modern-cpp-architecture/CONCEPTS.md#modern-error-handling-synthesis) | `std::expected` (xref 06) | 6 | 13-P24, P25 | — | — | ☐ | synthesis problem set, not new material — tests transfer of Ch06 into unfamiliar API shapes |
| API design | [13](13-modern-cpp-architecture/CONCEPTS.md#api-design) | API error design (xref 06) | 6 | 13-P26..P29 | P-5.6 | — | ☐ | the contract a type/function exposes, independent of its implementation |
| Ownership design | [13](13-modern-cpp-architecture/CONCEPTS.md#ownership-design) | Ownership (xref 02) | 6 | 13-P30, P31 | P-5.2, P-5.6 | — | ☐ | choosing unique/shared/borrowed at an architectural boundary, not just within one class |
| Dependency management (design) | [13](13-modern-cpp-architecture/CONCEPTS.md#dependency-management-design) | Dependency management (xref 10) | 6 | 13-P32 | — | — | ☐ | as a design axis: what should this component be allowed to depend on, and why |
| Coupling / cohesion | [13](13-modern-cpp-architecture/CONCEPTS.md#coupling-cohesion) | API design | 6 | 13-P33 | — | — | ☐ | classic software-design vocabulary, applied specifically to C++ module/library boundaries |
| Abstraction | [13](13-modern-cpp-architecture/CONCEPTS.md#abstraction) | Coupling/cohesion | 6 | 13-P34 | — | — | ☐ | the cost of the wrong abstraction is at least as real as the cost of none |
| Observability | [13](13-modern-cpp-architecture/CONCEPTS.md#observability) | — | 6 | 13-P35, P36 | P-5.2, C-2 | — | ☐ | logging/metrics/tracing as first-class design concerns, not afterthoughts bolted onto a finished system |
| Backwards compatibility | [13](13-modern-cpp-architecture/CONCEPTS.md#backwards-compatibility) | Binary compatibility (xref 08) | 7 | 13-P37, P38 | P-5.6 | — | ☐ | source compatibility and binary compatibility are different guarantees and can be broken independently |
| Maintainability | [13](13-modern-cpp-architecture/CONCEPTS.md#maintainability) | Abstraction, Coupling/cohesion | 6 | 13-P39 | — | — | ☐ | the property that makes PL-4-style "the build takes 40 minutes, fix it" problems tractable at all |
| Architectural trade-offs | [13](13-modern-cpp-architecture/CONCEPTS.md#architectural-trade-offs) | all of the above | 7 | 13-P40, P41 | C-1, C-2, C-3, PL-1..4 | — | ☐ | the terminal skill of the entire workbook — evaluated by rubric, never a single correct answer |

---

## Reverse Index: Symptom → Concept

Use this when you don't know the concept's name, only that something broke.

| Symptom | Likely Concepts |
|---|---|
| Crash on container resize / insert | Iterator invalidation, reference invalidation, dangling `string_view` |
| Works in Debug, fails in Release (or vice versa) | Strict aliasing violation, uninitialized read, data race, UB masked by debug-build zero-init |
| Works on Linux, fails on Windows (or vice versa) | ABI/CRT mismatch, DLL boundary + C++ type crossing, handle inheritance defaults, path/line-ending assumptions, `errno` vs `GetLastError` |
| Intermittent test failure, passes most runs | Data race, memory ordering too weak, unsynchronized shared state, spurious-wakeup handling missing |
| Program hangs, does not crash | Deadlock (lock ordering), missed `notify`/lost wakeup on a condition variable, unbounded queue backpressure |
| Program is slower after a "safe" refactor | Extra allocation introduced (missing move), lost copy elision, cache-unfriendly data layout, accidental false sharing |
| Program is slower after a compiler/flag upgrade | Changed inlining/vectorization heuristics, disabled devirtualization, stricter aliasing assumptions exposing latent UB — see BC-5 |
| `terminate()` called with no visible `throw` in the stack trace | Exception during stack unwinding, `noexcept` violation, joinable `std::thread` destroyed without `join`/`detach` |
| Double free / heap corruption | Missing rule of 5, dangling pointer used after container reallocation, custom deleter mismatch, ABI mismatch across a DLL boundary |
| Memory leak that doesn't show in a quick run | `shared_ptr` cycle, missing RAII wrapper around a raw handle/fd, forgotten `join`/cleanup path on an exception |
| Function "does nothing" despite being called | `std::move`/`std::forward` misunderstanding (it's a cast, not an action), SFINAE silently removed the overload, template not instantiated because never used |
| Compile error mentioning "ambiguous" or a huge template backtrace | Overload resolution ambiguity, SFINAE misapplied, missing/incorrect constraint, ODR violation surfacing as inconsistent instantiation |
| Linker error: unresolved external / undefined reference | Missing definition, template not instantiated in this TU (missing `extern template` or include), ODR-related inline mismatch, symbol visibility (`-fvisibility=hidden` / lack of `dllexport`) |
| Linker error: duplicate symbol / multiple definition | Missing `inline`, header-defined non-inline function/variable, ODR violation |
| Object slicing / lost polymorphism | Passing/storing a derived object by value through a base-typed parameter or container |
| Benchmark numbers don't reproduce | Missing warmup, optimizer eliminated the "work" (needs a `DoNotOptimize`-style barrier), thermal throttling, background load — not a code bug at all |

---

## Cross-Reference Note

Problem IDs above (`NN-Pnn`) are placeholders for the eventual chapter content — they establish the *count and slot* each concept will occupy, consistent with `CURRICULUM.md`'s per-chapter problem-distribution totals. They will be filled in with actual problem text when each chapter is generated; the ID itself does not change at that point.
