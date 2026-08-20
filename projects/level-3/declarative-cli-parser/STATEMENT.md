# P-3.5 — Declarative Command-Line Parser

**Level:** 3 (Realistic utility) · **Category:** Libraries · **Requires:** Ch01–06 · **Est. effort:** M (10-16h)

## Objective

Build a command-line argument parsing library where a consumer declares their program's flags, options, and subcommands via a compile-time-checked specification (not string-parsing configuration at runtime), and receives generated `--help` text and clear parse errors for free.

## Functional Requirements

1. Support declaring flags (boolean, presence-only), options (taking a typed value: string/int/double/etc.), and positional arguments, each with a name, optional short form, description, and optional default value.
2. Support subcommands (e.g. `tool build --release`, `tool test --filter=foo`), each with its own independent set of flags/options, dispatched based on the first positional token.
3. Generate correct, readable `--help` output automatically from the declared specification — the help text must never be hand-written separately from the spec (a single source of truth, so they cannot drift out of sync).
4. Type-check option values at parse time, converting to the declared type (int, double, string, bool, or an enum-like restricted string set) and reporting a clear error for a value that doesn't convert.
5. Report clear, specific parse errors for: unknown flag/option, missing required option, wrong-type value, missing required positional argument — each distinguishable from the others, not one generic "invalid arguments" message.
6. The specification itself should be declared primarily at compile time (e.g. via a builder pattern evaluated in a `constexpr`-friendly way, or via template-based declaration) such that a name typo or type mismatch in the specification is caught as early as reasonably possible — document exactly how early ("compile time" vs "program-startup time" are both legitimate, differently-scoped answers; pick one and justify it).

## Input

`argc`/`argv` (or an equivalent `std::span<const char*>`/`std::vector<std::string_view>`), plus the compile-time-declared specification.

## Output

Either a successfully parsed, typed result object the caller can query, or a clear parse error (and/or generated help text) reported without crashing.

## Constraints

- C++20. No runtime string-based specification format (no parsing a JSON/YAML spec file to determine what flags exist) — the specification is part of the program's own compiled code.
- Must correctly reject an ambiguous or malformed specification itself (e.g. two options declared with the same name) — ideally at compile time, but a clear program-startup assertion is acceptable if compile-time rejection proves impractical; document which you chose.
- Must not use exceptions as the *only* way to report a parse error if the project's error-handling philosophy (per Ch06 and [P-2.4](../../level-2/result-error-propagation/STATEMENT.md)) favors a `Result`-style return — document your choice and be consistent with it.

## Edge Cases

- An option value that looks like a flag (e.g. `--count=--5` or a positional argument value starting with `-`) — must be parsed as the intended value, not misinterpreted as another flag.
- A subcommand-specific option that collides in name with a global option — document whether this is rejected at spec-declaration time or resolved by scope (subcommand-local overrides global).
- `--` as an explicit "everything after this is positional, not options" separator (a common CLI convention) — decide whether to support it and document the decision.
- Repeated options (the same flag passed twice) — decide and document whether this is an error, or the last one wins, or values accumulate into a list.

## Error Handling

- Every parse failure category from Functional Requirement 5 must be independently testable and distinguishable in the reported error.
- A malformed specification (caught at whichever time you documented) must produce a clear message identifying which part of the spec is invalid, not a generic assertion failure.

## Acceptance Criteria

- A representative multi-subcommand tool (e.g. mimicking `git`-style `tool <subcommand> [options]`) is fully specified and correctly parses a range of valid inputs, correctly rejects a range of invalid inputs with distinguishable errors, and produces `--help` output that a human reviewer would judge as clear and complete relative to the spec.
- The generated help text is demonstrated to update automatically when the specification changes (e.g. add a new option, rerun, see it appear in help with no separate help-text edit).
- The `--` separator and repeated-option policies (whichever chosen) are demonstrated explicitly.

## Testing Requirements

- Successful parse tests for flags, typed options, positionals, and subcommands, including edge-case-shaped values (looks-like-a-flag values, negative numbers).
- One test per distinguishable error category from Functional Requirement 5.
- The generated-help-matches-current-spec demonstration (can be a snapshot-style test: help text captured and compared against an expected string, which must be updated if the spec changes — proving the coupling is real).
- The malformed-specification-is-rejected test, exercised at whichever time (compile-time via `static_assert`/SFINAE-triggered error, or startup-time via a thrown/returned error) the design chose.

## Hints

### Hint 1 — Direction
The core design tension in this project is "runtime flexibility" (users pass arbitrary strings on the command line, which must be parsed at runtime) versus "compile-time correctness" (the specification of what's valid should be checked as early as possible) — resolve this by keeping the *specification* a compile-time-declared, statically-typed structure (a builder chain or a template-parameterized description), while the actual *parsing* of `argv` naturally remains a runtime operation against that fixed specification.

### Hint 2 — Technique
A builder pattern (`Parser{}.flag("verbose", 'v', "enable verbose output").option<int>("count", "c", "how many", /*default=*/1)`) that accumulates its declarations into a `constexpr`-friendly or at-least-startup-time-fixed data structure is a practical middle ground — full `constexpr` evaluation of a builder chain into a `static_assert`-checkable structure is possible in C++20 but nontrivial; a startup-time-validated structure (checked once, early, with a clear assertion/error) is a legitimate, simpler alternative — pick one and be honest in your documentation about which correctness guarantee you actually achieved.

### Hint 3 — Implementation
For generating `--help` from the same specification used to parse, make sure your specification's internal representation (whatever list/table of flag-name/type/description entries you build) is the *only* thing both the parser and the help-generator read from — if you ever find yourself writing a help string that isn't derived by iterating that structure, you've broken the single-source-of-truth requirement.

### Hint 4 — Debugging/Design
If a value like `-5` (a negative number) or `--file=--verbose` (a value that happens to start with `--`) gets misparsed as a flag rather than as the intended value, check whether your tokenizer decides "is this a flag" purely by checking if a token starts with `-`/`--`, versus correctly using the surrounding context (an option that's known to expect a value should consume the *next* token as that value unconditionally, without re-examining whether that next token also happens to look flag-shaped).
