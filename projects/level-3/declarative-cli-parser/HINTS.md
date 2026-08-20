# P-3.5 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch05's template/generic-programming material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Separate this project cleanly into two distinct concerns that happen to share data: a *specification* (what flags/options/subcommands exist, their types, defaults, and descriptions — built once, before any `argv` is examined) and a *parser* (which walks `argv` token by token against that fixed specification, producing either a typed result or an error). Almost every design difficulty in this project comes from conflating these two — keep the specification's data structure the single thing both parsing and help-generation read from, and the rest follows naturally.

## Hint 2 — Technique

A builder-pattern approach — each call like `.flag(...)` or `.option<T>(...)` returns a new (or mutated, if you're comfortable with a mutable builder) `Parser` object with the declaration appended to an internal list — gives you a natural fluent declaration syntax without needing a separate "spec description language." Decide early whether that internal list is fully `constexpr`-buildable (ambitious, requires careful use of `constexpr` containers or fixed-size `std::array`-based storage) or simply built once at program startup and validated then (much simpler, and a legitimate documented choice per the Constraints section).

## Hint 3 — Implementation

For type-checking a string value against a declared type `T`, a small set of `parse_value<T>(std::string_view) -> Result<T, ParseError>` overloads (or one templated function with `if constexpr` branches per supported type) centralizes exactly where "wrong type" errors get generated, keeping that logic out of the main token-walking loop. For generating help text, iterate your specification's declaration list once, formatting each entry's name/short-form/type/description/default consistently — resist the temptation to special-case any one flag's help line by hand.

## Hint 4 — Debugging/Design

If a negative-number value or a value that happens to start with `--` gets misinterpreted as another flag, the bug is almost always in the tokenizer deciding "is the *next* token a flag or a value" by re-inspecting that next token's shape, rather than by remembering that the *previous* token was an option requiring a value and therefore unconditionally consuming whatever comes next as that value. The token immediately following a value-taking option should never be reinterpreted as anything other than that option's value.
