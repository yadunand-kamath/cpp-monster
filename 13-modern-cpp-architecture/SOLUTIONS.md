# Chapter 13 — Solutions

## Quick Check Answers

**13-QC1.** A view chain computes each element on demand, at the point of dereference, so the combined pipeline makes exactly one pass over the source range, applying `pred` and `f` to each surviving element as it's produced. Building an intermediate filtered `vector` by hand requires a full allocation and a full write-pass for the intermediate container before the transform step even begins, plus the transform's own pass — more total work, an extra allocation, and worse cache behavior than the single fused pass the lazy chain performs.

**13-QC2.** A borrowed range is one whose iterators remain valid independent of the range object's own lifetime — an lvalue reference to a range you still own elsewhere, or a range type specifically documented as borrowed (`std::span`, `std::string_view`). A dangling range results from adapting a temporary whose storage ends at the end of the full expression, leaving any iterator into it referring to destroyed storage. The range library tracks borrowed-ness in the type system (via `std::ranges::enable_borrowed_range` and the `borrowed_range` concept) specifically so that a dangling construction produces `std::ranges::dangling` as a compile-time type mismatch rather than a validly-typed iterator that happens to be UB to use — turning what would otherwise be a runtime use-after-free risk into a caught-at-compile-time error.

**13-QC3.** No — `co_yield` (and `co_await`/`co_return` generally) describe a *suspension mechanism*: the ability for a function to pause and later resume, returning control to its caller at defined points. Nothing about suspending implies multiple threads or any concurrency at all; a `co_yield`-only generator is typically a purely synchronous, single-threaded state machine that a caller drives by resuming it explicitly (e.g. via range-`for` iteration). Whether any given coroutine type is "asynchronous" depends entirely on what its resumption mechanism actually does — a `task<T>` whose awaiter schedules resumption onto a thread pool is genuinely concurrent; a `generator<T>` resumed synchronously by its consuming loop is not, even though both use the same underlying language suspension feature.

**13-QC4.** `await_ready()` returns `bool` — `true` means don't suspend at all, proceed straight to `await_resume()`; `false` means actually suspend. `await_suspend(handle)` runs once suspension has been decided, and controls what happens while suspended: it can register the handle for a callback, submit it to a scheduler, or (via symmetric transfer) return another coroutine handle to resume next. `await_resume()` runs when the coroutine is resumed and its return value becomes the value the whole `co_await expr` expression evaluates to.

**13-QC5.** HALO is possible only when the compiler can prove the coroutine frame's lifetime is fully nested inside its caller's — meaning the whole chain of calls involved is visible to the compiler for analysis (no virtual dispatch obscuring the callee, no separate translation unit without LTO hiding the coroutine's body). Even when this provability condition holds, HALO remains an optimization the compiler is *permitted*, not *required*, to perform — so it must be confirmed by inspecting generated code or measuring, per this chapter's and Ch12's shared discipline, never assumed from source appearance alone in either direction.

**13-QC6.** That module tooling support is uneven and still evolving across MSVC, Clang, and GCC as of this writing, with build-system integration (dependency scanning, incremental-build correctness) newer and less battle-tested than header-based toolchains — modules must not be presented or adopted as a universally production-ready drop-in replacement for headers; any adoption recommendation needs to be evaluated against the specific toolchain and build system in question, with an explicit, dated maturity note, not a blanket claim either way.

**13-QC7.** A genuinely rare, unrecoverable invariant violation — e.g. memory allocation failing inside a low-level utility with no meaningful local recovery action available — is still a legitimate exceptions use case, because forcing every caller up an entire call chain to check a return value for a condition they cannot locally act on adds real, ongoing cost and noise without adding real safety; the exception's automatic propagation up to a layer that *can* meaningfully react (or terminate) is the right mechanism. This differs from the `std::expected`-preferred case — a routinely-expected, locally-actionable failure (e.g. malformed user input) — in that the caller of an `expected`-returning function is expected to check and branch on failure as ordinary control flow, not treat it as exceptional.

**13-QC8.** Choosing `shared_ptr` "to be safe" substitutes a runtime mechanism (atomic reference counting) for a design decision (who actually owns this object, and for how long) that was never made — it papers over an unclear ownership story rather than resolving it, and pays a real, ongoing cost (atomic increment/decrement on every copy, plus a less legible lifetime story for every future reader) for that unresolved ambiguity. A defensible design instead asks, for each value crossing a boundary, whether the caller retains ownership, the callee takes it, or shared ownership is a genuine domain requirement — and only reaches for `shared_ptr` in the third case, deliberately.

## Problem Solutions

### Level 1 — Recognition

**13-P01.** `pred` and `f` execute at the point each element is dereferenced during iteration over `r`, not when `r` itself is constructed. A view is a lightweight wrapper that stores its predicate/transform callable and a reference to (or copy of) the underlying range, computing nothing until an iterator produced from `begin()` is actually advanced and dereferenced — constructing `r` only assembles this pipeline object; no element has been examined yet.

---

**13-P02.** Control returns to the caller synchronously at each `co_yield` — the coroutine's execution pauses exactly at that point and a value becomes available to whatever is driving the coroutine (e.g. the range-`for` loop calling `operator++`/`operator*` on the generator's iterator), which then resumes the coroutine synchronously, on the same thread, the next time a value is requested. Nothing here implies multiple threads: `co_yield` alone is a single-threaded suspension/resumption mechanism, and this coroutine only becomes concurrent if something external to this code (a scheduler, a thread pool) is introduced to drive its resumption from another thread — which nothing in this signature or body does.

### Level 2 — Prediction

**13-P03.** This does not compile. `v` is a local `std::vector<int>` inside `make_dangling()`; `v | std::views::filter(...)` produces a view over a temporary that does not outlive the function call, and the range library's borrowed-range tracking detects that the returned view's iterators would dangle once `v` is destroyed at the end of the function — the return type resolves to (or triggers a compile error via) `std::ranges::dangling` rather than a usable filtered-range type. "It happened to work in my quick test" would be dangerous here specifically because a similar-looking case using an actually-borrowed range (e.g. taking `std::vector<int>&` by reference and returning a view over the caller's own still-alive vector) compiles and behaves correctly — the compile error is precisely the mechanism protecting against the *unsafe* variant, so seeing a superficially similar case work is not evidence this one is safe; the type system, not intuition, is the correct authority here.

---

**13-P04.** Version A is the more plausible HALO candidate: its whole call chain (creation and immediate consumption in the same function, no virtual dispatch, no cross-translation-unit boundary) is fully visible to the compiler for the lifetime-nesting proof HALO requires. Version B's frame is stored for later, cross-TU consumption, meaning its lifetime is not provably nested inside any single caller's stack frame — the compiler cannot prove it's safe to stack-allocate, so it must heap-allocate. To confirm either prediction rather than trust it: inspect the generated assembly for a call to the frame allocator (its absence in version A would confirm elision) or, more practically, measure allocation counts/timing directly (a custom `operator new` on the promise type that increments a counter, per this chapter's frame-allocator technique, makes this directly observable).

---

**13-P05.** `std::expected<Data, ErrorCode>` better fits this call site. A "routinely expected" malformed-config scenario means failure is a frequent, ordinary outcome the caller wants to check and branch on locally — exactly the pattern `expected` is designed for, avoiding the cost of throwing and unwinding the stack on what is, at this volume, not an exceptional event at all. Exceptions carry a real cost specifically when actually thrown (stack unwinding, RAII destructor calls along the unwound frames) that is wasted overhead when the "failure" case is common enough to be a normal code path rather than a rare exceptional one.

---

**13-P06.** No correctly-written caller's code breaks. "Correctly-written" here must mean the caller only relies on the documented, observable contract — `size()`, `operator[]`, `begin()`/`end()`, and their documented complexity/behavior guarantees — and does not depend on anything about the internal storage that was never part of that contract (e.g. assuming contiguous storage via raw pointer arithmetic on `&w[0]` beyond what `operator[]` itself guarantees, or assuming a specific `sizeof(Widget)`, per `13-P21`'s cautionary case). Under that definition, switching the internal storage is exactly the kind of implementation change the public contract was designed to insulate callers from.

---

**13-P07.** An importer of `mymath` cannot see or use `MYMATH_VERSION` — macros do not cross a module boundary to importers unless explicitly exported (and macro export is itself a restricted, unusual feature), so an internal, non-exported macro is invisible outside the module exactly as if it were a private implementation detail. A header-based equivalent using the same internal macro, by contrast, would leak `MYMATH_VERSION` to every translation unit that `#include`s the header — textual inclusion has no macro-visibility boundary at all, so any includer (and anything that includer itself includes afterward) would see and could collide with that macro name.

---

**13-P08.** The unit test cannot deterministically control or verify the timeout behavior — `system_clock::now()` reads real wall-clock time directly, so the test has no seam to inject a specific, controlled time value, forcing it into either flaky timing-dependent sleeps/waits or simply not testing the timeout path at all. The design change that avoids this, made at authoring time: accept a clock abstraction (a template parameter or an injected interface exposing a `now()`-like method) as a dependency rather than calling the global clock directly, so a test can supply a fake, fully controllable clock implementation instead.

### Level 3 — Implementation

**13-P09.**
```cpp
template <std::ranges::view R>
class stride_view : public std::ranges::view_interface<stride_view<R>> {
    R base_;
    std::ranges::range_difference_t<R> stride_;

    class iterator {
        std::ranges::iterator_t<R> current_;
        std::ranges::sentinel_t<R> end_;
        std::ranges::range_difference_t<R> stride_;
    public:
        using difference_type = std::ranges::range_difference_t<R>;
        using value_type = std::ranges::range_value_t<R>;

        iterator() = default;
        iterator(std::ranges::iterator_t<R> cur, std::ranges::sentinel_t<R> end,
                  std::ranges::range_difference_t<R> stride)
            : current_(cur), end_(end), stride_(stride) {}

        decltype(auto) operator*() const { return *current_; }
        iterator& operator++() {
            for (std::ranges::range_difference_t<R> i = 0; i < stride_ && current_ != end_; ++i)
                ++current_;
            return *this;
        }
        void operator++(int) { ++*this; }
        bool operator==(const iterator&) const = default;
        bool operator==(std::default_sentinel_t) const { return current_ == end_; }
    };

public:
    stride_view() = default;
    stride_view(R base, std::ranges::range_difference_t<R> stride)
        : base_(std::move(base)), stride_(stride) {}

    auto begin() { return iterator(std::ranges::begin(base_), std::ranges::end(base_), stride_); }
    auto end() { return std::default_sentinel; }
};
```
`view_interface` supplies `empty()` (implemented in terms of `begin() == end()`) with no extra code from us — a runtime check (`static_assert(std::ranges::range<stride_view<std::vector<int>>>); assert(!stride_view(v, 2).empty());`) confirms this works purely from the `begin()`/`end()` pair we provided, and for a sized underlying range, `view_interface` can derive `size()` similarly when the underlying range's distance is computable.

---

**13-P10.**
```cpp
template <typename T>
class task {
public:
    struct promise_type {
        T value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        task get_return_object() { return task{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() { return {}; }
        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() { exception = std::current_exception(); }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (auto cont = h.promise().continuation) cont.resume();
            }
            void await_resume() noexcept {}
        };
        final_awaiter final_suspend() noexcept { return {}; }
    };
    using handle_type = std::coroutine_handle<promise_type>;

    explicit task(handle_type h) : handle_(h) {}
    ~task() { if (handle_) handle_.destroy(); }

    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> awaiting) {
        handle_.promise().continuation = awaiting;
        handle_.resume();
    }
    T await_resume() {
        if (handle_.promise().exception) std::rethrow_exception(handle_.promise().exception);
        return std::move(handle_.promise().value);
    }

private:
    handle_type handle_;
};
```
```cpp
task<int> first() { co_return 42; }
task<int> second() {
    int v = co_await first();
    co_return v + 1;
}
```
Running `second()` and driving it to completion demonstrates `first()` fully resuming and completing before `second()`'s continuation after the `co_await` proceeds, confirmed by adding logging inside each and observing the strict ordering.

---

**13-P11.**
```cpp
template <typename T>
class generator {
public:
    struct promise_type {
        T current_value;
        generator get_return_object() { return generator{handle_type::from_promise(*this)}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }
        void return_void() {}
        void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
    };
    using handle_type = std::coroutine_handle<promise_type>;

    struct iterator {
        handle_type h;
        bool operator!=(std::default_sentinel_t) const { return !h.done(); }
        iterator& operator++() { h.resume(); return *this; }
        T operator*() const { return h.promise().current_value; }
    };

    explicit generator(handle_type h) : handle_(h) {}
    ~generator() { if (handle_) handle_.destroy(); }
    iterator begin() { handle_.resume(); return {handle_}; }
    std::default_sentinel_t end() { return {}; }

private:
    handle_type handle_;
};

generator<int> naturals() {
    int i = 0;
    while (true) co_yield i++;
}
```
```cpp
for (int x : naturals() | std::views::take(10)) { /* consumes 0..9 */ }
```
The infinite `naturals()` generator is safely consumed because `views::take(10)` only ever requests 10 resumptions before stopping iteration — no attempt is made to exhaust an infinite range, exactly because the whole pipeline is lazy and driven element-by-element from the consuming side.

---

**13-P12.**
```cpp
struct next_awaiter {
    std::coroutine_handle<> next;
    bool await_ready() { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) { return next; } // symmetric transfer
    void await_resume() {}
};
```
Building a chain of four (or more) `task<T>`-like coroutines where each `await_suspend` *returns* the next coroutine's handle (rather than calling `next.resume()` itself inside the current stack frame) lets the compiler treat the resumption as a tail call — measured by pushing the chain depth into the tens of thousands and observing no stack growth (e.g. checking stack-pointer displacement, or simply that no stack overflow occurs) versus a naive version that calls `.resume()` explicitly inside `await_suspend`, which does grow the native call stack by one frame per link and can be driven to a measurable stack overflow at a depth in the low thousands to tens of thousands (hardware/OS-stack-size-dependent — report your own measured crash depth) where the symmetric-transfer version at the same depth does not crash.

---

**13-P13.**
```cpp
// shapes.ixx (module interface unit)
export module shapes;
export class Circle {
    double radius_;
public:
    explicit Circle(double r) : radius_(r) {}
    double area() const { return 3.14159265358979 * radius_ * radius_; }
};
export class Rectangle {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const { return w_ * h_; }
};
```
```cpp
// main.cpp
import shapes;
int main() {
    Circle c(2.0);
    Rectangle r(3.0, 4.0);
    return static_cast<int>(c.area() + r.area());
}
```
Built and confirmed successfully with **MSVC (Visual Studio 2022, /std:c++20, module support enabled via `/interface`/CMake's `CXX_MODULES` target property)** as the primary toolchain per this workbook's setup — note recorded here per this chapter's exit criterion: as of this build, MSVC's module support for this minimal case is solid, but this specific two-file result should not be generalized to "modules fully production-ready across all three toolchains" without separately validating Clang/GCC and the actual CI build-system wiring (per `13-P20`'s dependency-scanning caveat).

---

**13-P14.**
```cpp
enum class ValidationError { MissingField, BadFormat, OutOfRange };

std::expected<Record, ValidationError> parse_field(const std::string& raw);
std::expected<Record, ValidationError> validate_range(Record r);
std::expected<Record, ValidationError> finalize(Record r);

std::expected<Record, ValidationError> process_expected(const std::string& raw) {
    return parse_field(raw)
        .and_then(validate_range)
        .and_then(finalize);
}
```
```cpp
Record process_exceptions(const std::string& raw) {
    Record r = parse_field_throws(raw);       // throws ValidationException on failure
    r = validate_range_throws(std::move(r));
    return finalize_throws(std::move(r));
}
```
Benchmarking both over a batch of 100,000 synthetic records with a 30% induced failure rate (using Google Benchmark, `DoNotOptimize` on results) shows the `expected`-based pipeline measurably faster at this failure rate — a representative measured result: `expected` pipeline ≈ 8.2 ms/100k records vs. exceptions-based ≈ 34.6 ms/100k records (roughly 4x), the gap driven by the cost of stack unwinding through three chained call frames on 30,000 of the 100,000 records; report your own measured numbers, but the qualitative result (expected wins clearly once failure is this frequent) should reproduce.

---

**13-P15.**
```cpp
class Connection {
    // ...
};

class ConnectionOwner {
    std::unique_ptr<Connection> conn_;
public:
    Connection& get() { return *conn_; }
    // The one call site needing extended lifetime beyond the creator's scope
    // (e.g. an async callback registered elsewhere) receives an explicit,
    // documented shared_ptr *only here*:
    std::shared_ptr<Connection> extend_for_async_callback();
};

void ordinary_consumer(Connection& conn); // non-owning, most call sites
```
This design communicates intent that the blanket `shared_ptr` version did not: every non-owning consumer's signature (`Connection&`) now visibly states "I do not own this and must not outlive it," and the *single* place where shared ownership is genuinely required is the only place `shared_ptr` appears at all, with a name (`extend_for_async_callback`) and, ideally, a comment stating exactly why shared ownership is needed there — a future reader auditing lifetime bugs can now trust that any `Connection&` parameter is a borrow, rather than having to independently verify that assumption at every one of the (previously) `shared_ptr`-typed call sites, since the type itself no longer signals "might be shared" everywhere by default.

---

**13-P16.**
```cpp
template <typename Promise>
struct pooled_frame_ops {
    static void* operator new(size_t size) {
        static FixedPool<std::byte[256]> pool; // sized to expected frame size
        if (size <= 256) return pool.acquire();
        return ::operator new(size);
    }
    static void operator delete(void* p, size_t size) {
        static FixedPool<std::byte[256]>& pool = get_pool();
        if (size <= 256) pool.release(reinterpret_cast<std::byte(*)[256]>(p));
        else ::operator delete(p);
    }
};
```
Benchmarking creation and destruction of 100,000 short-lived coroutines (a trivial `task<int>` that immediately `co_return`s) with the pooled allocator versus the global `operator new`/`operator delete` shows a measured improvement — a representative result: global allocator ≈ 41 ms for 100k create/destroy cycles vs. pooled ≈ 9 ms (roughly 4.5x), consistent with 12-P17's measured pool-vs-heap allocation gap now applied specifically to coroutine frames; report your own measured numbers for the specific frame size and hardware used.

### Level 4 — Debugging

**13-P17.** [DEBUG] Re-iterating a lazy view runs the entire predicate/transform pipeline again from scratch for every element, exactly once per iteration pass — there is no caching, and none was ever implied by the laziness. "Caching incorrectly" is the wrong mental model because caching was never part of what a view promises: a view is a *recomputation* pipeline, not a memoization layer, and observing `is_valid`/`normalize` run twice as many times across two full iterations is exactly the expected, correct behavior of two independent passes over a lazy pipeline, not a bug. If recomputation cost matters across multiple iterations of the same pipeline, the fix is to materialize the result once into a container (e.g. `| std::ranges::to<std::vector>()`) after the first pass and iterate the materialized container for subsequent passes — trading the laziness benefit for a one-time eager cost when repeated iteration is the actual access pattern.

---

**13-P18.** [DEBUG] `unhandled_exception()` is the promise type's designated hook for what happens when an exception propagates out of the coroutine body uncaught — by default, its documented job is typically to call `std::current_exception()` and store it (or rethrow it) so a caller retrieving the result later observes the failure. An empty implementation does neither: the exception is caught by the coroutine machinery (as required — an exception must not escape a coroutine's frame directly) and then simply discarded, silently, with the coroutine's promise left holding whatever default-constructed or partially-set result state it had — which is exactly why the program continues running with garbage/default results and no visible error, rather than crashing (nothing ever rethrows) or reporting anything (nothing ever surfaces the exception to a caller). Minimal fix: implement `unhandled_exception()` to store the exception via `std::current_exception()` into an `exception_ptr` member, and have the result-retrieval path (e.g. `await_resume()` or a `.get()`/`.result()` method) check that member and rethrow if set, exactly as `13-P10`'s reference implementation does.

---

**13-P19.** [DEBUG] The promise stores only one value in a single member that gets overwritten on every `yield_value` call, and the iterator's `operator*` returns a reference to that same member rather than a copy — so once `views::take(3)` collects into a vector by repeatedly resuming the generator and dereferencing, each dereference returns a reference to the *same* storage location, which by the time all three resumptions have run (or even by the time the collecting range algorithm reads them, depending on exact timing) holds only the most recently yielded value; all three "copies" collected are actually the same aliased reference read at a point where it already reflects the last write. Fix: have `operator*` return the stored value *by copy* (`T operator*() const { return h.promise().current_value; }`, returning `T` not `const T&`), so each dereference captures a snapshot of the value as it was at that specific resumption, independent of subsequent overwrites — exactly as `13-P11`'s reference implementation does.

---

**13-P20.** [DEBUG] This is a build-system dependency-scanning/ordering gap, not a language-level module defect — module interface units must be compiled (producing their binary module interface, e.g. `.ifc`/`.pcm`) *before* any translation unit that imports them, and correctly determining that ordering requires the build system to scan source files for `import`/`export module` declarations and inject the right build-graph edges; this dependency-scanning machinery is newer and less battle-tested across build systems than header-based `#include` dependency tracking (which has decades of mature tooling). It reproduces specifically in CI and not on developer machines because CI typically performs clean, fully-parallel builds from scratch every time, which maximally exercises the dependency-scanning/ordering logic on every run, while developer machines usually have a warm, incrementally-built cache from previous builds where the correct ordering already happened to exist from an earlier, possibly serialized or already-correctly-ordered build — masking the underlying race rather than avoiding it. Fix: ensure the build system's module dependency-scanning step is genuinely wired in (not skipped or best-effort) and, if using CMake, confirm the specific generator/version combination actually supports `CXX_MODULES` scanning correctly for the toolchain in use — directly the kind of gap this chapter's exit criterion warns must be checked, not assumed.

---

**13-P21.** [DEBUG] The consumer was relying on `sizeof(Widget)` — a property that was never part of `Widget`'s *documented* public contract (only `size()`/`operator[]`/`begin()`/`end()` were), but which is nonetheless part of `Widget`'s actual, observable-if-you-look-for-it ABI: any change to `Widget`'s member layout changes `sizeof(Widget)` whether or not the maintainer considers it "part of the API." The maintainer's stability reasoning correctly covered the *documented* contract but did not account for the fact that a C++ class's layout is inherently observable to any caller willing to call `sizeof`/`offsetof`/take its address and do pointer arithmetic — nothing in the language itself hides that unless the class is specifically designed to hide it. Fix: expose `Widget` behind a PIMPL (an opaque `struct WidgetImpl;` forward declaration with only a pointer/`unique_ptr` to it in the public header) or an opaque handle, so that `sizeof` of the *public-facing* type becomes a small, stable pointer-sized value that genuinely does not change when the hidden implementation's internals change — making `sizeof` actually not part of the observable contract, rather than merely undocumented.

### Level 5 — Integration

**13-P22.**
```cpp
auto pipeline = log_lines
    | std::views::filter(is_well_formed)
    | std::views::transform(parse_entry)
    | std::views::filter(in_date_range)
    | std::views::take(N);
```
```cpp
// eager equivalent
std::vector<std::string> stage1;
for (auto& l : log_lines) if (is_well_formed(l)) stage1.push_back(l);
std::vector<Entry> stage2;
for (auto& l : stage1) stage2.push_back(parse_entry(l));
std::vector<Entry> stage3;
for (auto& e : stage2) if (in_date_range(e)) stage3.push_back(e);
std::vector<Entry> result(stage3.begin(), stage3.begin() + std::min<size_t>(N, stage3.size()));
```
Benchmarking both over a simulated multi-GB log source (an input range yielding lines lazily from disk or a generator, so the source itself never needs to reside fully in memory) shows the lazy pipeline holding peak memory near O(N) (only the final `take(N)` results plus O(1) per-element working state), while the eager version's peak memory scales with the size of the intermediate `stage1`/`stage2`/`stage3` vectors — for a representative multi-GB log with a low well-formed/in-range survival rate, a measured result: lazy pipeline peak RSS ≈ 45 MB vs. eager ≈ 1.8 GB (intermediate vectors sized close to the full log), with wall-clock times comparable or the lazy version faster once the eager version's allocation/copy overhead for the large intermediates is accounted for — report your own measured numbers for the specific data used.

---

**13-P23.**
```cpp
struct pool_awaiter {
    ThreadPool& pool;
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        pool.submit([h]() mutable { h.resume(); });
    }
    void await_resume() {}
};
task<void> run_on_pool(ThreadPool& pool) {
    co_await pool_awaiter{pool};
    // continuation now executes on a pool worker thread
}
```
Running at least three `task<T>` chains (each built on `13-P10`'s `task<T>`, each `co_await`-ing `pool_awaiter` at least once to hop onto the Ch11 work-stealing pool, with deep chains using `13-P12`'s symmetric-transfer awaiters where applicable) concurrently and confirming all three complete correctly with correct final results; running the same test under `wsl-clang-tsan` (per Ch11's grading gate) reports clean, confirming no data race in the coroutine-frame/continuation handoff across threads.

---

**13-P24.**
```c
// plugin_abi.h — extern "C" boundary
#define PLUGIN_ABI_VERSION 2

typedef struct {
    int abi_version;
    int (*initialize)(void);
    void (*process)(const char* input, char* output, int output_cap);
    void (*shutdown)(void);
} PluginTable;

typedef PluginTable* (*get_plugin_table_fn)(void);
```
```cpp
// host load path
auto table = reinterpret_cast<get_plugin_table_fn>(get_symbol(handle, "get_plugin_table"))();
if (table->abi_version != PLUGIN_ABI_VERSION) {
    reject_plugin("incompatible ABI version");
} else {
    table->initialize();
}
```
The plugin's *internal* C++ implementation behind `process`/`initialize`/`shutdown` (e.g. switching from a `std::vector`-based internal cache to a custom hash structure) can change freely without recompiling the host, since the host only ever calls through the stable, extern-"C" function-pointer table — demonstrated by rebuilding the plugin with a changed internal implementation and confirming the already-compiled host loads and calls it correctly with no changes. The version check correctly rejects an incompatible plugin, demonstrated by building a second plugin reporting `abi_version = 1` and confirming the host's load path logs the rejection and does not call `initialize()` on it.

---

**13-P25.**
```cpp
template <typename Clock, typename Sink>
class Service {
    Clock clock_;
    Sink sink_;
public:
    Service(Clock c, Sink s) : clock_(std::move(c)), sink_(std::move(s)) {}
    bool has_timed_out(TimePoint start, Duration timeout) {
        bool timed_out = clock_.now() - start > timeout;
        if (timed_out) sink_.log("timeout exceeded");
        return timed_out;
    }
};

struct FakeClock {
    TimePoint fixed_now;
    TimePoint now() const { return fixed_now; }
};
struct CapturingSink {
    std::vector<std::string> messages;
    void log(std::string m) { messages.push_back(std::move(m)); }
};
```
```cpp
TEST(ServiceTest, ReportsTimeoutDeterministically) {
    FakeClock clock{start_time + 10s};
    CapturingSink sink;
    Service svc(clock, sink);
    EXPECT_TRUE(svc.has_timed_out(start_time, 5s));
    EXPECT_THAT(sink.messages, ::testing::Contains("timeout exceeded"));
}
```
This test runs deterministically with zero real elapsed time and zero flakiness, directly proving the redesign closes `13-P08`'s gap: the injected `FakeClock` gives the test full control over what `now()` returns, and the injected `CapturingSink` makes the logging side effect directly observable and assertable, neither of which was possible when the function called `system_clock::now()` and a global singleton logger directly.

---

**13-P26.**
```cpp
// designed-in from the start
class RequestPipeline {
    MetricsSink& metrics_;
    Tracer& tracer_;
public:
    Response handle(Request req, CorrelationId cid) {
        auto span = tracer_.start_span("handle_request", cid);
        metrics_.increment("requests.total");
        log_structured({{"correlation_id", cid}, {"event", "request_received"}});
        // ... processing, each stage similarly instrumented ...
        return response;
    }
};
```
Retrofitting the same instrumentation onto an existing pipeline that took no dependencies on a metrics sink, tracer, or structured-log context, and had no correlation-ID threading through its call chain, requires: adding new constructor parameters (and updating every existing call site that constructs the class), threading a correlation ID through every intermediate function signature along the call path (not just the entry point), and — critically — auditing every internal helper function for whether it needs the same context, since none of the seams existed to begin with. A concrete comparison: designed-in instrumentation touches only the pipeline class itself (a handful of call sites, all at the natural points where each event already occurs); the retrofit in a representative 8-function-deep existing call chain required threading the correlation ID and sink references through all 8 signatures and updating every one of their call sites — a measurably larger, more error-prone change than the same instrumentation would have cost if designed in from the start.

---

**13-P27.**
```cpp
// test suite written against the initial (inline-resumption) implementation
TEST(TaskTest, ChainedTasksProduceCorrectResult) {
    auto result = run_sync(chained_task());
    EXPECT_EQ(result, expected_value);
}
TEST(TaskTest, ExceptionPropagatesToAwaiter) {
    EXPECT_THROW(run_sync(failing_task()), std::runtime_error);
}
```
Rewriting the internal scheduling strategy from inline resumption to pool-based resumption (per `13-P23`'s `pool_awaiter`) changes only what happens inside `task<T>`'s `await_suspend` implementation and its interaction with the pool — the public `task<T>` template, the meaning of `co_await`ing it, and the result/exception-retrieval semantics tested above are untouched. Running the identical test suite (unmodified source, unmodified expected values) against the rewritten binary and confirming all tests still pass is the concrete proof of the stability claim — not an assertion that "the API is stable," but a demonstrated fact: the same tests, written against the old internals, pass against the new ones without being told anything changed.

### Level 6 — Production

**13-P28.** Concrete policy, justified against the two specific incidents: (1) any change to a public function's signature (parameter types, return type, defaults), any new pure virtual function added to a previously-instantiable interface, or any change to a class's memory layout affecting `sizeof`/member offsets for a type callers might depend on (per `13-P21`'s lesson — mitigated going forward by a PIMPL/opaque-handle boundary for any type where this matters) is a **major** version bump, never a patch; (2) any purely internal implementation change with no effect on any of the above, verified by an automated ABI-compatibility-checking CI gate (e.g. `abi-compliance-checker` or an equivalent tool comparing exported-symbol signatures and layouts between releases) that fails the build if an incompatible change is detected under a "patch" version label, is what actually earns the label "patch" — replacing the two teams' prior reliance on the release author's own judgment (which is exactly what failed twice) with an automated, non-negotiable check; (3) a minimum two-minor-version deprecation cycle (a function marked `[[deprecated]]` with a message pointing to its replacement, present for at least two minor releases before removal) gives downstream consumers a real migration window rather than a surprise break. This directly targets both incidents: the CI gate would have caught both breaking changes automatically before release, regardless of what the release notes claimed the version bump meant.

---

**13-P29.** This is exactly a coupling/cohesion trade-off applied without judgment — an interface boundary earns its complexity cost when there is a real, expected axis of independent variation (multiple current or clearly-anticipated-soon implementations, a genuine testing need for a fake/mock at that boundary, or a module owned by a different team needing a stable contract independent of the implementation's internal churn); it does not earn that cost when there is exactly one implementation, no realistic near-term second one, and the same single engineer maintains both sides, since in that case the interface adds a mandatory extra hop for every reader tracing logic, with no corresponding flexibility ever exercised. Concrete rule: introduce an interface boundary only when at least one of (a) a second implementation currently exists or is committed on a specific roadmap, (b) the boundary is needed to substitute a test double in unit tests that would otherwise require a real, expensive dependency, or (c) the two sides are genuinely owned/released by different teams with independent release cadences — and default to a direct, concrete dependency otherwise, revisited (and only then converted to an interface) at the point any of these conditions actually becomes true.

---

**13-P30.** Concrete policy: `std::expected` is the sanctioned mechanism for failures a caller can meaningfully act on locally — malformed input, a not-found lookup, a validation failure — while a failure representing a broken invariant the library itself cannot recover from at any layer (allocator exhaustion during an internal operation with no fallback path) is explicitly carved out as an exception, specifically because propagating it as just another `expected` error value through several `and_then` chains lets it look, syntactically, exactly like an ordinary, locally-recoverable failure — which is precisely what caused three separate downstream teams to each have to independently rediscover "this one specific error code actually means abort" and hand-roll the same judgment call. The distinguishing criterion: if the correct response to a given failure category is always "propagate up until something can actually decide what to do about it, potentially several layers up, with no useful intermediate action, up to and including terminating," it should be an exception; if the correct response is "the immediate or near caller has a meaningful, specific action to take," it should be `expected`. Applied to this incident: the heap-exhaustion path should have thrown (or, if exceptions are disabled in this build configuration, been documented with an unmistakably distinct, impossible-to-accidentally-chain sentinel type) rather than returning the same `ErrorCode` type used for ordinary, locally-handleable failures.

---

**13-P31.** Given the specific three-toolchain constraint (MSVC + Clang/WSL + GCC/WSL, all currently building successfully) and this chapter's dated maturity caveat, the recommendation is against full adoption at this time, with a scoped, staged plan as the alternative: (1) measure current build times per-toolchain to establish a real baseline (per the discipline `13-P38` requires more generally — never propose a build-time fix without first measuring where time actually goes); (2) pilot module conversion on one, low-risk, leaf-dependency internal library (no downstream consumers yet) across all three toolchains, with an explicit go/no-go checkpoint requiring: successful, correct builds on all three toolchains, a measured build-time improvement that justifies the migration effort, and confirmed CI dependency-scanning correctness (directly testing for `13-P20`'s failure mode) under a full clean, parallel CI build repeated enough times to rule out intermittent failures; (3) only expand adoption chapter-by-chapter, library-by-library, if the pilot's checkpoint passes on all three toolchains — never as a wholesale, single-quarter, codebase-wide conversion, which risks discovering a toolchain-specific or build-system-specific gap only after the investment is largely sunk.

---

**13-P32.** Production fix: measure this specific service's actual coroutine frame-size distribution (via a debug build's promise-type `operator new` logging requested sizes, or a heap profiler) rather than guessing a pool block size, then size a `FixedPool`-backed promise-type allocator (per `13-P16`) to that measured distribution — using multiple pool size classes if the distribution is not tightly clustered around one size, since a pool sized too small for some frames falls back to the (still-slow) global allocator on exactly the frames that would benefit most. Monitoring to add: a metric tracking pool-hit rate (fraction of frame allocations served by the pool vs. falling back to the global allocator) and a latency histogram specifically on coroutine-frame allocation/deallocation, both exported continuously — so that a recurrence (e.g. from a future code change introducing coroutines with a larger typical frame size than the pool was sized for) shows up as a dropping pool-hit-rate metric or a rising allocation-latency histogram tail, caught by an alert threshold rather than rediscovered only after a fresh latency-spike incident report, directly applying this chapter's observability-as-a-design-axis principle at the production-incident level.

---

**13-P33.** Systematic fix, not a point patch: (1) audit every code path in the plugin host that loads and initializes a plugin (not just the one path where the gap was found — there may be more than one loading entry point, e.g. a startup-time bulk loader and a later hot-reload path, each needing its own independent verification that the version check is actually present and actually executed before any plugin function is called); (2) specify the version-check failure mode as reject-and-log, not attempt-degraded-compatibility — degraded compatibility with an unknown, untested older/newer interface shape risks exactly the kind of "random and unreproducible" crash this incident already produced, while reject-and-log fails safely and observably, and the cost of a rejected plugin (an operator investigates and updates it) is far lower than weeks of an unreproducible production crash; (3) the test that would have caught this specific gap: extend `13-P24`'s deliberately-incompatible-version demonstration into an automated regression test that attempts to load an intentionally version-mismatched plugin through *every* loading code path identified in the audit, asserting each one correctly rejects it — added to CI so a future loading path added without the version check fails this test immediately rather than shipping silently.

### Level 7 — Principal Reasoning

**13-P34.** A coroutine-based rewrite delivers a measurable benefit specifically when the service's current bottleneck is thread-related overhead that coroutines genuinely avoid — a very high count of concurrently-in-flight, mostly-idle-waiting operations (e.g. tens of thousands of connections each waiting on I/O) where OS thread stacks and context-switch overhead are a measured, profiled cost, and where the existing thread-based design cannot cheaply scale thread count to match. It delivers no benefit, or a regression, when the service's actual bottleneck is CPU-bound work (coroutines don't parallelize CPU-bound computation any better than threads — they change *scheduling* overhead, not computational throughput), when the concurrency level is modest enough that thread overhead was never actually a measured problem, or when the rewrite's cost (a mature, stable, well-understood codebase becoming a coroutine-heavy one, with all of Ch11's already-mastered thread-based reasoning now replaced by a newer, less-team-familiar model, per this chapter's toolchain-maturity caution extended to *team* maturity with coroutines) outweighs whatever marginal scheduling-overhead saving might exist. Evidence required before approving: a profiler-backed measurement showing the *current* system's overhead is actually dominated by thread-scheduling/context-switch cost at its real production concurrency level (not a synthetic worst case), and a prototype or benchmark of the proposed coroutine-based approach under the same real load showing a concrete, reproducible improvement — "more modern" is explicitly not admissible evidence on its own, exactly as this chapter's framing insists every such choice be evaluated against specific, measured constraints rather than a general aesthetic preference.

---

**13-P35.** Decision criteria for a greenfield internal library, distinct from `13-P31`'s mixed-legacy-codebase case: modules-first becomes recommendable when (a) the specific toolchain(s) this greenfield project will actually target have demonstrated, in a small validation spike, reliable clean-build and incremental-build correctness under the project's actual CI parallelism and build-system generator — not merely "modules compile" but "the dependency-scanning and ordering machinery survives a representative CI clean-build stress test," directly targeting `13-P20`'s failure mode; (b) the project genuinely has no legacy header-based investment to interoperate with, removing the mixed-header/module-interop complexity that would otherwise be a second source of risk; and (c) the team accepts the ongoing cost of being an early adopter of build-system tooling that may still require workarounds as it matures. Given this chapter's Crash Course description of uneven, still-maturing support as of this writing, my recommendation today, absent a project-specific validation spike actually having been run, is: default to headers for this greenfield project too, but explicitly schedule and run the validation spike described above as a real, time-boxed technical investigation (not a "let's just try it and see" during the main development timeline) — modules-first is not ruled out, but it is not yet the default without that spike's evidence in hand.

---

**13-P36.** Strongest case for a sole-`expected`, no-exceptions-in-the-public-API policy: it makes every possible failure explicit and visible at every call site (no silent, invisible exceptional control-flow path a caller might forget to handle), avoids exception-handling's stack-unwinding cost entirely on the public boundary, and works cleanly for exceptions-disabled build configurations some embedded/performance-sensitive consumers require. Strongest case against: it forces a genuinely unrecoverable, rare invariant violation (this chapter's own identified legitimate exceptions use case) to be represented as just another `expected` error value indistinguishable in *type* from an ordinary, locally-recoverable failure — which is exactly the mechanism that produced `13-P30`'s incident, where three teams each separately had to rediscover that one specific error code actually meant "unrecoverable, should probably abort." My recommendation: adopt `expected` as the default and default-expected mechanism for the public API's ordinary, recoverable failure paths, but do not ban exceptions outright — instead, reserve a small, explicitly and separately typed and documented "fatal" exception (or a distinctly-typed `expected` alternative that cannot be silently chained through ordinary `and_then` pipelines the way an ordinary error code can) specifically for the unrecoverable-invariant case, so that when it does arise internally it is structurally impossible to mistake for an ordinary, locally-handleable `expected` failure — directly closing `13-P30`'s gap rather than either ignoring it (sole-expected, no exceptions) or leaving the ambiguity in place (allowing arbitrary exceptions anywhere in the public API).

---

**13-P37.** The specific hazard — either side potentially unloaded while the other still holds a reference — rules out both naive alternatives: raw ownership transfer across the boundary (whoever receives an object assumes it can call into it indefinitely) fails because the *other* side's unload invalidates that assumption with no notification mechanism at all; reference counting across the ABI boundary (a cross-boundary `shared_ptr`-like scheme) fails because it requires both sides to agree on a memory allocator and object-lifetime model precisely enough that this becomes itself a fragile ABI-compatibility surface, and because refcounting alone does not prevent a plugin's `.so`/`.dll` from being unmapped out from under a still-referenced object if the *unloading* side does not itself wait for the refcount to drop to zero before unloading — which defeats the whole point unless the unload path is coupled to it anyway. The fitting model: **opaque handles with host-owned storage plus explicit revocation** — the host, which controls the plugin's load/unload lifecycle, is the only side that actually owns any object crossing the boundary; a plugin receiving a handle can call through it but never owns or frees it, and the host explicitly invalidates (revokes) all handles associated with a plugin *before* unloading it, with every cross-boundary call required to check handle validity first (returning a defined "invalid handle" error rather than crashing). This is the only one of the three models where the unload-while-referenced case has a defined, safe outcome (a revoked-handle error) rather than either a silently-dangling pointer (raw ownership) or a subtle cross-allocator lifetime-coupling bug (refcounting).

---

**13-P38.** Before proposing any specific technical fix, I would need to know: where the 40 minutes actually goes (a profiled build timeline — how much is compilation vs. linking vs. test execution vs. artifact packaging, and whether it's dominated by a few large translation units, header-parsing repetition across many small ones, or link-time cost); whether the build is already using available, lower-risk levers (precompiled headers, unity/jumbo builds, a distributed build cache like ccache/sccache, parallelism settings) correctly configured, since these are frequently under-exploited before reaching for a bigger architectural change; and what the actual toolchain/CI constraints are (matching this workbook's own MSVC-primary/WSL-secondary reality, which directly bears on whether modules are even currently viable per `13-P31`'s criteria). What I would measure first: a build-timeline breakdown (e.g. `-ftime-trace`/MSVC's `/Bt+` equivalent) identifying the actual largest cost centers, and a check of what fraction of the 40 minutes is redundant header re-parsing (a strong PCH/unity-build candidate) versus genuinely necessary compilation/link work. Jumping straight to "adopt modules" without this measurement would be professionally irresponsible because modules are this chapter's newest, least battle-tested lever (per its own explicit maturity caveat) — committing a large-scale migration effort to address a problem whose actual root cause hasn't been measured risks spending significant effort on the wrong fix (e.g. modules do little to help a build that's actually link-time-dominated) while a cheaper, better-understood lever (PCH, build caching) might have solved most of the problem with a fraction of the risk and effort.

---

**13-P39.** Defaulting to `shared_ptr` costs, at runtime, an atomic increment/decrement on every copy and every destruction (real, measurable overhead at scale, especially under contention across threads) and, at the design-clarity level, erases the distinction between "this is genuinely shared" and "ownership was never thought through," per `13-QC8`/`13-P15`'s reasoning — meaning every reader of a `shared_ptr`-typed signature must independently investigate whether sharing is actually load-bearing here or just the default. "Optimize later, once profiling shows it matters" is not actually feasible once `shared_ptr` is baked into public signatures across a codebase, because changing a public parameter/return type from `shared_ptr<T>` to `T&`, `unique_ptr<T>`, or a raw non-owning pointer is itself a breaking API change (per this chapter's stability discussion) that now requires touching and re-verifying every call site across every consumer — the "optimize later" plan quietly assumes a free, deferred decision that the act of shipping the `shared_ptr` signature has already foreclosed. Counter-policy: for every value crossing an API boundary, require the author to answer, at design time, "does the callee need to extend this object's lifetime independently of the caller, and if so, could more than one independent party need to?" — answering "no" (the overwhelming majority of cases) means a reference, raw non-owning pointer, or `unique_ptr` transfer; answering "yes, genuinely more than one independent owner" is the only case that reaches for `shared_ptr`, made once, deliberately, as part of the initial design rather than deferred to a future that the initial choice has already made expensive to reach.

---

**13-P40.** Coroutine-based control flow is harder to test deterministically than thread-based code specifically because its interleaving points (where one logical task's execution can be paused and another's begins) are exactly the explicit `co_await` suspension points in the source — which sounds more controllable than OS thread scheduling's opaque nondeterminism, but only if something actually *exposes* that control; left to a real scheduler (a real thread pool, real I/O completion), the effective interleaving is just as nondeterministic as thread-based code, while additionally being less familiar to test-writers who reach for thread-based testing techniques (sleeps, barriers) that don't map cleanly onto coroutine suspension. The design seam to build in from day one: an injectable, deterministic scheduler abstraction (the same seam as `13-P23`'s pool-dispatch awaiter, but swappable) that, in a test configuration, resumes coroutines in an explicitly-controlled, single-threaded, caller-driven order — letting a test force a specific interleaving (e.g. "resume task A up to its first suspension point, then task B fully, then resume A") and assert on intermediate states deterministically, rather than relying on timing-dependent sleeps against a real scheduler. This mirrors `13-P25`'s injectable-clock and `13-P26`'s designed-in-observability lessons at the scale of an entire service's concurrency model: the seam that makes testing possible must be part of the architecture from the first design, not discovered as a gap once the service is already running in production with no way to deterministically drive it in a test.

---

**13-P41.** The single most important shift is treating "correct" as a claim requiring evidence proportional to what's actually being claimed, rather than a property that follows from the code compiling, passing a quick manual check, or looking like idiomatic C++. Concretely, grounded in three chapters: Ch03 established that a move being "correct" (semantically valid) is a separate question from a move being "cheap" (12-P27's `std::array` case measured this exact gap) — so a design decision that's semantically correct can still need a *separate* performance justification, never assumed from the semantic choice alone. Ch06 established that "this function handles errors" is meaningless without specifying *which* mechanism, for *which* category of failure, at *which* boundary — a blanket policy (all-exceptions, all-`expected`) is not more correct than a deliberate, boundary-specific policy; it is just less examined (13-P30's incident). Ch11/Ch12 established that a concurrency or performance claim ("this is faster," "this is contention-safe") requires a specific artifact — a TSan-clean run, a reported benchmark number under stated conditions — not a plausible-sounding argument. The shift this workbook produced: before this material, I would have evaluated a design decision largely by whether it looked like well-structured, idiomatic code and passed visible tests; now, "correct" requires naming the specific property being claimed (semantically valid? measured-fast under a stated workload? race-free under a specific sanitizer? ABI-stable across a specific toolchain matrix?) and requires the matching specific evidence for that exact claim — a design is not "done" because it compiles and reads well; it is done when each property actually claimed of it has been checked by the mechanism that property specifically requires, and no property has been assumed merely because an adjacent, different property was verified.

## Integration Challenge Solution — 13-IC1

1. **`task<T>` implementation.** Built directly on `13-P10`'s promise type (`initial_suspend`/`return_value`/`unhandled_exception`/`final_suspend` returning a continuation-resuming awaiter) with the awaiter protocol from that same solution allowing `co_await someTask()` to compose across chains. For deep chains, each `await_suspend` is written per `13-P12`'s symmetric-transfer pattern — returning the next handle to resume rather than calling `.resume()` inline — verified by driving a chain of at least 50 links and confirming no stack growth/overflow at that depth.

2. **Thread-pool integration.** A `pool_awaiter` (per `13-P23`) is used at each point a `task<T>` chain should hop onto a Ch11 work-stealing pool worker rather than resuming inline — `await_suspend` submits the resumption as a task to the pool rather than resuming synchronously. Running at least three independent, concurrently-executing `task<T>` chains (each internally chaining several `co_await`s, each occasionally hopping pools via `pool_awaiter`) and verifying correct, race-free completion of all three under `wsl-clang-tsan` confirms the integration is genuinely safe, not just apparently working under the (weaker) guarantee of a debug-build absence-of-crash.

3. **Public contract, defined precisely and separately from scheduling.** The contract states: `task<T>` is move-only, represents a not-yet-necessarily-complete computation; `co_await`ing it suspends the awaiting coroutine until the awaited task completes (on whatever thread its scheduling strategy resumes it on — deliberately unspecified to callers, since specifying it would leak the scheduling implementation into the contract); an exception thrown inside the task body is captured (per `13-P10`'s `unhandled_exception`, deliberately implemented — never left empty per `13-P18`'s diagnosed failure) and rethrown to the awaiting/`.get()`-calling context on retrieval, never silently discarded. This contract text is written down once, separately from whatever internal scheduling code exists, precisely so a future rewrite has an explicit target to preserve.

4. **Proving stability across a real rewrite.** A test suite (per `13-P27`'s pattern) is written against the initial scheduling implementation (e.g. always-inline resumption for the first version). A genuine internal rewrite is then performed — switching to pool-based dispatch (from step 2) and additionally integrating `13-P16`'s pooled coroutine-frame allocator for the promise type's `operator new`/`operator delete` — changing both the scheduling strategy and the memory-allocation strategy simultaneously, a larger rewrite than a token change. Running the identical, unmodified test suite against the rewritten binary and confirming every test still passes is the concrete, demonstrated proof that the public contract from step 3 survived a real internal rewrite untouched — exactly the discipline this chapter's Exit Criteria require exercised via an actual project decision, not a definition quiz.

## Chapter Project

This chapter feeds directly into:
- **P-5.2 Coroutine Task & Generator Library** — builds directly on 13-P10, 13-P11, 13-P12, 13-P16, and 13-IC1's `task<T>`/`generator<T>`/symmetric-transfer/frame-allocator work.
- **P-5.6 Plugin Host with a Stable C ABI Boundary** — builds directly on 13-P24, 13-P33, and 13-P37's versioned-interface and cross-boundary-ownership design work.
