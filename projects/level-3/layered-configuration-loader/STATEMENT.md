# P-3.6 — Layered Configuration Loader

**Level:** 3 (Realistic utility) · **Category:** Libraries · **Requires:** Ch01–06,09 · **Est. effort:** M (10-16h)

## Objective

Build a configuration system that merges settings from multiple layered sources — compiled-in defaults, a config file, environment variables, and command-line overrides (via [P-3.5](../declarative-cli-parser/STATEMENT.md), reused rather than rebuilt) — with a well-defined precedence order, schema validation, and optional hot-reload when the underlying file changes.

## Functional Requirements

1. Support at least four layered sources with a fixed, documented precedence order (lowest to highest: compiled-in defaults → config file → environment variables → CLI arguments), where a higher-precedence source's explicitly-set value always overrides a lower one's, but an *unset* value at any layer falls through to the next lower layer.
2. Validate the merged configuration against a declared schema (types, required fields, allowed ranges/enum values for specific keys) and report all validation errors found, not just the first one encountered.
3. Support hot-reload: when the config file changes on disk (using [P-3.2](../file-watcher/STATEMENT.md)'s file-watching mechanism, reused rather than rebuilt), the loader re-reads and re-validates it, and notifies subscribers of which specific keys changed value as a result — not just "something changed."
4. Provide type-safe access to configuration values (no stringly-typed lookups returning `std::string` that the caller must parse themselves for every read).
5. Distinguish, in error reporting, between "a source doesn't set this key at all" (fine, falls through) and "a source sets this key to an invalid value" (a real validation error) — these must never be conflated.
6. A hot-reload that produces an *invalid* configuration (fails schema validation) must not replace the previously-valid, currently-in-use configuration — the last known-good configuration remains active, with the validation failure surfaced to subscribers as an error event.

## Input

A schema declaration (keys, types, constraints), a config-file path, and access to environment variables and parsed CLI arguments.

## Output

A merged, validated, typed configuration object with change-notification support.

## Constraints

- C++20. Must reuse [P-3.5](../declarative-cli-parser/STATEMENT.md)'s CLI parser and [P-3.2](../file-watcher/STATEMENT.md)'s file watcher rather than reimplementing either — this project is explicitly about the *merging and validation* layer on top of already-solved sub-problems.
- The precedence-merge logic must be a distinct, independently testable component from the individual source-parsing logic (file parsing, env-var reading, CLI parsing) — a bug in "which layer wins" must be reproducible without needing a real file/environment/CLI invocation at all.
- Hot-reload must not introduce a window where a reader observes a partially-updated configuration (e.g. some keys reflecting the new file, others still the old one) — the swap to the new merged configuration must be atomic from a reader's perspective.

## Edge Cases

- A config file that doesn't exist at all — a legitimate, non-error case (falls through entirely to defaults/env/CLI), distinct from a config file that exists but fails to parse (a real error).
- An environment variable present but empty string — decide and document whether this counts as "set" (empty string is the value) or "unset" (falls through) for precedence purposes.
- A key present in the config file with a type that doesn't match the schema (e.g. a string where an integer is expected) — a validation error, not a silent coercion or a crash.
- Hot-reload racing with a concurrent read of the current configuration from another thread — must not produce a data race (verified under ThreadSanitizer) or a torn read.
- The config file being deleted while hot-reload watching is active — the loader must not crash, and must have a documented fallback behavior (keep last-known-good, presumably).

## Error Handling

- All schema validation errors are collected and reported together, each identifying the specific key and the specific problem (missing/wrong-type/out-of-range), never just a single generic failure.
- A config file that exists but is unparseable (malformed syntax) is a distinct, clearly-labeled error from a schema validation failure (bad syntax vs. valid syntax but invalid values).
- A hot-reload validation failure is reported to subscribers as a distinct event type from a normal successful-change notification.

## Acceptance Criteria

- A representative multi-key schema is declared, and the merge precedence is demonstrated explicitly: a key set at every layer resolves to the CLI value; a key set at only the file and default layers resolves to the file value; a key set nowhere resolves to its compiled-in default.
- Schema validation correctly collects and reports multiple simultaneous errors from one deliberately-broken configuration.
- Hot-reload is demonstrated end-to-end: a running program with an active configuration observes a specific, correctly-identified key change after the underlying file is edited on disk, without restarting the program.
- The invalid-reload-doesn't-replace-good-config behavior is demonstrated: a reload triggered by writing an invalid file leaves the previously-valid configuration active and in use, with the failure surfaced as a distinct event.
- A concurrent-read-during-reload test passes cleanly under ThreadSanitizer.

## Testing Requirements

- Precedence-merge unit tests using synthetic in-memory source values (no real file/env/CLI needed) covering every combination of "which layers set this key."
- Multi-error schema validation test.
- The empty-env-var and missing-config-file edge cases, each with their documented, distinct behavior verified.
- The hot-reload success and hot-reload-rejected-invalid-update scenarios, plus the concurrent-read-during-reload TSan-clean test.

## Hints

### Hint 1 — Direction
Structure this as two genuinely separate problems: first, reading each individual source (file/env/CLI) into some common, source-agnostic "key → optionally-set value" representation; second, and entirely independently, merging several of these source representations together according to precedence rules. The second problem is pure logic with no I/O at all, which is exactly why the Constraints section requires it to be independently testable without a real file or environment.

### Hint 2 — Technique
Represent each source's parsed result as something like `std::unordered_map<std::string, std::optional<ConfigValue>>` or simply a map that only contains keys the source actually set (absence of a key IS "unset," rather than needing an explicit optional-wrapping layer) — then the merge operation is a straightforward "iterate layers from lowest to highest precedence, letting a later layer's present key overwrite an earlier layer's" fold over these maps, independent of where each map came from.

### Hint 3 — Implementation
For the atomic hot-reload swap, consider building an entirely new, fully-validated merged configuration object off to the side (re-reading the file, re-merging, re-validating) and only atomically publishing it (e.g. via `std::atomic<std::shared_ptr<const Config>>` or an equivalent single-pointer-swap mechanism) once it's confirmed valid — readers always dereference the currently-published pointer, which is either the old, still-fully-valid configuration or the new one, never a half-updated mix of both.

### Hint 4 — Debugging/Design
If your concurrent-read-during-reload test is flaky or fails under ThreadSanitizer, check whether your "swap in the new config" step mutates the existing configuration object's fields in place (even briefly) rather than publishing a wholly new object atomically — any in-place field-by-field update, even guarded by a mutex around each individual field, can still let a reader observe an inconsistent mix of old and new field values unless the *entire* configuration is swapped as one atomic unit.
