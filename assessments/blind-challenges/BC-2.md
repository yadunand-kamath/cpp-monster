# BC-2 — Unifying Inconsistent Error Reporting Without Breaking the Public API

**Placement:** After Chapter 6 (Error Handling and API Failure Design) · **Format:** Blind Challenge — requirements only, no canonical solution, no named target concepts.

## Premise

> "This library reports errors three inconsistent ways. Unify it. The public API may not break."

You are handed a small library whose functions report failure inconsistently: some return error codes, some throw exceptions, some return a sentinel value indistinguishable from a valid result, and at least one silently logs and returns a default. Callers currently have to know, function by function, which convention applies — a maintenance and correctness hazard. Your task is to unify error reporting behind one consistent, coherent policy, without breaking any existing caller.

## What You're Given

- A small library (several translation units) with a full passing test suite that exercises both success and failure paths for every function, written against the *current*, inconsistent conventions.
- No guidance on which unification strategy to adopt (a single `expected`-based convention throughout, a single exception hierarchy throughout, or some other coherent policy) — that choice is yours to make and justify.

## Requirements

- Every existing public function's signature must remain source- and binary-compatible with existing callers, OR — if you determine full backward compatibility while unifying error reporting is impossible for a specific function, you must explicitly document why, and provide a compatibility shim (e.g. a deprecated overload preserving the old behavior) rather than silently breaking that caller.
- All failure paths, across the entire library, must now report errors through one coherent, consistent mechanism — no function may still silently swallow an error or return an ambiguous sentinel after your change.
- Every existing test must continue to pass — if a test's meaning depended on the old inconsistent convention (e.g. it asserted on a specific detectable-only-via-exception failure), the test itself may be adapted, but only with a written justification of why the adaptation preserves the original test's intent.
- No behavior change on any success path.

## What Success Looks Like

A single, stated error-handling policy applied uniformly, a full green test suite, backward compatibility preserved or explicitly and narrowly broken with justification and a shim, and a short written rationale for the chosen unification strategy over the alternatives.

## Self-Assessment Questions

- Did you unify toward whichever convention was easiest to retrofit, or the one that's actually the better fit for this library's failure modes (e.g. are failures here truly exceptional, or are they routine enough that forcing exceptions would be the wrong choice)?
- For any function where full backward compatibility wasn't achievable, is your documented reason genuinely unavoidable, or a shortcut?
- Would a caller migrating from the old inconsistent API to relying on your new unified convention actually find it easier to write correct error-handling code than before?
