# PL-3 — Design an ABI and Versioning Policy for a Five-Year, Two-Platform External Library

**Format:** Principal-level design problem. This document is a rubric for self- or peer-evaluation, not a solution.

## The Prompt

Design the ABI and versioning policy for a C++ library that will be shipped as a binary to external customers (not rebuilt from source by them) on two platforms (Linux and Windows), with an expected support lifetime of five years, during which the library will need to add features, fix bugs, and potentially change internal implementation details — all without breaking any existing customer's already-deployed binary that links against an earlier version.

Your design should address, at minimum:
- What "ABI compatibility" concretely means for this library's public interface (which language features are ABI-safe to use across a version boundary and which are not, for C++ specifically).
- A concrete policy for how new functionality can be added without breaking old binaries (e.g. how a new virtual method, a new struct member, or a new optional parameter is handled).
- How the two platforms' differing ABI conventions and toolchains (MSVC vs GCC/Clang, differing default calling conventions, differing STL ABI stability guarantees) factor into the policy.
- A versioning scheme (both for the library's own release versioning and for any explicit interface-version field, if your design uses one) and what compatibility guarantee each version-number component carries.
- What happens when a genuine breaking change is unavoidable — how it's communicated, staged, and how customers migrate.
- At least one explicit, named tradeoff.

## Evaluation Rubric

### Problem framing
- ☐ Correctly identifies that "ABI stability" for C++ specifically (as opposed to a C library) has to grapple with the fact that most idiomatic C++ (virtual functions, templates, inline functions, STL types) is not ABI-stable across compiler versions by default — a design that doesn't acknowledge this hasn't engaged with the actual hard part of the prompt.
- ☐ States explicitly which parts of the interface are the ABI boundary (the actually-exported symbols/vtables/struct layouts) versus which are just "the public headers," since these are not the same thing.

### Architecture and correctness
- ☐ The extensibility mechanism (how new functionality is added without breaking old binaries) is concrete and specific — e.g. "new virtual methods are only ever added at the end of a vtable, never inserted" or "the public interface is a C-style struct-of-function-pointers with a version field, mirroring [P-5.6](../../projects/level-5/plugin-host-stable-abi/STATEMENT.md)'s approach" — not a vague "we'll be careful."
- ☐ Addresses the specific Linux-vs-Windows ABI divergences relevant to this problem (symbol visibility defaults, calling convention defaults, STL ABI stability differences between libstdc++/libc++ and MSVC's STL) rather than treating "cross-platform" as a detail to solve later.
- ☐ The versioning scheme's guarantees are precise: given two specific version numbers, a reader should be able to determine from the policy alone whether they're guaranteed compatible.

### Tradeoffs and judgment
- ☐ Addresses the tradeoff between ABI stability and being able to freely use modern, idiomatic C++ internally (a common resolution: a stable, narrow, boring C-shaped public boundary wrapping freely-evolving idiomatic C++ internals) — and states which resolution was chosen and why.
- ☐ At least one tradeoff is named with a rejected alternative and specific reasoning.
- ☐ Addresses what happens at end-of-life for an old version — is indefinite backward compatibility promised, or is there a stated deprecation horizon, and how is that decision justified against the five-year lifespan constraint.

### Communication
- ☐ A reader could, from the document, correctly determine whether a specific proposed interface change (e.g. "add a new parameter with a default value to an existing exported function") is or is not ABI-safe under this policy.
- ☐ The document distinguishes clearly between what the policy guarantees and what it merely encourages/recommends.

## Suggested Self-Check

Take one specific hypothetical future change (adding a genuinely new capability that doesn't fit cleanly into the existing interface shape) and walk it through your own policy step by step — if your policy doesn't have a clear answer for it, that's a real gap, not a hypothetical one, given a five-year horizon. This connects directly to [P-5.6](../../projects/level-5/plugin-host-stable-abi/STATEMENT.md), whose stable-C-ABI-boundary design is a concrete, working example of exactly the kind of boundary this policy needs to define abstractly.
