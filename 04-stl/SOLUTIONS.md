# Chapter 04 — Solutions

## Quick Check Answers

**04-QC1.** Reallocation (the capacity must not be exceeded) and any capacity-changing operation in general — as long as `size() <= capacity()` continues to hold after the push, no reallocation occurs, and existing references/pointers/iterators to already-present elements stay valid. (The end iterator still changes, but that's not "an existing element reference.")

**04-QC2.** No. `std::map` is node-based; erasing one element invalidates only the iterator to that erased element. Every other iterator, pointer, and reference into the map remains valid — this is the node-based-container guarantee.

**04-QC3.** No. Rehashing an `unordered_map` relinks nodes into new buckets; it does not move or destroy the elements themselves. Pointers and references to individual elements survive a rehash. Only iterators (which may encode bucket/traversal position) are invalidated.

**04-QC4.** `a + b` constructs a temporary `std::string`. `sv` is bound to that temporary's internal buffer. The temporary's lifetime ends at the end of the full expression (the `sv = ...` statement) — not at the end of the block, not after the "next statement." By the time the next statement runs, the buffer `sv` points at has already been destroyed; there is no grace period.

**04-QC5.** `.value()` throws `std::bad_optional_access` if the optional is empty — a checked, diagnosable failure. `operator*()` on an empty optional is undefined behavior — no check, no exception, just UB. Same distinction in spirit as `.at()` vs `operator[]` for containers.

**04-QC6.** `std::visit` (with an exhaustive overload set) forces a compile error if a new alternative is added to the `variant` and not handled. A hand-written `switch` on `.index()` has no such enforcement — adding a new alternative silently falls through to a default case (or no case at all), which is a correctness bug that only manifests at runtime, if ever.

**04-QC7.** No. `std::views::filter` produces a lazy view — constructing the pipeline expression does no computation. Filtering happens incrementally, element by element, only as the view is actually iterated (e.g., in a `for` loop or by an algorithm that consumes it).

**04-QC8.** The range must already be sorted **according to the same ordering/comparator being used for the search**. Violating this (searching an unsorted range, or using a different comparator than the one used to sort) doesn't crash or throw — it silently returns a wrong-but-plausible-looking result, because binary search's decision at each step assumes sortedness it never actually verifies.

## Problem Solutions

### Level 1 — Recognition

**04-P01.**

**Reference Solution:**
- `std::vector` — Random Access (Contiguous, in C++20 terms)
- `std::list` — Bidirectional
- `std::map` — Bidirectional
- `std::deque` — Random Access
- `std::forward_list` — Forward (singly-linked; no `--it`)

**Explanation:** Category tracks what the underlying structure can physically support in O(1)/amortized-O(1): singly-linked → forward only; doubly-linked or balanced-tree-with-parent-links → bidirectional; contiguous-or-chunked-with-index-arithmetic → random access; genuinely one contiguous block → contiguous (C++20 addition, `vector`/`string`/`array` only, not `deque` since `deque` is chunked).

---

**04-P02.**

**Reference Solution:** `vector::push_back` (when it exceeds capacity) and `vector::reserve` (when the requested capacity exceeds the current capacity) can both trigger a reallocation that invalidates everything. `vector::shrink_to_fit` *may* reallocate (implementation-defined whether it does, but if it does, it invalidates everything) — the standard permits but does not require an actual reallocation. `deque::push_back` never invalidates existing elements' iterators/pointers/references to other elements (it may invalidate iterators, but references to already-inserted elements remain valid — deque's chunked layout means push_back/push_front never moves existing elements). `list::push_back` never invalidates anything except possibly the end iterator, since it's node-based.

**Explanation:** The "reallocation invalidates everything" hazard is specific to containers whose storage is one contiguous block that must grow by copying to a new, larger block. `deque` and `list` structurally avoid this by design (chunked storage, node-based storage, respectively).

---

**04-P03.**

**Reference Solution:**
(a) **True.** `map` is node-based; inserting a new key allocates a new node and links it in — existing nodes, and therefore existing iterators to them, are untouched.
(b) **True.** If `size() < capacity()` held before the call, the requested reservation is already satisfied; the standard guarantees `reserve` only reallocates if the requested capacity exceeds the current capacity, so no reallocation occurs and all iterators/pointers/references remain valid.
(c) **True.** Rehashing relinks nodes; it doesn't move or destroy the elements. References survive rehashing for the unordered associative containers specifically (this is *not* true of `vector`, where reallocation does move elements).

---

**04-P04.**

**Reference Solution:**
(a) **Legal.** The literal has static storage duration, outliving any use of `sv` within the same scope.
(b) **Illegal — UB.** `some_local_string` is destroyed when the function returns; the view returned by value dangles immediately upon return, before the caller can even use it.
(c) **Legal, with a caveat.** As long as the `string_view` member is initialized *after* the `std::string` member it views (declaration order determines initialization order) and the struct is never copied/moved in a way that leaves the view pointing at the old object's now-destroyed buffer (a default-generated copy/move constructor would copy the view's raw pointer, leaving it pointing at the *source* object's string, not the newly copied one — a use-after-move/copy bug), this compiles and runs correctly for the straightforward, non-copied case. Flag this as fragile: any implicitly-generated copy or move constructor breaks the invariant silently.

---

**04-P05.**

**Reference Solution:** (i) → `std::variant<A,B,C>` (closed, known set of exactly three alternatives). (ii) → `std::optional<T>` (a single type that may or may not be present). (iii) → `std::any` (open-ended, unknown type set at declaration time).

**Explanation:** The deciding question is always "is the set of possible types closed and known?" `variant` and `optional` both answer yes (with `optional` being the degenerate one-type-or-nothing case); `any` is reached for specifically when the answer is no.

---

**04-P06.**

**Reference Solution:** `std::sort(v.begin(), v.end())` compiles (vector supports random access). `std::ranges::sort(v)` compiles (same reason, range overload). `std::sort(lst.begin(), lst.end())` does **not** compile — `list::iterator` is only Bidirectional, and `std::sort`'s algorithm requires Random Access iterators (it needs `it + n`/`it1 - it2` arithmetic to do its partitioning); the mismatch is caught at compile time via the algorithm's iterator-category requirement, producing a (verbose, template-heavy) compiler error rather than a runtime failure.

---

### Level 2 — Prediction

**04-P07.**

**Reference Solution:** `p` **is** still valid. `reserve(100)` sets capacity to at least 100 while size is 3; the subsequent `push_back` only grows size to 4, which is still well within the reserved capacity, so no reallocation occurs. `reserve`'s guarantee is precisely that a *sufficiently large* prior reservation makes subsequent `push_back`s (up to that reserved capacity) non-reallocating, and therefore non-invalidating for existing element pointers.

---

**04-P08.**

**Reference Solution:** Compiles fine — `erase` and `operator<<` are both valid syntax regardless of iterator validity, since validity is a runtime/semantic property, not a type-system one. At runtime, this is **undefined behavior**: `v.erase(it)` invalidates `it` (and every iterator at or after the erased position); dereferencing the now-invalid `it` afterward has no defined result — it may read garbage, may (in some debug/checked-iterator builds) trigger an assertion, or may appear to "work" by accident.

---

**04-P09.**

**Reference Solution:** Yes, `it` is still valid. `m.erase(1)` erases the node for key `1`; `it` points at the (different) node for key `2`. Per the node-based-container guarantee, erasing one node never invalidates iterators to other, non-erased nodes.

---

**04-P10.**

**Reference Solution:** With `reserve(1000)` up front and never inserting more than 1000 elements total, no rehash is guaranteed to occur during either loop — `reserve(n)` guarantees the container can hold at least `n` elements without rehashing (assuming load-factor-driven rehash thresholds aren't hit below that count, which `reserve` accounts for). If a rehash *did* occur anyway (e.g., because the actual reserved amount had been smaller than needed), `p` would still be valid — for `unordered_map` specifically, a rehash relinks nodes but does not move element storage, so references/pointers to individual elements (unlike iterators) survive a rehash unconditionally.

---

**04-P11.**

**Reference Solution:** Yes, `sv` dangles — but not via the return-a-view-of-a-local mechanism (that's a `string_view`-returning function; `build()` returns `std::string` by value). Here, `build()` returns a `std::string` **by value**, which produces a temporary. `sv` is a `string_view` bound to that temporary's buffer. The temporary's lifetime ends at the end of the full expression `std::string_view sv = build();` — so `sv` dangles immediately, via the same "view bound to a temporary" mechanism as `operator+` concatenation (04-QC4), not the "view returned from inside a function whose local goes out of scope" mechanism from the Crash Course's "classic trap."

---

**04-P12.**

**Reference Solution:** No, `sv` does **not** dangle after the `push_back`. `sv` views `names[0]`'s *character buffer* (the `std::string`'s own internal storage), not the `vector<std::string>` slot. `vector::push_back` triggering reallocation moves the `std::string` *objects* to new storage — but moving a `std::string` (assuming no SSO, i.e., heap-allocated) typically transfers ownership of its internal character buffer pointer rather than copying/reallocating the characters themselves. So the character data `sv` points at survives, even though `names[0]`'s *address as a vector element* has changed. (Caveat, not required by the problem but worth flagging: this relies on `std::string`'s move constructor being a pointer-transfer, which is the practical norm but not a hard guarantee for very short strings using SSO — a short string's data lives inline in the object itself, and moving the object *does* invalidate a view into its old inline buffer. For `"Alice"`, which is short enough to trigger SSO on virtually every implementation, `sv` in fact *would* dangle in practice — this is the sharper, correct answer if SSO is accounted for.)

**Common Wrong Approaches:** Answering "yes it dangles, `vector` reallocation invalidates everything" ignores that `sv` views the *string's* buffer, not the vector's element storage directly — the two are one level removed from each other, and the answer depends on `std::string`'s own move behavior (SSO vs. heap), not `vector`'s.

---

**04-P13.**

**Reference Solution:** For `o` (empty): `"computing\n"` **does** print, because `value_or` unconditionally evaluates its argument before checking `has_value()` — the argument is a function call, and function arguments are evaluated regardless of whether the result ends up being used. For `o2` (has value 5): `"computing\n"` **also** prints, for the same reason — `value_or` isn't short-circuiting; `compute_expensive_default()` is called, its result is discarded, and `5` is returned.

**Explanation:** This is exactly Misconception 4 from the Crash Course — `value_or` looks like a ternary but doesn't short-circuit the way a hand-written `has_value() ? value() : compute_default()` would.

---

**04-P14.**

**Reference Solution:** Compiles and runs correctly, printing `"now a string"`. `variant`'s assignment operator handles the type change transparently — assigning a `std::string` to a `variant<int, std::string>` currently holding an `int` destroys the `int` and constructs the `std::string` alternative in its place. `std::get<int>(v)` at that point would throw `std::bad_variant_access`, since the variant no longer holds the `int` alternative.

---

**04-P15.**

**Reference Solution:** Undefined behavior. `view` is a lazy filter view that stores a reference to (or iterators into) `data`, not a snapshot/copy of its contents. `data.push_back(100)` may reallocate `data`'s underlying storage, which invalidates every iterator into the old storage — including whatever `view` holds internally. Iterating `view` afterward dereferences dangling iterators. This connects directly to `vector`'s own invalidation rule: a lazy view doesn't get a special exemption from the underlying container's invalidation contract — it's just as exposed as any other iterator-holding object, and mutating the underlying container after constructing (but before consuming) a view over it is exactly as dangerous as holding a raw iterator across the same mutation.

---

**04-P16.**

**Reference Solution:** Only elements at or after index 5 are invalidated (specifically, the element previously at index 5 and everything after it shift right by one position; their iterators/references become invalid, or at least refer to a different logical element post-shift). Elements at indices 0–4 are untouched and their iterators/references remain valid, since no reallocation occurs (sufficient spare capacity assumed) and `insert` at position 5 only needs to shift elements from position 5 onward to make room.

---

**04-P17.**

**Reference Solution:** Yes, `p` is guaranteed valid. `deque` is chunked, not contiguous end-to-end; `push_front`/`push_back` allocate a new chunk at the appropriate end when needed but never relocate already-stored elements to do so. This is a strictly stronger guarantee than `vector`, where a capacity-exceeding push invalidates *every* existing pointer/iterator/reference, not just ones "at the far end."

---

**04-P18.**

**Reference Solution:** Yes, `ref` remains valid and still refers to `m["a"]`'s value. `map` is node-based; inserting `"b"` allocates a new, separate node and does not touch or relocate the `"a"` node. Contrast: if `m` were a `std::vector<std::pair<std::string,int>>` and a new pair were appended via `push_back`, a reallocation could move every existing pair (including the one `ref` referred to) to new storage, invalidating `ref` — this is precisely the node-based vs. contiguous distinction from the Crash Course.

---

**04-P19.**

**Reference Solution:** Compiles (the comparator's signature is syntactically fine). It is **undefined behavior** at runtime, because `<=` violates the strict weak ordering's irreflexivity requirement: `cmp(a, a)` must be `false` for a valid strict weak ordering, but `a <= a` evaluates to `true`. `std::sort` is permitted to assume irreflexivity internally (e.g., using it to reason "if not cmp(a,a) then..." in its partitioning logic); violating it doesn't necessarily crash, but the standard makes no behavioral guarantee at all — output may look "almost sorted," may crash, or may (worse) appear correct on small inputs and fail only on larger ones or under a different standard library implementation.

---

**04-P20.**

**Reference Solution:** Compiles — `lower_bound`'s default comparator (`operator<`) is well-typed for `int`. The result is **not meaningful**: `lower_bound` performs a binary search assuming the range is sorted ascending by `<`; searching a descending-sorted range with the default ascending comparator silently violates the "sorted according to the comparator used" precondition, producing an arbitrary (not necessarily even "close") iterator rather than a diagnosed error. Minimal fix: pass `std::greater<int>{}` explicitly as `lower_bound`'s comparator, matching the actual descending order, e.g. `std::lower_bound(sorted_desc.begin(), sorted_desc.end(), 5, std::greater<int>())`.

---

### Level 3 — Implementation

**04-P21.**

**Approach:** Use the `erase`-returns-next-iterator idiom uniformly; it works correctly (if not maximally efficiently) for both `vector` and `list`. Optionally special-case `vector` with the erase-remove idiom for better asymptotic behavior, using `if constexpr` to dispatch.

**Reference Solution:**
```cpp
template<typename Container>
void erase_all_even(Container& c) {
    if constexpr (std::is_same_v<Container, std::vector<int>>) {
        c.erase(std::remove_if(c.begin(), c.end(),
                                [](int x) { return x % 2 == 0; }),
                c.end());
    } else {
        for (auto it = c.begin(); it != c.end(); ) {
            if (*it % 2 == 0) it = c.erase(it);
            else ++it;
        }
    }
}
```

**Explanation:** The uniform `it = c.erase(it)` branch is correct for any container supporting `erase(iterator)`, including `vector` — but for `vector` specifically, each `erase` is O(n) (shifting every subsequent element), making the whole loop O(n²) in the worst case (many erasures). The erase-remove idiom does a single O(n) pass that moves all "kept" elements to the front, then one O(k) erase (k = removed count) — O(n) total, since only one shift-and-truncate happens instead of one shift per erasure. `list::erase` is O(1) per call regardless, so the direct loop is already optimal there and the erase-remove idiom would (per 04-P31) be actively wrong to apply.

**Common Wrong Approaches:** `for (auto it = c.begin(); it != c.end(); ++it) { if (*it % 2 == 0) c.erase(it); }` — increments an already-invalidated iterator; classic erase-while-iterating UB (04-P33's bug).

**Complexity:** `vector` branch: O(n) time via single-pass erase-remove. `list` branch: O(n) time, O(1) per erasure.

---

**04-P22.**

**Reference Solution:**
```cpp
// Lifetime contract: the returned string_view is valid only as long as `s`
// itself remains alive and unmodified. Callers must not let `s` go out of
// scope, be reassigned, or be moved-from while the returned view is in use.
std::string_view safe_trim(const std::string& s) {
    std::string_view sv(s);
    size_t start = sv.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return {};
    size_t end = sv.find_last_not_of(" \t\n\r");
    return sv.substr(start, end - start + 1);
}
```

**Explanation:** The view is constructed from `s`'s own buffer and merely narrows the `{pointer, length}` window via `substr` (which, on `string_view`, never allocates or copies — it just adjusts the pointer/length pair). No new storage is created anywhere in this function, so the returned view's validity is entirely inherited from `s`'s validity — which the function's type signature (`const std::string&`) cannot express or enforce; it's a documentation-only contract.

**C++ Considerations:** If a caller passes a temporary `std::string` directly (e.g., `safe_trim(get_line())`), the temporary is destroyed at the end of the full expression, and the returned view dangles immediately — a real risk this signature invites and does nothing to prevent.

---

**04-P23.**

**Reference Solution:**
```cpp
struct Person { int id; std::string name; };

class PersonTable {
    std::vector<Person> people_;
    std::unordered_map<int, size_t> by_id_;
    std::map<std::string, size_t> by_name_;
public:
    void insert(Person p) {
        by_id_[p.id] = people_.size();
        by_name_[p.name] = people_.size();
        people_.push_back(std::move(p));
    }
    const Person& find_by_id(int id) const { return people_[by_id_.at(id)]; }
};
```

**Explanation:** `vector::push_back` can trigger a reallocation, which moves every `Person` to new storage — any pointer or reference into the old storage becomes dangling. An index (`size_t`) is just a number; it is completely unaffected by where the underlying storage physically lives, so it remains a valid way to locate an element across a reallocation (as long as the *position* itself hasn't changed — which is why erase, addressed in 04-P41, is the harder half of this pattern).

**Common Wrong Approaches:** Storing `Person*` or `Person&` in the indices — correct only until the first reallocation, then silently dangling with no compiler warning.

---

**04-P24.**

**Reference Solution:**
```cpp
template<typename T>
std::optional<T> parse(std::string_view s) {
    T value{};
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc{} || result.ptr != s.data() + s.size())
        return std::nullopt;
    return value;
}

// call sites:
auto a = parse<int>("42");            // a == 42
auto b = parse<int>("not a number");  // b == std::nullopt
int c = parse<int>("oops").value_or(-1); // c == -1
```

**Explanation:** `std::from_chars` reports failure via an error code rather than throwing, matching `parse`'s no-throw contract; checking `result.ptr` against the end of the input additionally rejects partial parses like `"42abc"` (which `from_chars` alone would accept up through `"42"` and silently ignore the trailing garbage).

**C++ Considerations:** `from_chars` requires the input to be contiguous character data reachable via raw pointers, which `string_view::data()` provides directly with no allocation — this is one of the motivating use cases for `string_view` existing as a parameter type in this kind of API.

---

**04-P25.**

**Reference Solution:**
```cpp
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

using Event = std::variant<ClickEvent, KeyEvent, ScrollEvent>;

void handle(const Event& e) {
    std::visit(overloaded{
        [](const ClickEvent& c)  { /* ... */ },
        [](const KeyEvent& k)    { /* ... */ },
        [](const ScrollEvent& s) { /* ... */ },
    }, e);
}
```

**Explanation:** `std::visit` requires the visitor to be callable with *every* alternative type the `variant` can hold. If a fourth alternative (`ResizeEvent`, say) is added to `Event` and `handle`'s overload set isn't updated, `std::visit(overloaded{...}, e)` fails to compile — there's no lambda overload matching `ResizeEvent`, so overload resolution fails at the call site, forcing the omission to be caught immediately.

```cpp
// Hand-rolled switch equivalent, for contrast:
void handle_bad(const Event& e) {
    switch (e.index()) {
        case 0: /* handle ClickEvent */ break;
        case 1: /* handle KeyEvent */ break;
        case 2: /* handle ScrollEvent */ break;
        // if a 4th alternative (index 3) is added and this switch isn't
        // updated, it silently falls through with no case matched — no
        // compile error, no runtime error, just a missed event.
    }
}
```

**Common Wrong Approaches:** The `switch`-on-`.index()` form above — it compiles fine both before and after a new alternative is added, silently doing nothing for the new case. This is exactly the gap `std::visit` + exhaustive overload set closes.

---

**04-P26.**

**Reference Solution:**
```cpp
template<typename Range>
auto sum_even_squares(Range&& r) {
    auto view = std::forward<Range>(r)
              | std::views::filter([](int x) { return x % 2 == 0; })
              | std::views::transform([](int x) { return x * x; });
    int sum = 0;
    for (int x : view) sum += x;
    return sum;
}
```

**Explanation:** Each element is pulled through the *entire* pipeline (filter check, then transform, then accumulate) one at a time as the final `for` loop advances — laziness here means "no intermediate materialized container exists between stages," not "each stage runs as a separate batch pass." So filter/transform run once *per element*, interleaved with the accumulation, rather than once *in total* over the whole range up front.

**C++ Considerations:** If `fold_left` is unavailable on the toolchain, the explicit loop above is a faithful substitute with identical laziness characteristics.

---

**04-P27.**

**Reference Solution:**
```cpp
template<typename Key, typename Value>
class LRUCache {
    using ListT = std::list<std::pair<Key, Value>>;
    ListT items_;
    std::unordered_map<Key, typename ListT::iterator> index_;
    size_t capacity_;
public:
    explicit LRUCache(size_t cap) : capacity_(cap) {}

    void put(const Key& k, Value v) {
        auto it = index_.find(k);
        if (it != index_.end()) {
            it->second->second = std::move(v);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }
        items_.emplace_front(k, std::move(v));
        index_[k] = items_.begin();
        if (items_.size() > capacity_) {
            index_.erase(items_.back().first);
            items_.pop_back();
        }
    }
};
```

**Explanation:** Storing a `std::list` iterator (rather than an index or pointer) is safe because `list` is node-based: `splice` relinks existing nodes into a new position without destroying or reallocating any of them, and per the node-based-container invalidation guarantee, an iterator to a node remains valid as long as that specific node isn't erased — regardless of how many times it's relinked elsewhere in the list via `splice`. An index would go stale the moment ordering changes; a raw pointer to the `pair` would technically also survive `splice` (nodes aren't moved in memory), but the iterator is the idiomatic, self-documenting choice that also supports `erase` directly.

**Complexity:** O(1) amortized for `put` (hash lookup + splice, both O(1); occasional eviction also O(1)).

---

**04-P28.**

**Reference Solution:**
```cpp
template<typename T, typename Compare>
bool insertion_position_check(const std::vector<T>& v, Compare cmp) {
    for (size_t i = 0; i + 1 < v.size(); ++i) {
        if (cmp(v[i+1], v[i])) return false;   // not sorted w.r.t. cmp
    }
    for (const auto& x : v) {
        if (cmp(x, x)) return false;           // irreflexivity violated
    }
    return true;
}
```

**Explanation:** Checking `!cmp(v[i+1], v[i])` for every adjacent pair verifies the vector is consistent with `cmp`'s ordering (no adjacent inversion). Checking `!cmp(x, x)` for every element catches the specific irreflexivity violation from 04-P19/04-P31's family of bugs — a comparator using `<=` instead of `<` (or otherwise treating equal elements as "less than" themselves).

**Why It Works:** It's a *necessary*, not *sufficient*, check for a valid strict weak ordering — transitivity can't be verified pairwise without checking all triples (O(n³)), which this deliberately doesn't do; but adjacent-pair sortedness plus irreflexivity catches the overwhelmingly common real-world mistake (an off-by-one in the comparator's inequality) cheaply.

**Complexity:** O(n) — a single pass for each check.

---

**04-P29.**

**Reference Solution:**
```cpp
// Lifetime contract: every returned string_view aliases the character buffer
// owned by `s`. The caller must ensure `s`'s underlying storage (the object
// `s` was constructed from) outlives every view in the returned vector.
std::vector<std::string_view> split(std::string_view s, char delim) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

// correct call site: `line` (a named std::string) outlives the parts vector.
std::string line = "a,b,c";
auto parts = split(line, ',');   // OK — line outlives parts.

// do not run — violates the contract:
// auto parts2 = split(std::string("x,y,z"), ',');
// // the temporary std::string is destroyed at the end of this statement;
// // every view in parts2 dangles immediately.
```

**Explanation:** `string_view::substr` never allocates; it only narrows the `{pointer,length}` window, so every element of the returned vector points directly into `s`'s original buffer — no copies exist anywhere.

---

**04-P30.**

**Reference Solution:**
```cpp
template<typename Map>
auto& get_or_compute(Map& m, const typename Map::key_type& key, auto&& compute) {
    auto [it, inserted] = m.try_emplace(key, std::forward<decltype(compute)>(compute)());
    return it->second;
}
```
Wait — this still calls `compute()` unconditionally (argument evaluation happens before `try_emplace` sees whether the key exists). The single-call, compute-only-if-absent form needs `try_emplace` with a callable-deferred value, which for a general `compute` requires checking first:
```cpp
template<typename Map, typename F>
auto& get_or_compute(Map& m, const typename Map::key_type& key, F&& compute) {
    auto it = m.find(key);
    if (it == m.end()) {
        it = m.try_emplace(key, std::forward<F>(compute)()).first;
    }
    return it->second;
}
```

**Explanation:** `try_emplace` is the single member function that makes "insert only if absent, without touching an already-present value" achievable — unlike `operator[]`, which for a *missing* key always default-constructs the mapped value first (potentially expensive or semantically wrong) and then would require a separate assignment, meaning naive `operator[]`-based code either double-looks-up (`find` then `[]`) or default-constructs-then-overwrites rather than computing once directly into place. The `find`-then-`try_emplace` structure above still avoids calling `compute()` when the key is present (the `if` guards it), which was the actual goal — a version using `try_emplace` alone up front cannot defer `compute()`'s evaluation without a wrapper (e.g., a lazy proxy), which is unnecessary complexity beyond this problem's scope.

**Common Wrong Approaches:** `if (m.find(key) == m.end()) m[key] = compute(); return m[key];` — three lookups instead of two, and doesn't avoid the default-construct-then-overwrite step that `operator[]` performs internally for the insert case.

---

**04-P31.**

**Reference Solution:**
```cpp
template<typename Container, typename Pred>
void reverse_erase_by_predicate(Container& c, Pred pred) {
    if constexpr (std::is_same_v<Container, std::vector<typename Container::value_type>>) {
        c.erase(std::remove_if(c.begin(), c.end(), pred), c.end());
    } else {
        for (auto it = c.begin(); it != c.end(); ) {
            if (pred(*it)) it = c.erase(it);
            else ++it;
        }
    }
}
```

**Explanation:** `std::remove_if` doesn't actually remove anything from the container — for a contiguous range, it shifts "kept" elements leftward over "removed" ones (via move-assignment) and returns the boundary past which elements are considered removed, leaving the container's *size* unchanged until a subsequent `erase(new_end, end())` truncates it. That shifting operation only makes sense (and is only efficient) for a contiguous, random-access container; a node-based `std::list` doesn't need to shift anything at all to erase a node — `list::erase` unlinks and frees a single node in O(1) — so applying `remove_if` to a `list` would be both slower (needlessly copying/moving element values around instead of just unlinking nodes) and conceptually backwards relative to what `list` is designed for.

---

**04-P32.**

**Reference Solution:**
```cpp
template<typename Row>
class StableIndex {
    std::vector<std::optional<Row>> slots_;
    std::unordered_map<int, size_t> id_to_slot_;
public:
    size_t insert(int id, Row row) {
        slots_.push_back(std::move(row));
        size_t slot = slots_.size() - 1;
        id_to_slot_[id] = slot;
        return slot;
    }
    void erase_by_id(int id) {
        auto it = id_to_slot_.find(id);
        if (it == id_to_slot_.end()) return;
        slots_[it->second] = std::nullopt;   // tombstone, no shift
        id_to_slot_.erase(it);
    }
    void compact() {
        std::vector<std::optional<Row>> new_slots;
        std::unordered_map<int, size_t> new_index;
        for (auto& slot : slots_) {
            if (slot) {
                new_index[slot->id] = new_slots.size();
                new_slots.push_back(std::move(slot));
            }
        }
        slots_ = std::move(new_slots);
        id_to_slot_ = std::move(new_index);
    }
};
```

**Explanation:** Tombstoning trades space (tombstoned slots occupy memory and are skipped-but-not-reclaimed until `compact()` runs) for the guarantee that a stored slot number *never* goes stale on erase — unlike 04-P41's shift-and-fix-up approach, where every erase must walk and update every affected secondary-index entry (O(n) per erase in the worst case). Here, `erase_by_id` is O(1); the cost of reclaiming space is deferred entirely to the explicit, rare `compact()` call, which is the point — most workloads erase far more often than they need to reclaim space immediately.

**Complexity:** `insert`/`erase_by_id`: O(1) amortized. `compact()`: O(n), called rarely by design.

---

### Level 4 — Debugging

**04-P33.** [DEBUG]

**Reference Solution:** Two interacting bugs: (1) `v.erase(it)` invalidates `it`, but the loop's `++it` in the `for` header still executes on the invalidated iterator — classic erase-while-iterating UB. (2) Even ignoring (1), erasing an element shifts the next element into the just-erased position; a plain `++it` after a successful erase would skip checking that shifted-in element, so even a "fixed" version using the erase-returns-next-iterator idiom must specifically *not* additionally increment after an erase.

```cpp
for (auto it = v.begin(); it != v.end(); ) {
    if (*it % 2 == 0) it = v.erase(it);
    else ++it;
}
```

---

**04-P34.** [DEBUG]

**Reference Solution:** This becomes undefined behavior specifically if `counts["brand_new_key_not_seen_before"] = 0;` triggers a rehash (i.e., inserting that key pushes the load factor over the container's rehash threshold) — a rehash invalidates `it` (iterators are invalidated by rehash, even though references/pointers to individual elements are not), so `it->first` afterward reads through a dangling iterator. Minimal fix: re-look up the iterator after any insertion that might rehash, i.e. replace `it->first` with `counts.find("target")->first`, or restructure to read `it->first`'s value *before* performing the potentially-rehashing insertion.

---

**04-P35.** [DEBUG]

**Reference Solution:** Initialization order is not the bug — `raw_line` is declared (and thus initialized) first, so it exists before `key`/`value` are computed in the constructor body, and both `string_view` constructions in the body correctly reference the now-fully-constructed `raw_line`. The actual bug: `Config`'s implicitly-generated copy constructor (and move constructor) copies `key` and `value` as raw `{pointer, length}` pairs pointing into the *original* object's `raw_line` buffer — it does **not** re-point them at the new object's own (copied) `raw_line`. So the moment a `Config` is copied or moved anywhere (returned by value, stored in a container that reallocates, etc.), the copy's `key`/`value` views dangle, pointing at the original object's (possibly already-destroyed) `raw_line`. Fix: either delete the copy/move operations and require in-place construction only, or write custom copy/move constructors that recompute `key`/`value` from the new object's own `raw_line` rather than copying the view members verbatim.

---

**04-P36.** [DEBUG]

**Reference Solution:** `v` is sorted ascending by `std::sort(v.begin(), v.end())` (default `operator<`), giving `{1,3,5,8,9}`. Then `binary_search` is called with `std::greater<int>()` — a *descending* comparator — searching a range that is actually sorted *ascending*. `found` evaluates to **`false`**, even though `8` is genuinely present in `v`, because binary search's internal decisions (which half to discard at each step) are made assuming the range is ordered consistently with the comparator passed in; since the range is ascending but the comparator assumes descending, the search discards the half that actually contains `8`. This is precisely the Crash Course's "sorted according to the same comparator used to search" precondition — violating it silently produces a wrong answer, not a diagnosed error.

---

**04-P37.** [DEBUG]

**Reference Solution:** If `handlers_` is cleared mid-loop (e.g., via `.clear()` called from inside a handler), the range being iterated is destroyed out from under the `for` loop; the loop's internal iterator (whether explicit or the range-`for` desugaring) becomes invalid, and the next `++`/comparison against `handlers_.end()` is undefined behavior. Fix: snapshot the handlers (or their count) before the loop begins, or iterate over a copy, so that mutation during dispatch doesn't affect the in-progress iteration:
```cpp
void publish(int value) {
    auto handlers_copy = handlers_;   // snapshot
    for (auto& h : handlers_copy) h(value);
}
```
This trades a copy per `publish` call for reentrancy safety — an acceptable tradeoff unless `publish` is on an extremely hot path with many handlers, in which case a "mark for removal, sweep after the loop" scheme would avoid the copy at the cost of more bookkeeping.

---

**04-P38.** [DEBUG]

**Reference Solution:** Yes — there is a lifetime bug in the ternary expression itself. The ternary operator's two operands are `std::string` (from `tokens[0]`) and `const char*`/string literal (from `""`); to unify to a common type, the compiler converts the string-literal operand to `std::string` as well (or converts both to some common type — in practice, the `std::string` branch typically wins as the common type, forcing the literal to convert to `std::string`) — either way, the *ternary expression as a whole* becomes a **temporary `std::string`**, not a reference to the existing `tokens[0]`. `first_token` is then a `string_view` bound to that temporary's buffer, and the temporary is destroyed at the end of the full expression (the `first_token = ...` statement) — `first_token` dangles before `process(first_token)` is even called on the next line.

---

**04-P39.** [DEBUG]

**Reference Solution:** Range-`for` over `m` desugars to something using `m.begin()`/`m.end()` and an implicit iterator that is advanced by the loop machinery after each iteration body completes. Erasing by key (`m.erase(key)`) inside the loop body erases the node the loop's hidden iterator currently points at — invalidating that hidden iterator — but the loop then still tries to advance it on the next implicit `++`, which is exactly the erase-while-iterating hazard, just obscured by range-`for`'s desugaring. Since range-`for` provides no syntactic hook to capture and reassign the iterator `erase` returns, the safe rewrite must abandon range-`for` for this loop:
```cpp
for (auto it = m.begin(); it != m.end(); ) {
    if (it->first == 2) it = m.erase(it);
    else ++it;
}
```

---

**04-P40.** [DEBUG]

**Reference Solution:** There is no actual dangling-memory lifetime bug — `cache` is `static`, so its storage outlives the function call, and the returned `optional<std::string>` is a genuine *copy* of `cache[id]`'s current contents at the time of the call, not a view or reference into `cache`. So `*name` is well-defined and reads the copy's own data. The design smell: using a fixed-size `static` array as a *cache* keyed by `id` when the values are then handed out as independent, decoupled copies means the "cache" provides no actual benefit (nothing is ever read back from it — every call recomputes and overwrites `cache[id]` unconditionally) while still paying for shared mutable global state, a hidden capacity limit (100), and thread-unsafety (concurrent calls with different `id`s writing to the same static array with no synchronization is a data race, formalized in Ch11, but already smellable here). It's "just" a bad pattern, not a lifetime bug — but a costly one: it looks like caching, provides none of caching's benefit, and introduces real risk (races, silent overwrites, arbitrary capacity ceiling) for no payoff.

---

### Level 5 — Integration

**04-P41.**

**Reference Solution:**
```cpp
struct Row { int id; std::string name; int score; };

class Table {
    std::vector<Row> rows_;
    std::unordered_map<int, size_t> by_id_;
    std::multimap<int, size_t> by_score_;
public:
    void insert(Row r) {
        size_t idx = rows_.size();
        by_id_[r.id] = idx;
        by_score_.emplace(r.score, idx);
        rows_.push_back(std::move(r));
    }
    void erase_by_id(int id) {
        auto it = by_id_.find(id);
        if (it == by_id_.end()) return;
        size_t erased_idx = it->second;
        int erased_score = rows_[erased_idx].score;

        // swap-with-last-then-pop avoids an O(n) index-fixup walk
        size_t last_idx = rows_.size() - 1;
        if (erased_idx != last_idx) {
            std::swap(rows_[erased_idx], rows_[last_idx]);
            by_id_[rows_[erased_idx].id] = erased_idx;
            // fix up by_score_ entry for the row that moved into erased_idx
            auto range = by_score_.equal_range(rows_[erased_idx].score);
            for (auto sit = range.first; sit != range.second; ++sit) {
                if (sit->second == last_idx) { sit->second = erased_idx; break; }
            }
        }
        rows_.pop_back();
        by_id_.erase(it);
        auto range = by_score_.equal_range(erased_score);
        for (auto sit = range.first; sit != range.second; ++sit) {
            if (sit->second == erased_idx || sit->second == last_idx) {
                by_score_.erase(sit); break;
            }
        }
    }
    std::vector<Row> find_by_score_range(int lo, int hi) const {
        std::vector<Row> out;
        for (auto it = by_score_.lower_bound(lo); it != by_score_.end() && it->first <= hi; ++it)
            out.push_back(rows_[it->second]);
        return out;
    }
};
```

**Explanation:** A plain `vector::erase(rows_.begin() + idx)` would shift every subsequent element's index down by one, silently invalidating every secondary-index entry pointing at any of those shifted rows unless each is explicitly walked and decremented (O(n) per erase). Swap-with-last-then-`pop_back` instead only ever moves *one* row (the last one) into the erased slot, so only that one row's index entries need fixing up — O(1) amortized instead of O(n), at the cost of no longer preserving insertion order in `rows_` (acceptable here since order is only exposed via the score index, which is unaffected).

**Complexity:** `insert`: O(log n) (multimap insert). `erase_by_id`: O(log n) amortized. `find_by_score_range`: O(log n + k) where k is the result count.

---

**04-P42.**

**Reference Solution:**
```cpp
void process_log(std::istream& in) {
    std::string line_buffer;
    while (std::getline(in, line_buffer)) {
        std::string_view line(line_buffer);
        auto fields = split(line, '\t');   // reuses 04-P29's split
        // process fields immediately; do NOT store them for later use
        handle_fields(fields);
        // line_buffer is overwritten by the next getline — any view held
        // past this point would dangle.
    }
}
```

**Explanation:** The single point where a `string_view` could accidentally outlive its buffer is `line_buffer` itself, which is reused (overwritten in place, not reallocated fresh) across loop iterations by `std::getline`. Every `string_view` derived from `line` (directly or via `split`) is only valid for the duration of the current iteration's body; the implementation eliminates the hazard structurally by fully consuming `fields` (via `handle_fields`) before the loop advances, and never returning or storing any view derived from `line_buffer` beyond the current iteration's scope.

**C++ Considerations:** If a future maintainer adds "collect all parsed lines into a vector for a second pass," that change would silently reintroduce the dangling hazard (all early lines' views would point at whatever `line_buffer` currently holds — the *last* line read) unless they explicitly copy field contents into owned `std::string`s at that point — worth a comment at the loop site warning against holding views past one iteration.

---

**04-P43.**

**Reference Solution:**
```cpp
struct ClickEvent { int x, y; };
struct KeyEvent { int keycode; };
struct ScrollEvent { int delta; };
struct FocusEvent { bool gained; };
using Event = std::variant<ClickEvent, KeyEvent, ScrollEvent, FocusEvent>;

class Dispatcher {
    std::vector<Event> queue_;
public:
    void push(Event e) { queue_.push_back(std::move(e)); }
    void dispatch() {
        for (auto& e : queue_) {
            std::visit(overloaded{
                [](const ClickEvent&)  { /* ... */ },
                [](const KeyEvent&)    { /* ... */ },
                [](const ScrollEvent&) { /* ... */ },
                [](const FocusEvent&)  { /* ... */ },
            }, e);
        }
        queue_.clear();
    }
};
```

**Explanation:** Adding `FocusEvent` as a fifth alternative and omitting its lambda from the overload set above would fail to compile (per 04-P25's mechanism) — demonstrated by the fact that the overload set here *does* include all four current alternatives and compiles; deleting the `FocusEvent` lambda and attempting to compile again is the "add a case, forget to handle it" scenario, and it fails to build rather than silently skipping focus events at runtime. `std::any` is inappropriate here because the event type set is closed and known at compile time (exactly four kinds of events this application ever produces) — reaching for `any` would throw away the compile-time exhaustiveness check entirely in exchange for no actual flexibility benefit, since new event types still require code changes (a new struct, a new handler) regardless of which tool is used.

---

**04-P44.**

**Reference Solution:**
```cpp
std::vector<int> filtered_transformed_eager(const std::vector<int>& data, int threshold, auto f) {
    auto view = data | std::views::filter([threshold](int x) { return x > threshold; })
                     | std::views::transform(f);
    return std::vector<int>(view.begin(), view.end());   // materializes now
}

auto filtered_transformed_lazy(const std::vector<int>& data, int threshold, auto f) {
    return data | std::views::filter([threshold](int x) { return x > threshold; })
                | std::views::transform(f);
}
```

**Explanation:** The eager version copies the lazily-computed results into an owned `std::vector<int>` before returning, so the caller receives a value fully independent of `data`'s lifetime. The lazy version returns the view itself, which — exactly as in 04-P15 — holds a reference into `data` rather than a copy; if the caller lets `data` go out of scope, gets mutated, or (worse) is itself a temporary passed into this function, the returned view dangles the moment it's used. The lazy version trades this hazard for avoiding the materialization cost — appropriate only when the caller can guarantee `data` outlives the view's use.

---

**04-P45.**

**Reference Solution:**
```cpp
void resize(size_t new_capacity) {
    capacity_ = new_capacity;
    while (items_.size() > capacity_) {
        index_.erase(items_.back().first);
        items_.pop_back();
    }
}
```

**Explanation:** Evicting via `list::pop_back` removes only the tail node; per the node-based-container guarantee (already relied on in 04-P27), erasing one node never invalidates iterators to any *other* node — so every remaining entry's stored `list` iterator in `index_` stays valid throughout the eviction loop, even across multiple consecutive `pop_back` calls. The matching `index_.erase` for each evicted key keeps the two data structures consistent; doing the `list` pop and the `map` erase in the same loop iteration (rather than, say, batching all pops first) avoids ever having a stale `index_` entry pointing at an already-freed node, even transiently.

---

**04-P46.**

**Reference Solution:**
```cpp
struct GenSlot { size_t index; size_t generation; };

class GenerationIndex {
    std::vector<Row> rows_;
    size_t generation_ = 0;
    std::unordered_map<int, GenSlot> by_id_;
public:
    GenSlot insert(Row r) {
        size_t idx = rows_.size();
        rows_.push_back(std::move(r));
        GenSlot slot{idx, generation_};
        by_id_[rows_[idx].id] = slot;
        return slot;
    }
    std::optional<Row> lookup(GenSlot slot) const {
        if (slot.generation != generation_) return std::nullopt;   // stale
        return rows_[slot.index];
    }
    void compact() {
        // ... rebuild rows_/by_id_ as in 04-P32 ...
        ++generation_;
    }
};
```

**Explanation:** This catches a *different* bug class than 04-P41's raw dangling-index problem: 04-P41 is about a single erase silently shifting other rows' indices underneath still-in-use index entries within the *same* generation; the generation check instead catches a caller who holds onto a `GenSlot` value *across* a `compact()`/rebuild call and later tries to use it without realizing the entire layout has since been rebuilt from scratch — even if that caller's specific `id` still logically exists post-rebuild, its slot number is likely now different, and using the old, stale slot number directly (bypassing `by_id_` entirely) would silently read the wrong row. The generation mismatch check turns that into a detectable `nullopt` rather than a wrong-but-plausible-looking result.

---

### Level 6 — Production

**04-P47.**

**Reference Solution:** Call sites passing a string literal directly become strictly more efficient — a `string_view` constructed from a literal has zero allocation cost (versus the old signature's implicit `const std::string&` binding, which for a literal also happened to be zero-copy already, since a `const string&` parameter can bind to a temporary `std::string` implicitly constructed from the literal — so literals were *already* about as cheap; the actual win is confined to the substring case). Call sites that were "explicitly copying a `string_view`-compatible substring into a temporary `std::string` just to satisfy the parameter type" become the primary beneficiaries — those explicit copies can now be deleted entirely, passing the substring's `string_view` directly with no allocation at all. No call site becomes newly *dangerous* under the stated conditions (literals and explicit-substring-copies) — but the signature change itself broadens what the function *accepts* going forward, and future callers who pass, say, the result of a `string`-returning expression that produces a temporary (e.g. `f(a + b)`) would previously have had that temporary's *full lifetime extended through the call* via `const string&` binding rules (binding a reference to a temporary keeps it alive for the duration of the full expression, which covers the call), whereas under `string_view`, the temporary is still destroyed at the end of the full expression, but now nothing about the parameter type visibly signals "you're viewing something that must survive independently" the way a reference-to-owning-type at least loosely implies. **Convention to add:** an explicit doc comment on the function stating the view must remain valid for the duration of the call and that passing the result of a temporary-producing expression (concatenation, `substr` returning a new `string`, etc.) is unsafe — since the type signature alone no longer carries any lifetime-extension guarantee.

---

**04-P48.**

**Reference Solution:** Likely root cause: an adversarial or unusually-structured set of session tokens from that one customer's traffic hashes to a small number of buckets under the standard `std::hash<std::string>` implementation (e.g., due to a client library generating tokens with a shared, exploitable prefix/structure, or genuinely adversarial input crafted to collide), degrading `unordered_map`'s average O(1) lookup toward its worst-case O(n) per operation — a 40x slowdown under load is consistent with heavy bucket clustering, not merely "more traffic." It wouldn't show up in normal testing because test data (synthetic or from well-behaved customers) is typically hash-distributed close to uniformly across buckets, and the pathological case specifically requires input crafted or structured to defeat the particular hash function in use — exactly the scenario the Crash Course flags as "a real, exploitable concern for untrusted keys." **Structural mitigation:** switch to a hash function seeded with a per-process random value (many standard library implementations already randomize `std::hash<std::string>`'s seed per process specifically to defeat this; if the current build doesn't, adopting one that does, or switching the container to a tree-based `std::map` for this specific key type, trades average-case speed for a guaranteed O(log n) worst case that adversarial keys can't defeat). **Operational mitigation:** don't use the raw user-supplied token as the hash key directly — derive the key via an application-controlled transform (e.g., hash the token through a keyed/HMAC-style function known only server-side, or truncate/normalize it) so an external party can no longer choose the exact bytes the container's hash function sees; tradeoff is added CPU cost per lookup (computing the transform) and a small risk of introducing new collisions if the transform is chosen carelessly.

---

**04-P49.**

**Reference Solution:**
```cpp
template<typename T>
T sum_all(std::span<const T> values) {
    T total{};
    for (const T& v : values) total += v;
    return total;
}

std::vector<int> v{1, 2, 3};
int a[] = {1, 2, 3, 4, 5};
std::array<double, 4> arr{1.5, 2.5, 3.5, 4.5};
sum_all<int>(v);      // vector<int> -> span<const int>, no conversion code at call site
sum_all<int>(a);      // C array -> span<const int>
sum_all<double>(arr); // array<double,4> -> span<const double>

template<typename T>
std::span<T> middle_half(std::span<T> s) {
    std::size_t quarter = s.size() / 4;
    return s.subspan(quarter, s.size() - 2 * quarter);
}
```
**Explanation:** `span`'s constructors accept any contiguous range exposing compatible `data()`/`size()` (or a raw array), which is exactly why `sum_all` needs no overloads or template-on-container-type gymnastics — the conversion to `span<const T>` happens implicitly at the call boundary, once, regardless of the caller's actual container type. `middle_half` returns a `span` built from `s.subspan(...)`, which shares the same underlying buffer as `s` — it does not copy — so writing through an element of the returned sub-span writes through the same memory the original container owns; this is the direct demonstration that `span`, like `string_view`, is a view, not a value.

**04-P50.**

**Reference Solution:**
```cpp
struct Point { double x, y; };

template<>
struct std::formatter<Point> {
    std::formatter<double> underlying;   // reuse double's parser/formatter for precision control

    constexpr auto parse(std::format_parse_context& ctx) {
        return underlying.parse(ctx);    // delegate spec parsing (e.g. ".2f") to double's formatter
    }
    auto format(const Point& p, std::format_context& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "(");
        out = underlying.format(p.x, ctx);
        out = std::format_to(out, ", ");
        out = underlying.format(p.y, ctx);
        return std::format_to(out, ")");
    }
};

std::format("Point at {}", p);        // "Point at (3.50, -1.25)" (default double formatting)
std::format("Point at {:.1f}", p);    // format-spec forwarded through to both coordinates
```
**Explanation:** Specializing `std::formatter<Point>` is the mechanism `std::format` requires for any type it doesn't know about natively — the `parse` member consumes whatever appears after the `:` in the placeholder (here, delegated wholesale to an embedded `std::formatter<double>`, so any spec valid for `double` — precision, fixed vs. scientific — becomes valid for `Point` for free), and `format` writes the actual output using whatever the parse step configured. Delegating `parse` to the wrapped `double` formatter, rather than writing a bespoke parser, is what makes the second call's `{:.1f}` spec apply correctly to both coordinates without `Point`'s formatter needing to reimplement floating-point spec parsing itself.

**04-P51.**

**Reference Solution:** (a) Every call site currently forced to build a temporary `std::vector<int>` just to satisfy the old `const vector<int>&` parameter — e.g., a caller holding a `std::array<int, N>` or a stack `int[N]` — can now pass that data directly with zero copies and zero allocation, since `span<const int>` converts implicitly from all of those shapes. (b) No new dangling risk is introduced beyond what `const vector<int>&` already had in kind (both are non-owning references into caller-controlled storage that must outlive the call) — the *shape* of the risk is identical to 04-P47's `string_view` analysis: `span`, like `string_view`, is safe as long as the referenced storage outlives the call, and the switch doesn't change what "outlives the call" requires, it just widens which storage shapes are accepted. (c) The parameter should be `span<const int>`, not `span<int>`, because the function only reads the data — accepting `span<int>` would (i) needlessly reject callers whose data is itself `const` (a `span<int>` cannot be formed from a `const std::vector<int>&`'s data), and (ii) signal, incorrectly, to every caller and reader of the signature that the function might mutate the referenced elements, when it does not.

**04-P52.**

**Reference Solution:** `std::format`'s design takes the format string and all arguments as parameters to a single call, produces a `std::string` (or writes to an iterator via `std::format_to`), and holds no state that persists between calls — there is no shared stream object whose flags (`std::fixed`, `std::setprecision`, etc.) could be set by one call site and observed by an unrelated later call site, because there is no stream object in the picture at all. This structurally eliminates the flag-leakage bug class rather than merely making it rarer: the bug required a *shared mutable formatting-state object* to exist across calls, and `std::format` simply has no such object — each call's format-spec is local to that call's format string. One situation where `ostringstream`-based construction might still be legitimately preferred: incrementally building a single message across many separate append operations interleaved with other logic (e.g., conditionally appending optional sections in a loop) before a single final `.str()`, where `std::format`'s single-call, single-format-string model is a less natural fit than a stream you can keep appending to piecemeal.

## Integration Challenge Solution — 04-IC1

**Reference Solution:**
```cpp
struct Record { int id; std::string category; double value; };

class RecordStore {
    std::vector<Record> records_;
    std::unordered_map<int, size_t> by_id_;
    std::multimap<std::string, size_t> by_category_;

    // Mutation protocol: never touch records_/by_id_/by_category_ directly
    // from outside this class. All mutation goes through insert()/erase_by_id(),
    // which maintain index consistency as an invariant. Enforced by making
    // records_ private with no accessor that exposes a mutable vector reference.
public:
    void insert(Record r) {
        size_t idx = records_.size();
        by_id_[r.id] = idx;
        by_category_.emplace(r.category, idx);
        records_.push_back(std::move(r));
    }

    auto query(const std::string& category, double min_value) const {
        auto range = by_category_.equal_range(category);
        std::vector<Record> matches;
        for (auto it = range.first; it != range.second; ++it)
            if (records_[it->second].value > min_value)
                matches.push_back(records_[it->second]);
        std::sort(matches.begin(), matches.end(),
                  [](const Record& a, const Record& b) { return a.value > b.value; });
        return matches;   // materialized — see note below
    }

    void erase_by_id(int id) {
        auto it = by_id_.find(id);
        if (it == by_id_.end()) return;
        size_t erased_idx = it->second;
        std::string erased_cat = records_[erased_idx].category;
        size_t last_idx = records_.size() - 1;

        if (erased_idx != last_idx) {
            std::swap(records_[erased_idx], records_[last_idx]);
            by_id_[records_[erased_idx].id] = erased_idx;
            auto range = by_category_.equal_range(records_[erased_idx].category);
            for (auto cit = range.first; cit != range.second; ++cit) {
                if (cit->second == last_idx) { cit->second = erased_idx; break; }
            }
        }
        records_.pop_back();
        by_id_.erase(it);
        auto range = by_category_.equal_range(erased_cat);
        for (auto cit = range.first; cit != range.second; ++cit) {
            if (cit->second == erased_idx || cit->second == last_idx) {
                by_category_.erase(cit); break;
            }
        }
    }
};
```

**Explanation:**

**Step 4 (query):** the filter (`value > min_value`) over `equal_range`'s matches could stay lazy via `std::views::filter`, but the final descending-by-value sort forces materialization — `std::sort` requires random-access iterators into a mutable, fully-known-size sequence, which a lazy view over a `multimap::equal_range` (only bidirectional) doesn't provide, and sorting inherently needs to see every element before producing the first output element (there's no way to lazily produce "the largest remaining element" without effectively re-deriving a full sort or a priority-queue-like structure). This is judged an **inherent limitation of sorting a lazy view** in general, not an avoidable inefficiency specific to this design.

**Step 5 (erase_by_id):** resolved via the same swap-with-last-then-pop technique as 04-P41, extended to fix up *both* the `unordered_map` id index and the `multimap` category index for whichever record moves into the erased slot, in a single `erase_by_id` call — avoiding the O(n) shift-everything-after cost a plain `vector::erase` would impose on two independent indices simultaneously.

**Mutation protocol:** stated as "all mutation goes through `insert`/`erase_by_id`," but the stronger, actually-implemented option is that `records_`, `by_id_`, and `by_category_` are all `private` with no mutable accessor exposed — so the invariant isn't just documented, it's structurally impossible to violate from outside the class. Documenting-only is weaker because nothing stops a future maintainer (or the class's own future author, months later) from adding a convenience accessor that returns a mutable reference "just this once," silently reopening the exact hazard the convention was meant to prevent — a private, encapsulated implementation has no such failure mode: the only way to violate the invariant is to edit `RecordStore`'s own member functions, which are exactly the code already responsible for maintaining it.
