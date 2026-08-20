# P-1.4 — Copy/Move Instrumentation Harness

**Level:** 1 (Focused component) · **Category:** Performance · **Requires:** Ch01–03 · **Est. effort:** S (4-8h)

## Objective

Build a small instrumented type — one that counts every construction, copy, move, and destruction it participates in — and use it to empirically prove or disprove, with actual numbers rather than assumptions, exactly when the compiler elides copies/moves, when NRVO applies, and when `std::move_if_noexcept`-driven container operations choose to copy instead of move.

## Functional Requirements

1. `Tracked` (or your chosen name) records, via counters accessible after the fact, at minimum: default constructions, value constructions, copy constructions, move constructions, copy assignments, move assignments, and destructions — each incremented at exactly the right point, distinguishable from each other.
2. The counters must be resettable between test cases (a global reset function, or counters scoped per test in a way that doesn't leak state between tests).
3. Using `Tracked`, produce and document a concrete, numbered demonstration of: (a) a case where copy elision applies to a returned local (and what the *pre-C++17-guaranteed* vs *C++17-mandatory* distinction actually changes about your counts), (b) a case where NRVO applies to a named local, and a case constructed so that NRVO's non-mandatory nature under the standard is actually observable — i.e., a case where you cannot rely on it happening, (c) at least one case demonstrating `std::move_if_noexcept` choosing to copy specifically because `Tracked`'s move constructor is not marked `noexcept` — and a second case, with a `noexcept`-move variant of the type, showing the same operation choosing to move instead.
4. All findings are captured in a written report tying each demonstration back to the exact standard/compiler rule responsible — this project's deliverable is as much the report as the harness code.

## Input

None — this is a self-contained instrumented library plus a set of demonstration programs, run and their output captured for the report.

## Output

Console output showing counters at each stage of each demonstration, referenced by your report; the report itself documents the numbers alongside the explanation.

## Constraints

- C++20, compiled and run under at least two different optimization levels (e.g. `/Od` and `/O2` on MSVC, or `-O0` and `-O2` on WSL Clang/GCC) — because some of what you're trying to observe (mandatory copy elision under C++17 rules) is *not* an optimization at all and must be present at every level, while NRVO genuinely is a compiler choice that can differ between optimization levels or between compilers entirely.
- Do not use a debugger or third-party profiler for this — the counters themselves are the instrument. This project is specifically about building the observation tool, not using an existing one.
- Your `Tracked` type must have observably different behavior (different resulting counts) depending on whether its move constructor is marked `noexcept`, since that's one of the three demonstrations required.

## Edge Cases

- A function returning a `Tracked` by value where the returned expression is a function parameter passed by value (not a purely local variable) — does copy elision still apply here, or does this specific shape force a move/copy that a naively-local return wouldn't?
- Two different named locals returned from two different `return` statements in the same function (defeating NRVO in every implementation that requires a single, unambiguous local to elide) — construct this case and show the counts differ from the single-named-local case.
- Comparing what happens when a `Tracked` is pushed into a `std::vector<Tracked>` that must reallocate, once with a `noexcept` move constructor and once without — this is the concrete container-level consequence of the `move_if_noexcept` demonstration required above.

## Error Handling

Not applicable in the traditional sense — there's no failure mode for the harness itself beyond a demonstration program failing to compile or producing counts that contradict your own documented explanation (which is itself a finding worth documenting, if your mental model turns out to be wrong and the compiler's actual behavior teaches you something).

## Acceptance Criteria

- All required demonstrations (elision, NRVO presence and absence, `move_if_noexcept` both ways) build and run at both optimization levels tested, with output captured.
- A written report (Markdown is fine) presenting each demonstration's counts and tying them to the specific rule responsible, written so that someone who has read Ch03's Crash Course but hasn't done this project could follow your reasoning.
- Builds cleanly under `/W4 /permissive-`.

## Testing Requirements

- Each demonstration should be its own small, independently runnable program or clearly separated test case — not one large program whose output is hard to attribute to a specific cause.
- At least one demonstration must be run at two different optimization levels with genuinely different results, specifically to show that NRVO is a compiler *choice*, not a guarantee — if your chosen demonstration happens to produce identical results at both levels on your compiler, you have not yet found a case that actually depends on NRVO; keep looking.

## Hints

### Hint 1 — Direction
You need a type whose special member functions do nothing functionally interesting except record that they were called — think about the minimum set of operations the standard actually distinguishes (default construction, construction from a value, copy, move, each kind of assignment, destruction) and make sure your counters distinguish all of them, since conflating "copy constructor called" with "move constructor called" would defeat the entire purpose of the harness.

### Hint 2 — Technique
For observing the *mandatory* copy-elision case (a prvalue returned directly, `return Tracked{...};`), versus the *NRVO* case (a named local returned, `Tracked t{...}; return t;`), make sure you understand which of these two the standard actually guarantees as of C++17 and which remains a compiler optimization the standard merely permits — this distinction is the entire point of requirement 3(a) and 3(b), and getting them confused will make your report's explanation subtly wrong even if your counts happen to look right.

### Hint 3 — Implementation
To force NRVO to fail in a way you can observe, think about what property a single-return, single-local function has that a function with multiple named locals returned from different branches lacks — specifically, what does the compiler need to know statically about *which* object is "the" one being returned in order to construct it directly in the caller's storage in the first place? For the `move_if_noexcept` demonstration, you'll need two versions of essentially the same type differing in exactly one place (the `noexcept` specifier on the move constructor) — put them in a container operation that must reallocate (like repeated `push_back` past capacity) and count copies vs. moves for each.

### Hint 4 — Debugging/Design
If your elision/NRVO counts look identical between `-O0` and `-O2` when you expected a difference, double check that the case you built is actually NRVO-dependent and not accidentally the *mandatory* elision case (a bare prvalue return) — mandatory elision must produce identical, minimal counts at every optimization level by the language rules, so if you're trying to demonstrate optimization-dependent behavior and it isn't showing up, you likely need a genuinely different function shape (e.g., the multi-named-local-return case), not a different compiler flag.
