# P-1.4 — Solution

## Reference Architecture

A single instrumented type with static counters, each incremented from exactly one special member function:

```cpp
struct Tracked {
    int value = 0;

    inline static int default_ctors = 0, value_ctors = 0, copy_ctors = 0,
                       move_ctors = 0, copy_assigns = 0, move_assigns = 0,
                       dtors = 0;

    Tracked() { ++default_ctors; }
    explicit Tracked(int v) : value(v) { ++value_ctors; }
    Tracked(const Tracked& o) : value(o.value) { ++copy_ctors; }
    Tracked(Tracked&& o) noexcept(false) : value(o.value) { ++move_ctors; } // deliberately NOT noexcept
    Tracked& operator=(const Tracked& o) { value = o.value; ++copy_assigns; return *this; }
    Tracked& operator=(Tracked&& o) noexcept(false) { value = o.value; ++move_assigns; return *this; }
    ~Tracked() { ++dtors; }

    static void reset_counts() {
        default_ctors = value_ctors = copy_ctors = move_ctors =
            copy_assigns = move_assigns = dtors = 0;
    }
};

struct TrackedNoexceptMove : Tracked {
    TrackedNoexceptMove() = default;
    TrackedNoexceptMove(TrackedNoexceptMove&&) noexcept { /* ... */ }
    // second type exists solely so its move ctor is noexcept while Tracked's is not
};
```

The three required demonstrations as functions:

```cpp
Tracked make_prvalue() { return Tracked{1}; }              // mandatory elision (C++17)

Tracked make_named_local() {
    Tracked local{2};
    return local;                                          // NRVO-eligible, not guaranteed
}

Tracked make_multi_branch_local(bool flag) {
    Tracked a{3};
    Tracked b{4};
    if (flag) return a;                                    // two distinct named locals —
    return b;                                               // no conforming implementation can elide this
}
```

## Design Rationale

**Why must the move constructor be non-`noexcept` on `Tracked` but `noexcept` on a second type?** This is the entire mechanism behind the `move_if_noexcept` demonstration: `std::vector`'s growth path uses `std::move_if_noexcept` on each element, which yields an rvalue (enabling move) only if the type's move constructor is `noexcept`, and otherwise yields a const lvalue (forcing copy) — specifically so that a reallocation can't be left in a half-moved, exception-corrupted state for types whose move might throw. Two structurally identical types differing only in this one specifier is the cleanest way to isolate that single variable and observe its consequence directly, rather than trying to infer it from a single type's ambiguous behavior.

**Why does `make_multi_branch_local` reliably defeat NRVO in every implementation, while `make_named_local` doesn't reliably enable it in every implementation?** NRVO requires the compiler to identify, at the point a local is declared, a *single* unambiguous object that will occupy the return slot, so it can construct that object directly in the caller-provided storage rather than in its own stack frame and then copy/move it out. With two different named locals returned from two different branches, there is no such single object identifiable at declaration time in any conforming implementation — the standard doesn't even permit NRVO here, so this case is guaranteed to produce a real copy or move on every compiler. `make_named_local`, in contrast, has exactly the shape NRVO is *allowed* to optimize, but the standard never requires it to — so this case's actual result is compiler- and optimization-level-dependent, which is precisely the point being demonstrated: your report needs a case in each category, not just one representative case.

**Why compile and run at two optimization levels?** Mandatory elision (the `make_prvalue` case) must produce identical counts at `/Od` and `/O2` because it's a language rule, not an optimization — observing this invariance across levels is itself evidence supporting the "guarantee" classification. NRVO, by contrast, is exactly the kind of thing that can differ across optimization levels since it's classified by the standard as an optimization the implementation is merely permitted to perform — some compilers apply it even at `-O0`, others only at higher levels. Running both levels and comparing is what turns "I read that NRVO isn't guaranteed" into an actual observed data point.

## Reference Implementation

The snippets above are representative of the whole harness; the remaining work is largely the report itself:
1. Wire each demonstration function into a small runner that resets counters, calls the function, and prints the resulting counts.
2. Compile and run the full set of demonstrations at both required optimization levels, capturing the raw output.
3. Write the report tying each observed count back to the specific rule (mandatory elision under `[class.copy.elision]`, NRVO as permitted-not-required, `move_if_noexcept`'s check) — this write-up is graded as carefully as the code, per this project's Acceptance Criteria.

## Testing Strategy

Treat each demonstration as its own minimal, independently-runnable unit so a reader (or grader) can attribute a specific count to a specific cause without disentangling it from unrelated activity in the same program. For the `move_if_noexcept` cases specifically, `reserve()` to a known capacity before the counted `emplace_back()` so the reallocation you're observing happens on a single, predictable call rather than an arbitrary one buried inside a longer sequence.

## Performance Analysis

Not the focus of this project in the traditional sense — the "performance" being analyzed here is qualitative (which operation classes are avoided or incurred), not quantitative timing. The instrumentation itself (incrementing a static counter per call) has negligible overhead and does not materially distort the counts being measured, though a genuinely timing-sensitive follow-up project would need to account for the counters' own cost.

## Failure Modes

- Building the `move_if_noexcept` demonstration with only one `Tracked` type and trying to toggle its `noexcept`-ness "in your head" between runs rather than having two concrete types side by side — this makes the demonstration much weaker as evidence, since nothing in the actual compiled artifact proves the claimed cause.
- Mistaking the mandatory-elision case for the NRVO case (or vice versa) in the report's explanation — the code producing correct counts does not guarantee the report correctly attributes *why*, and that attribution is the actual point of the exercise.

## Extensions

- Extend the harness to also count "elided entirely" as its own bucket by comparing total constructions against total destructions at each site, cross-checking your explicit counters against that derived invariant.
- Add a fourth demonstration exploring what happens when a `Tracked` is returned from a function but immediately used only to initialize a `const Tracked&` reference rather than a new object — a genuinely different, less-obvious value-category interaction worth exploring once the required three are solid.
