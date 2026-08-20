# Chapter 04 — The Standard Library: Containers, Iterators, Algorithms

> Prerequisites: [Chapter 01](../01-core-semantics/CONCEPTS.md), [Chapter 02](../02-lifetime-raii/CONCEPTS.md), [Chapter 03](../03-value-categories/CONCEPTS.md).
> This chapter is about **choosing correctly and using safely** — not memorizing every member function. Each container section focuses on its invalidation rules and its one or two genuine failure modes, not its full API surface (that's what a reference is for).

## Crash Course

### Container Taxonomy and the Choice That Matters

The standard containers split along two axes: **contiguous vs. node-based** storage, and **sequence vs. associative/unordered** access.

- `vector` — contiguous, dynamic array. Default choice unless you have a specific reason not to.
- `deque` — chunked (not contiguous end-to-end), O(1) push at both ends, no reallocation-driven invalidation of *other* elements on push_back/push_front.
- `list` — node-based doubly-linked list. O(1) insert/erase anywhere given an iterator, but pointer-chasing traversal and per-node allocation overhead. Rarely the right default.
- `map`/`set` — node-based, ordered, `Compare`-based (usually red-black tree). O(log n) operations, stable iterators across insertion.
- `unordered_map`/`unordered_set` — hash-based. O(1) average, O(n) worst case (adversarial hash collisions are a real, exploitable concern for untrusted keys). Rehashing invalidates iterators, similar in spirit to `vector` reallocation.
- `string` — contiguous, like `vector<char>` with extra guarantees (null-termination, SSO in practice).

**The choice that matters most in practice:** default to `vector`, and have a specific, articulable reason before reaching for anything else. "I need to insert in the middle a lot" is not automatically a `list` justification — a `vector`'s cache-friendliness often wins even with O(n) shifting, up to surprisingly large n; measure (Ch12) before assuming.

### Iterator Categories

Iterators form a capability hierarchy, each a refinement of the last: **Input** → **Forward** → **Bidirectional** → **Random Access** → **Contiguous** (C++20). Output iterators are a separate, orthogonal category (write-only, single-pass).

- Input: single-pass, read-only, `++it`, `*it`.
- Forward: multi-pass version of input (can be copied and re-traversed).
- Bidirectional: adds `--it`. (`list`, `map`, `set` provide this.)
- Random Access: adds `it + n`, `it[n]`, `it1 - it2`. (`deque`, `vector`'s superset.)
- Contiguous: random access, plus a guarantee the underlying storage is one contiguous block (`vector`, `string`, `array`).

An algorithm's requirements are stated in terms of the *weakest* category it needs — `std::find` needs only Input, `std::sort` needs Random Access. Passing a `list`'s iterator to `std::sort` is a compile error, not a runtime surprise — the iterator category is checked at compile time via the algorithm's template constraints (formalized with actual C++20 concepts in Ch05; here, know that the check exists and fails at compile time, not that it's built with `requires`-clauses).

### Iterator Invalidation — The Chapter's Central Hazard

Each container has its own invalidation rules; the two dominant patterns:

- **`vector`:** any operation that causes reallocation (a `push_back`/`insert` exceeding capacity) invalidates **all** iterators, pointers, and references to elements. An operation that doesn't cause reallocation but does shift elements (`insert`/`erase` in the middle, no capacity change) invalidates iterators/references to elements *at or after* the modification point, but not before it.
- **`map`/`set`/`list`:** node-based containers give the strongest guarantee — inserting or erasing a *different* element never invalidates iterators to elements you aren't erasing. Erasing element X invalidates only the iterator to X itself; every other iterator remains valid. This is the single biggest practical reason to reach for `std::list` or `std::map` over `vector` when you must hold long-lived iterators across mutations.
- **`unordered_map`/`unordered_set`:** insertion *may* trigger a rehash (when load factor is exceeded), which invalidates all iterators (though, unlike `vector`, **not** references/pointers to individual elements — those remain valid across a rehash for the unordered associative containers specifically, since rehashing only relinks nodes, it doesn't move element storage). Erasure without rehash invalidates only iterators to the erased element.

**The universal trap:** "erase-while-iterating" — calling `container.erase(it)` and then continuing to use `it` (now dangling) instead of using the iterator `erase` returns. The correct idiom, uniformly across containers: `it = container.erase(it);` — never `container.erase(it); ++it;`.

### `string_view` — Borrowing Without Owning

`std::string_view` is a non-owning `{pointer, length}` view into character data it does not manage the lifetime of. It is exactly as dangerous as a raw pointer/reference in this respect, wrapped in a convenient interface that makes it *easy to forget* that danger.

- **Safe:** a `string_view` into a `string`/literal that outlives the view.
- **Classic trap:** a function returning a `string_view` into a local `std::string` — the string is destroyed at function return, the view dangles (identical in shape to Ch03's dangling-reference-to-local problem, just with a view type instead of a reference).
- **Subtler trap:** `std::string_view sv = some_string + "suffix";` — string concatenation with `operator+` produces a **temporary** `std::string`; the `string_view` binds to that temporary's buffer, and the temporary is destroyed at the end of the full expression — `sv` dangles immediately, often with no visible symptom until the memory happens to be reused.

### `std::span` — Borrowing Without Owning, Generalized to Contiguous Ranges

`std::span<T>` (C++20) is `string_view`'s generalization: a non-owning `{pointer, length}` view over any contiguous sequence of `T` — a `std::array`, a `std::vector`, a C array, or a raw pointer-plus-count pair — not just `char`. It carries exactly the same danger profile as `string_view`, for exactly the same reason: it is a view, not an owner.

```cpp
void print_all(std::span<const int> values) {   // accepts array, vector, C array, or {ptr, n} — uniformly
    for (int v : values) std::cout << v << ' ';
}
std::vector<int> v{1, 2, 3};
print_all(v);                 // implicit conversion from vector<int>
int raw[] = {4, 5, 6};
print_all(raw);                // implicit conversion from C array — size is deduced, not passed separately
```

Before `span`, a function that needed to operate generically over "some contiguous block of `T`" had three unappealing choices: template on the container type (loses the ability to express "any contiguous sequence" without over-constraining or under-constraining what's accepted), take a `(T* ptr, size_t n)` pair (loses the size/pointer coupling — nothing stops a caller from passing a mismatched pair), or take a specific container type by reference (rejects perfectly good callers, like a C array or a `std::array`, that aren't that exact type). `span` is the standard, non-owning answer: it holds a pointer and a length, is trivially copyable, and converts implicitly from any of the container shapes above.

`span`'s dangling risk is identical in shape to `string_view`'s: a `span` outlives the container it was formed from (e.g., returned from a function that built a local `std::vector` and returned a `span` into it), or a `span` survives a reallocation of the underlying container (a `push_back` that triggers a `vector` growth invalidates every `span` formed before it, exactly as it invalidates iterators — this is the same invalidation hazard from earlier in this chapter's Crash Course, just reached through a different type).

### `std::format` — Type-Safe, Compile-Time-Checked Text Formatting

`std::format` (C++20, header `<format>`) produces a `std::string` from a format string and arguments, positionally, using `{}` placeholders — a type-safe, extensible alternative to both `printf`-style formatting (no type checking against the format string's `%` specifiers — a mismatched `%d`/`%s` is undefined behavior, not a compile error) and manual `std::ostringstream` chains (verbose, and easy to get flag/precision state leaking between unrelated `<<` calls).

```cpp
std::string s = std::format("{} scored {:.2f}%, rank #{}", name, score, rank);
// {}      -> uses the argument's default formatting (calls its formatter)
// {:.2f}  -> format-spec: fixed-point, 2 decimal places
```

The format string is checked **at compile time** against the supplied arguments' types (a placeholder count mismatch, or a format-spec that doesn't apply to the given argument's type, is a compile error, not a runtime surprise or UB) — this is the concrete, checkable advantage over `printf`, whose format-string/argument-list agreement is entirely the programmer's responsibility and unchecked by the type system. User-defined types gain `std::format` support by specializing `std::formatter<T>`, which is how the mechanism stays extensible without the library needing to know about every type in advance.

### `optional`, `variant`, `any` — Three Different Absence/Polymorphism Tools

- **`std::optional<T>`** — a `T` or nothing. Models "this value might legitimately not exist" (a parse result, a lookup that might miss) without resorting to a sentinel value or a pointer. `.value()` throws `std::bad_optional_access` on an empty optional; `operator*`/`operator->` on an empty optional is undefined behavior (no check) — this distinction (throwing accessor vs. UB accessor) mirrors the general standard-library pattern of `.at()` vs `operator[]`.
- **`std::variant<Ts...>`** — a type-safe tagged union: exactly one of several named alternative types, at any time, with no invalid/uninitialized state (baring the rare valueless-by-exception state after a throwing assignment). `std::visit` is the primary, exhaustive-by-construction way to operate on it — a `switch` you write by hand on an index can silently miss a case when a new alternative is added; `std::visit` with an overload set (or, from C++17, an *overloaded lambda helper*) forces the compiler to check every alternative is handled.
- **`std::any`** — type-erased single-value storage for *any* copyable type, recovered only via `std::any_cast<T>` (throws `std::bad_any_cast` on mismatch). Unlike `variant`, the set of possible types is open-ended and unknown at the point `any` is declared — this flexibility is also `any`'s weakness: there's no compile-time exhaustiveness check possible at all, and misuse degrades to "stringly-typed"-style runtime type confusion if overused. `any` is a tool of last resort, not a default; prefer `variant` whenever the alternative set is closed and known.

### Ranges — Introduction Only (Lazy Filter/Transform/Sort)

C++20 ranges let algorithms operate directly on a range (anything with `begin()`/`end()`) rather than an iterator pair, and introduce **views** — lazy, non-owning adaptors that don't eagerly compute their result.

```cpp
auto result = data | std::views::filter([](int x) { return x % 2 == 0; })
                   | std::views::transform([](int x) { return x * x; });
```

This produces a lazy view — no computation happens until the view is iterated. `std::ranges::sort(container)` sorts in place, directly on a range, without needing to spell out `.begin()`/`.end()`. This chapter stops here deliberately — chaining many adaptors, writing your own view type, and reasoning about a view's iterator invalidation relative to its underlying range's mutation are Chapter 13 material; here, know the shape and the laziness, not the full composition algebra.

### Algorithms: Correctness Requirements, Not Just Behavior

Every standard algorithm has preconditions beyond "the iterators are valid" — `std::sort` requires the range to be *valid for random access* and the comparator to establish a **strict weak ordering** (irreflexive, transitive, and transitively consistent with equivalence) — a comparator that returns `true` for `cmp(a, a)`, or that isn't transitive, produces undefined behavior, not just "a weird sort order." `std::binary_search`/`std::lower_bound`/`std::upper_bound` require the range to already be sorted **according to the same comparator being used to search it** — searching an unsorted range, or sorting with one comparator and searching with another, is silently wrong (typically returns garbage, not a crash) rather than diagnosed.

## Common Misconceptions

1. **"`std::vector::erase` in a loop with `++it` afterward is fine as long as I check bounds."** No — `erase` returns the *next valid* iterator specifically because the iterator you passed in is invalidated by the call itself; incrementing the (already-invalid) old iterator is undefined behavior regardless of bounds-checking logic layered around it.

2. **"`unordered_map` rehashing invalidates element addresses, just like `vector` reallocation."** No — for the unordered associative containers specifically, rehashing relinks existing nodes into new buckets; it does not move or reallocate the elements themselves. Pointers and references to elements remain valid across a rehash; only iterators (which may encode bucket-traversal state) are invalidated.

3. **"A `string_view` is basically a lightweight `const string&`, safe wherever a `const string&` would be."** No — a `const string&` parameter extends no lifetime on its own either, but it's far more commonly bound directly to a named, long-lived object; `string_view` is used far more casually (often from a freshly-computed substring or concatenation), which is exactly why its dangling failure mode shows up in practice more often, not because the underlying rule differs.

4. **"`std::optional<T>::value_or(default)` is just a convenience for `has_value() ? value() : default`."** Mostly true in effect, but note `value_or` always **constructs** the default argument, even when the optional has a value and the default is discarded — if constructing the default is expensive, this can be a surprising cost `has_value() ? value() : compute_default()` (short-circuiting) wouldn't pay.

5. **"`std::variant` always holds one of its listed types — there's no 'empty' state to worry about, unlike `optional`."** Mostly true, but not absolutely — a `variant` can enter the rare **valueless-by-exception** state if a type-changing assignment's construction throws partway through and no fallback storage is available; `holds_alternative`/`visit` correctly account for this, but code that assumes `variant` is *never* checkable-for-emptiness the way `optional` is can be surprised by this corner.

6. **"Any algorithm works on any container as long as I pass `.begin()`/`.end()`."** No — this is precisely the iterator-category mismatch trap: `std::sort` compiles against `list::iterator` only if you're not using the classic (pre-ranges) `std::sort` overload naively — in fact it *fails to compile* against `std::list`'s bidirectional iterators, since `std::sort` requires random access. This is a compile-time category-requirement failure, not a silent runtime issue — but it surprises people who expect "it's an iterator, it should work everywhere."

7. **"`std::span` owns a lightweight copy of the data, the way `std::array` does."** No — `span` never owns or copies anything; it is exactly as much a borrow as `string_view` is, and dangles under identical conditions (the underlying container destroyed, or reallocated via something like `push_back`, while the `span` is still alive).

8. **"`std::format`'s `{}` placeholders are checked the same way `printf`'s `%d`/`%s` are — at runtime, if at all."** No — a `std::format` call's placeholder count and format-spec compatibility with the supplied argument types is checked at **compile time**; a mismatch is a compile error, not the undefined behavior a mismatched `printf` specifier produces.

## Quick Checks

**04-QC1.** Which two operations does a `vector::push_back` need to *not* trigger for existing element references to remain valid?

**04-QC2.** Does erasing one element from a `std::map` invalidate iterators to the map's *other* elements?

**04-QC3.** Does rehashing an `unordered_map` invalidate a raw pointer previously obtained via `&map[key]`?

**04-QC4.** Why does `std::string_view sv = a + b;` (where `a`, `b` are `std::string`) dangle immediately, even though `sv` is used on the very next statement?

**04-QC5.** What's the difference between `optional<T>::value()` and `optional<T>::operator*()` when the optional is empty?

**04-QC6.** Why is `std::visit` generally preferable to a hand-written `switch` on `variant::index()`?

**04-QC7.** Does `std::views::filter(...)` eagerly compute its filtered result when the pipeline expression is constructed?

**04-QC8.** What precondition does `std::binary_search` have that, if violated, produces a wrong answer rather than a crash or diagnostic?

**04-QC9.** Why can a function parameter typed `std::span<const int>` accept a `std::vector<int>`, a C array `int[8]`, and a `std::array<int, 8>` all without the caller writing any conversion code?

**04-QC10.** What happens, at compile time versus at runtime, when a `std::format` call's format string has more `{}` placeholders than arguments supplied?

## Problems

### Level 1 — Recognition

**04-P01.** For each container, state its iterator category (Input/Forward/Bidirectional/Random Access/Contiguous): `std::vector`, `std::list`, `std::map`, `std::deque`, `std::forward_list`.

**04-P02.** Which of these standard container operations can trigger a reallocation that invalidates *all* existing iterators/pointers/references: `vector::push_back`, `vector::reserve`, `vector::shrink_to_fit`, `deque::push_back`, `list::push_back`?

**04-P03.** True or false, with one-sentence justification each: (a) `std::map` iterators remain valid after inserting a new key. (b) `std::vector` iterators remain valid after `reserve()` is called when `size() < capacity()` already held before the call and still holds after. (c) `std::unordered_map` element *references* remain valid after a rehash.

**04-P04.** State whether each is legal to construct without UB: (a) `std::string_view sv(literal_c_string);`, (b) `std::string_view sv = some_local_string;` returned by value from the enclosing function as the function's own return value, (c) a `string_view` member of a struct that also holds (by value) the `std::string` it views.

**04-P05.** Match each tool to its best-fit use case: `std::optional<T>`, `std::variant<A,B,C>`, `std::any`. Use cases: (i) a config value that is one of exactly three known formats, (ii) a function result that may legitimately be absent, (iii) a plugin system's per-plugin metadata blob whose type set is not known to the host application.

**04-P06.** For a `std::vector<int> v = {1,2,3,4,5};`, which of these compiles: `std::sort(v.begin(), v.end())`, `std::ranges::sort(v)`, and for `std::list<int> lst = {5,4,3,2,1};`, does `std::sort(lst.begin(), lst.end())` compile? Justify the `list` answer via iterator category.

### Level 2 — Prediction

**04-P07.** 
```cpp
std::vector<int> v = {1,2,3};
v.reserve(100);
int* p = &v[0];
v.push_back(4);
```
Is `p` still valid after the `push_back`? Justify using `reserve`'s guarantee about `capacity()`.

**04-P08.** 
```cpp
std::vector<int> v = {1,2,3,4,5};
auto it = v.begin() + 2;
v.erase(it);
std::cout << *it;
```
Predict whether this compiles, and separately whether it's well-defined at runtime.

**04-P09.** 
```cpp
std::map<int, std::string> m = {{1,"a"},{2,"b"},{3,"c"}};
auto it = m.find(2);
m.erase(1);
std::cout << it->second;
```
Is `it` still valid after `m.erase(1)`? Justify via the node-based-container invalidation guarantee.

**04-P10.**
```cpp
std::unordered_map<int, std::string> m;
m.reserve(1000);
for (int i = 0; i < 500; ++i) m[i] = "x";
std::string* p = &m[10];
for (int i = 500; i < 1000; ++i) m[i] = "y";
std::cout << *p;
```
Given the `reserve(1000)` up front, is a rehash guaranteed *not* to occur during the second loop? If a rehash did occur anyway (e.g., `reserve`'s argument was smaller), would `p` still be valid? Justify both parts.

**04-P11.**
```cpp
std::string build() {
    std::string s = "prefix-";
    s += get_suffix();
    return s;
}
std::string_view sv = build();
std::cout << sv;
```
Does `sv` dangle? Justify precisely — is this the "view into a local" trap or a different mechanism?

**04-P12.**
```cpp
std::vector<std::string> names = {"Alice", "Bob"};
std::string_view sv = names[0];
names.push_back("Carol");
std::cout << sv;
```
Does `sv` dangle after the `push_back`? Which invalidation rule applies, given that `sv` views `names[0]`'s *characters*, not the `vector<std::string>` element slot directly?

**04-P13.** Predict the output:
```cpp
std::optional<int> o;
std::cout << o.value_or(compute_expensive_default());
```
assuming `compute_expensive_default()` prints `"computing\n"` and returns `99`, and separately for `std::optional<int> o2 = 5; std::cout << o2.value_or(compute_expensive_default());`. Does `"computing\n"` print in both cases?

**04-P14.**
```cpp
std::variant<int, std::string> v = 42;
v = "now a string";
std::cout << std::get<std::string>(v);
```
Does this compile and run correctly? What would `std::get<int>(v)` (instead of `std::get<std::string>(v)`) do at that point?

**04-P15.** 
```cpp
std::vector<int> data = {1,2,3,4,5,6};
auto view = data | std::views::filter([](int x){ return x % 2 == 0; });
data.push_back(100);
for (int x : view) std::cout << x << " ";
```
Given that `view` is a lazy filter view over `data`, and `data` is mutated (via `push_back`, potentially reallocating) *before* `view` is iterated, what happens when the loop runs? Reason about what a lazy view actually stores (a reference/iterators into `data`, not a copy) and connect this to `vector`'s own invalidation rules from earlier in this chapter.

**04-P16.** For `std::vector<int> v(10);` then `v.insert(v.begin() + 5, 99);` with no reallocation triggered (assume sufficient spare capacity), which elements' iterators/references are invalidated: none, all, or only those at/after index 5? Justify.

**04-P17.** 
```cpp
std::deque<int> d = {1,2,3,4,5};
int* p = &d[2];
d.push_front(0);
std::cout << *p;
```
Is `p` guaranteed valid after `push_front`? Contrast `deque`'s guarantee here with `vector`'s.

**04-P18.**
```cpp
std::map<std::string, int> m;
m["a"] = 1;
auto& ref = m["a"];
m["b"] = 2;
std::cout << ref;
```
Is `ref` still valid and still referring to `m["a"]`'s value after inserting `"b"`? Justify via the map invalidation guarantee (contrast with what would happen if `m` were instead a `std::vector<std::pair<std::string,int>>` being appended to).

**04-P19.**
```cpp
std::vector<int> v = {3,1,2};
std::sort(v.begin(), v.end(), [](int a, int b) { return a <= b; });
```
This comparator uses `<=` instead of `<`. Does this compile? Is it well-defined behavior at runtime? Justify using the strict-weak-ordering requirement (specifically the irreflexivity clause: what does `cmp(a, a)` evaluate to here, and why does that matter).

**04-P20.**
```cpp
std::vector<int> sorted_desc = {9,7,5,3,1};
auto it = std::lower_bound(sorted_desc.begin(), sorted_desc.end(), 5);
```
`sorted_desc` is sorted in *descending* order, but `lower_bound` defaults to assuming ascending order (`operator<`). Predict whether this compiles, and whether the result is meaningful. What's the minimal fix?

### Level 3 — Implementation

**04-P21.** Write a function `template<typename Container> void erase_all_even(Container& c)` that removes all even values from `c`, correctly handling the erase-while-iterating hazard for both a `std::vector<int>` and a `std::list<int>` (your function should work for both, but you may use `if constexpr` or an overload if the idiomatically-efficient approach genuinely differs between them — state which approach you chose and why).

**04-P22.** Implement a function `std::string_view safe_trim(const std::string& s)` that returns a view of `s` with leading/trailing whitespace excluded — and explain, in a comment or accompanying note, the exact lifetime contract callers must uphold for the returned view (what must remain true about `s` for the view to stay valid), since the function's signature alone can't enforce it.

**04-P23.** Write a small multi-index lookup structure: given a `std::vector<Person>` (with `id` and `name` fields), build both a `std::unordered_map<int, size_t>` (id → index) and a `std::map<std::string, size_t>` (name → index) as secondary indices into the vector. Explain precisely why storing an **index** rather than a pointer/reference into the vector is the safer design here, tying your answer to `vector`'s reallocation-invalidation rule.

**04-P24.** Implement `template<typename T> std::optional<T> parse(std::string_view s)` for `T = int` (using `std::from_chars` or equivalent) that returns `std::nullopt` on malformed input rather than throwing. Write three call sites demonstrating: a successful parse, a malformed-input case, and a case using `.value_or(...)` to supply a fallback.

**04-P25.** Given `using Event = std::variant<ClickEvent, KeyEvent, ScrollEvent>;` (three distinct structs), implement a `handle(const Event& e)` function using `std::visit` and an overloaded-lambda-set helper (the classic `template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };` pattern), such that adding a fourth alternative to the `variant` and forgetting to handle it in `handle` causes a **compile error**, not a silent no-op. Demonstrate (in comment form, since you can't literally show a compile failure in running code) what happens if you instead used a hand-rolled `switch` on `e.index()`.

**04-P26.** Write a function `template<typename Range> auto sum_even_squares(Range&& r)` using a `views::filter` + `views::transform` pipeline (from the Crash Course) to lazily filter even numbers and square them, then fold the result with `std::ranges::fold_left` (or an explicit accumulation loop if your toolchain's ranges support doesn't include `fold_left`) to produce the sum. State, referencing laziness, whether the filter/transform steps run once total or once per element as the final accumulation traverses the pipeline.

**04-P27.** Implement a small LRU cache using `std::list<std::pair<Key,Value>>` for order tracking plus a `std::unordered_map<Key, std::list<...>::iterator>` for O(1) lookup. Explain precisely why storing a `std::list` iterator in the map (rather than an index or pointer) is safe and stays valid across the cache's `list::splice`-based reordering operations — tie this to the node-based-container invalidation guarantee.

**04-P28.** Implement `template<typename T, typename Compare> void insertion_position_check(std::vector<T>& v, Compare cmp)` that asserts (or returns a bool) whether `v` is correctly sorted with respect to `cmp` **and** that `cmp` itself is a valid strict weak ordering over the actual elements present — i.e., for every pair of adjacent elements, `!cmp(v[i+1], v[i])` holds, and spot-check irreflexivity (`!cmp(x, x)`) for each element. Explain why this check, while not exhaustive (it can't prove transitivity in general), catches the specific class of bug in 04-P31's family.

**04-P29.** Implement a small "view-safe" split function `std::vector<std::string_view> split(std::string_view s, char delim)` that returns views into the *original* `s` (no copies of the substrings). Write a caller-facing comment/note stating the exact lifetime contract (what must remain true about the object `s` was constructed from) that every caller must uphold, and demonstrate one correct call site and one call site that would violate the contract (in comment form, marked clearly as "do not run").

**04-P30.** Implement a generic `template<typename Map> auto get_or_compute(Map& m, const typename Map::key_type& key, auto&& compute)` helper that returns a reference to `m[key]`'s value, computing and inserting it via `compute()` only if the key is absent (avoid double-lookup — don't call `m.find` and then `m[key]` separately). State which single `map`/`unordered_map` member function makes this achievable in one call, and why using `operator[]` alone (without that member function) would silently default-construct-then-overwrite rather than compute-once.

**04-P31.** Implement a `template<typename Container> void reverse_erase_by_predicate(Container& c, auto pred)` that removes all elements satisfying `pred`, using the erase-remove idiom (`std::remove_if` + `erase`) for a `std::vector`, and the direct `erase-returns-next-iterator` pattern (04-P21's approach) for a `std::list`. Explain in one sentence why the erase-remove idiom is specifically a `vector`/contiguous-container technique and would be a correctness *and* efficiency mistake if applied unchanged to `std::list` (hint: what does `std::remove_if` actually do to the "removed" elements' positions, and does that operation make sense for a node-based container's O(1)-splice removal model).

**04-P32.** Implement a `struct StableIndex` wrapper for the vector-plus-secondary-index pattern from 04-P23/04-P41: rather than storing raw `size_t` positions in your `unordered_map`/`map` indices (which go stale on erase, per 04-P41's problem statement), store elements in a `std::vector<std::optional<Row>>` where erase merely sets the slot to `std::nullopt` (a "tombstone") instead of physically removing it, and indices store the now-permanently-stable slot number. Implement `insert`, `erase_by_id` (tombstones, doesn't shift), and `compact()` (an explicit, rarely-called operation that actually reclaims tombstoned slots and rebuilds indices from scratch). State the space/time tradeoff this design makes relative to 04-P41's shift-and-fix-up approach.

### Level 4 — Debugging

**04-P33.** [DEBUG]
```cpp
std::vector<int> v = {1,2,3,4,5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) {
        v.erase(it);
    }
}
```
Identify the bug (there may be more than one interacting issue — find both), and provide the corrected loop.

**04-P34.** [DEBUG]
```cpp
std::unordered_map<std::string, int> counts;
for (const auto& word : words) {
    counts[word]++;
}
auto it = counts.find("target");
counts["brand_new_key_not_seen_before"] = 0;   // may trigger a rehash
std::cout << it->first;
```
Under what condition does this become undefined behavior, and what's the minimal-change fix that avoids relying on that condition never occurring?

**04-P35.** [DEBUG]
```cpp
struct Config {
    std::string raw_line;
    std::string_view key;
    std::string_view value;

    Config(std::string line) : raw_line(std::move(line)) {
        auto pos = raw_line.find('=');
        key = std::string_view(raw_line).substr(0, pos);
        value = std::string_view(raw_line).substr(pos + 1);
    }
};
```
A caller does `Config c("name=value");` and later reads `c.key`/`c.value` and finds garbage. The member initialization order looks correct (raw_line first, since it's declared first). Find the actual bug — it isn't initialization order.

**04-P36.** [DEBUG]
```cpp
std::vector<int> v = {5, 3, 8, 1, 9};
std::sort(v.begin(), v.end());
bool found = std::binary_search(v.begin(), v.end(), 8, std::greater<int>());
```
`found` is computed incorrectly (predict what it actually evaluates to, don't just say "it's wrong"). Explain the precondition violation causing this, referencing the Crash Course's binary-search precondition.

**04-P37.** [DEBUG]
```cpp
class EventBus {
public:
    void subscribe(std::function<void(int)> handler) {
        handlers_.push_back(std::move(handler));
    }
    void publish(int value) {
        for (auto& h : handlers_) {
            h(value);
            if (should_remove_) {
                // a handler itself calls unsubscribe_all() during h(value),
                // clearing handlers_ mid-loop
            }
        }
    }
private:
    std::vector<std::function<void(int)>> handlers_;
    bool should_remove_ = false;
};
```
If a handler invoked inside `publish`'s loop causes `handlers_` to be cleared (e.g., by calling a method that does `handlers_.clear()`) *during* the `for` loop's execution, what happens to the loop's iterator on the next `++` or comparison? Propose a fix that makes `publish` safe against handlers that mutate `handlers_` reentrantly.

**04-P38.** [DEBUG]
```cpp
std::vector<std::string> tokens = tokenize(input);
std::string_view first_token = tokens.empty() ? "" : tokens[0];
process(first_token);
```
Assume `tokens[0]` is a `std::string` with a longer-than-SSO-threshold value (heap-allocated), and `""` is a string literal. Is there a lifetime bug in the ternary expression itself, independent of anything happening after this statement? (Hint: consider what type the ternary's common type is, and whether a temporary is created.)

**04-P39.** [DEBUG]
```cpp
std::map<int, int> m = {{1,10},{2,20},{3,30}};
for (auto& [key, value] : m) {
    if (key == 2) {
        m.erase(key);   // erase by key, not by iterator, mid-range-for
    }
}
```
Range-based `for` over an associative container desugars to begin()/end() iterator use internally. Explain precisely why erasing by key (rather than capturing and using the returned iterator) inside this loop is dangerous, and what the safe rewrite looks like (note: range-`for` doesn't give you direct access to the underlying iterator to reassign, so the fix likely requires abandoning range-`for` here — say so if that's your conclusion).

**04-P40.** [DEBUG]
```cpp
std::optional<std::string> get_name(int id) {
    if (id < 0) return {};
    static std::string cache[100];
    cache[id] = compute_name(id);
    return cache[id];
}
auto name = get_name(5);
std::cout << *name;
```
This compiles and usually "works," but a reviewer flags it as fragile/wrong for reasons beyond the obvious `id >= 100` bounds issue (assume bounds are separately validated elsewhere and not the point of this review). Identify the design smell around using a `static` array as backing storage for values now being returned as independent `optional<std::string>` copies — is there an actual lifetime bug, or "just" a bad pattern? Justify precisely.

### Level 5 — Integration

**04-P41.** Design and implement a small in-memory "database table" over `std::vector<Row>` (where `Row` has an `id`, a `name`, and a `score`), with two secondary indices — `std::unordered_map<int, size_t>` for id lookup and `std::multimap<int, size_t>` (score → row-index, allowing duplicate scores) for range queries by score. Implement `insert`, `erase_by_id`, and `find_by_score_range(int lo, int hi)`. Address explicitly: what happens to your indices' stored indices when a row in the middle of the vector is erased (hint: `vector::erase` shifts every subsequent element's index down by one) — either fix this in your `erase_by_id` implementation (by updating affected index entries) or justify a design change (e.g., swap-with-last-then-pop, or a stable-index scheme) that avoids the problem entirely.

**04-P42.** Implement a small streaming log-line processor: given a large (conceptually multi-GB, though your test input can be modest) text file processed line-by-line, use `std::string_view` throughout the per-line parsing (splitting on delimiters, extracting fields) to avoid allocating a new `std::string` per field. Explicitly identify every point in your implementation where a `string_view` could accidentally outlive the buffer it views (e.g., if a line buffer is reused/overwritten for the next line while an earlier line's views are still held), and either eliminate those points structurally or document why they can't occur given your control flow.

**04-P43.** Build a small type-safe event dispatcher using `std::variant` over a closed set of ≥4 event types, with a `std::vector<Event>` event queue and a `dispatch()` function using `std::visit`+overloaded-lambda-set (per 04-P25's pattern) to handle each. Add a fifth event type later (as a deliberate second step) and confirm/demonstrate that every existing `visit` call site either handles it or fails to compile — no silent gaps. Contrast, in a short written justification, why this closed-set design is more appropriate here than reaching for `std::any` per the Crash Course's "any is a last resort" guidance.

**04-P44.** Implement a `ranges`-based pipeline that reads a `std::vector<int>`, filters values above a threshold, transforms them via a caller-supplied function, and returns the result materialized into a `std::vector<int>` (i.e., you must eagerly collect the lazy view at the end — research/use `std::ranges::to<std::vector<int>>()` if your toolchain supports it, or an explicit loop otherwise). Then write a *second* version that keeps the pipeline lazy and returns the view itself, and explain the concrete lifetime hazard the caller must now avoid that the eager version didn't have (tying back to 04-P15's dangling-view-over-a-mutated-container scenario).

**04-P45.** Extend 04-P27's LRU cache to support a `resize(size_t new_capacity)` operation that, when shrinking, must evict the least-recently-used entries until the new capacity is satisfied. Implement this using only `list::pop_back` (evicting from the tail, which your design should already treat as "least recently used") plus the corresponding `unordered_map` erasures — explain why evicting via `list::pop_back` plus a matching `map::erase` is safe with respect to the other, non-evicted entries' stored iterators (tie back to 04-P27's original invalidation justification, now under a bulk-eviction operation rather than a single lookup-driven touch).

**04-P46.** Implement a small "generation-counted" index wrapper: instead of 04-P32's tombstone approach, pair every stored secondary-index value with a `generation` counter that increments every time the underlying `std::vector<Row>` is fully rebuilt (e.g., after a `compact()`-style operation), and have lookups check that the caller's remembered generation matches the current one before trusting a stored index, throwing/returning-empty on mismatch rather than silently returning a wrong or stale row. Explain what class of bug (distinct from the raw dangling-index problem in 04-P41) this generation check specifically catches — namely, a caller holding an index value across a `compact()`/rebuild call and using it afterward without realizing the underlying layout changed.

### Level 6 — Production

**04-P47.** You inherit a codebase where a hot-path function takes `const std::string&` for a parameter that is, at every call site in the codebase (verified by grep), either a string literal or a `std::string_view`-compatible substring being explicitly copied into a temporary `std::string` just to satisfy the parameter type. Propose a signature change (to `std::string_view`, presumably) and rigorously enumerate: which existing call sites become *more* efficient (no allocation), which (if any) become newly *dangerous* (a `string_view`-shaped dangling risk that didn't exist with `const std::string&`, since a `const string&` can bind to a temporary and extend nothing but at least guarantees the *string itself*, if it was a real object, outlives the call — whereas a `string_view` constructed from something even shorter-lived than that could dangle in a way the old signature's binding rules would have prevented), and what convention/documentation you'd add to the function to make the new contract clear to future callers.

**04-P48.** A production incident report says: "Our `unordered_map<std::string, Session>` keyed by user-supplied session tokens started taking 40x longer under load, only in production, only for one specific customer's traffic." Given the Crash Course's note on adversarial hash collisions, diagnose the likely root cause, explain why it wouldn't show up in normal testing (what's special about "one specific customer's traffic" here), and propose two independent mitigations — one structural (a change to the container/hashing strategy) and one operational (a change to what's accepted as a key, or how it's derived) — with a brief tradeoff for each.

### Level 3 — Implementation (continued: `span`, `format`)

**04-P49.** Write a function `template<typename T> T sum_all(std::span<const T> values)` that computes a sum over any contiguous range of `T`, and call it with a `std::vector<int>`, a raw C array `int[5]`, and a `std::array<double, 4>` — all without writing any explicit conversion at the call site. Then write a second function `std::span<T> middle_half(std::span<T> s)` that returns a sub-span covering the middle 50% of `s` (rounding however you like), and demonstrate that mutating an element through the returned sub-span mutates the original underlying container's element (i.e., that `span`, like `string_view`, aliases rather than copies).

**04-P50.** Implement a small `struct Point { double x, y; };` and specialize `std::formatter<Point>` (following the standard's `formatter` specialization pattern — a `parse` member reusing the format-spec parsing where reasonable, and a `format` member producing something like `(3.50, -1.25)`) so that `std::format("Point at {}", p)` works directly. Then write a second call using an explicit format-spec passed through to the underlying `double` formatting (e.g., controlling decimal precision) and explain what your `parse` implementation needs to do to support that.

### Level 6 — Production (continued: `span`, `format`)

**04-P51.** A logging function currently takes `const std::vector<int>&` purely to iterate and print its contents, rejecting callers who have the same data in a `std::array` or a stack-allocated C array without first copying it into a `vector`. Propose changing the parameter to `std::span<const int>`, and address explicitly: (a) which existing call sites gain the ability to pass data with zero copies that couldn't before, (b) whether this signature change introduces any *new* dangling risk versus the old `const vector<int>&` signature (reasoning the same way as 04-P47's `string_view` analysis), and (c) why the parameter should be `std::span<const int>` and not plain `std::span<int>` given the function only reads the data.

**04-P52.** A codebase's error/status messages are currently built with `std::ostringstream` chains scattered across dozens of call sites, several of which have been the source of past bugs where a `std::setprecision`/`std::fixed` flag set for one message accidentally leaked into a later, unrelated message sharing the same stream object. Propose migrating these call sites to `std::format`, and explain concretely why `std::format`'s per-call, no-shared-mutable-state design structurally eliminates this specific class of bug (rather than merely making it less likely), plus one situation (if any) where the team might legitimately still prefer `ostringstream`-based construction over `std::format`.

## Integration Challenge — 04-IC1

Build a multi-index dataset lookup system over a collection of `Record { int id; std::string category; double value; }`:

1. Store the canonical data in a `std::vector<Record>`.
2. Build a `std::unordered_map<int, size_t>` index (id → vector position) for O(1) id lookup.
3. Build a `std::multimap<std::string, size_t>` index (category → vector position) for ordered, grouped category lookup.
4. Implement a `std::ranges`-based query: given a category, return a lazy view of all matching `Record`s with `value` above some threshold, sorted descending by `value` (state clearly whether your final sort step forces materialization, breaking laziness — and if so, whether that's actually avoidable here or an inherent limitation of sorting a lazy view).
5. Implement `erase_by_id(int id)` and explicitly resolve the index-invalidation problem from 04-P41 for *both* secondary indices simultaneously (an id-erase must fix up both the `unordered_map` and the `multimap` consistently, not just one).

Write and state your **mutation protocol** as an explicit, documented rule (e.g., "never call vector::erase directly; always call `erase_by_id`, which maintains index consistency as an invariant") — and justify why documenting this as a convention (rather than, say, enforcing it by making the vector private with no direct erase exposed — which you should also just... do, since nothing stops you) is the weaker of the two options, then actually do the stronger one in your implementation.

## Chapter Projects

This chapter feeds directly into:

- **[P-2.1](../PROJECT_ROADMAP.md) Log Line Indexer** — a 1GB-log, `string_view`-based, bounded-memory indexer draws directly on 04-P22, 04-P29, 04-P38, and 04-P42's `string_view`-lifetime discipline, plus this chapter's associative-container indexing patterns (04-P23, 04-P41, 04-IC1).
