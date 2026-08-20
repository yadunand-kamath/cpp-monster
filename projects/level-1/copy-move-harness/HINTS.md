# P-1.4 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch03's Crash Course on value categories and move semantics before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

You need a type whose special member functions do nothing functionally interesting except record that they were called. Think about the minimum set of operations the standard actually distinguishes — default construction, construction from a value, copy construction, move construction, copy assignment, move assignment, destruction — and make sure your counters distinguish all of them individually; conflating any two would defeat the entire purpose of the harness.

## Hint 2 — Technique

For observing the *mandatory* copy-elision case (a prvalue returned directly, e.g. `return Tracked{...};`) versus the *NRVO* case (a named local returned, e.g. `Tracked t{...}; return t;`), make sure you're clear on which of these two is a C++17 language guarantee and which remains a compiler optimization the standard merely permits. Getting this distinction backwards is the single most common conceptual error this project is designed to catch — write down, before you write any code, which case you expect to be guaranteed identical at every optimization level and which you expect might legitimately vary.

## Hint 3 — Implementation

To force NRVO to fail in a way you can observe, think about what a single-return, single-local function has that a function returning different named locals from different branches lacks: the compiler can only construct the return value directly in the caller's storage if it can determine, at the point each local is declared, that this specific object is unambiguously "the" object that will be returned. A function with two differently-named locals, each returned from a different branch, denies the compiler that certainty in every conforming implementation — that's the case you want for requirement 3(b)'s "cannot rely on it happening" demonstration. For the `move_if_noexcept` demonstration, build two near-identical types differing in exactly one place — the `noexcept` specifier on the move constructor — and observe them inside a `std::vector` operation that must reallocate.

## Hint 4 — Debugging/Design

If your elision/NRVO counts look identical between `-O0`/`/Od` and `-O2`/`/O2` when you expected a difference, double-check that the case you built is actually NRVO-dependent and not accidentally the *mandatory* elision case — a bare prvalue return must produce identical, minimal counts at every optimization level by the language rules themselves, so if you're trying to demonstrate optimization-dependent behavior and it refuses to show up, the fix is a genuinely different function shape (like the multi-named-local-return case), not a different compiler flag or a "maybe my compiler just doesn't do NRVO here" guess.
