# BC-5 — 3x Slower After a Compiler Upgrade, Same Source

**Placement:** After Chapter 12 (Performance: Memory, Caches, Allocators, Measurement) · **Format:** Blind Challenge — requirements only, no canonical solution, no named target concepts.

## Premise

> "This code got 3x slower after a compiler upgrade. The source did not change."

You are handed a performance-sensitive piece of code, two compiler versions (an older one under which it meets its documented performance target, and a newer one under which it measures roughly 3x slower on the identical source and identical benchmark), and the benchmark harness itself. Your task is to determine exactly why the newer compiler produces slower code for this specific source, and to fix the *source* (not by pinning the compiler version, which is explicitly disallowed) so that performance is restored under the newer compiler.

## What You're Given

- The source code, unchanged between both measurements.
- Both compiler versions/toolchains, installed and usable.
- A benchmark harness (built along the lines of [P-5.1](../../projects/level-5/allocator-container-benchmark-harness/STATEMENT.md)'s design) that reproduces the 3x slowdown reliably when switching compilers.
- No hint about which specific language construct or optimization the newer compiler handles differently — that diagnosis is the point.

## Requirements

- Pinning the toolchain to the older compiler version is explicitly disallowed as a solution — the fix must be a source change that restores performance under the *newer* compiler while continuing to meet the same performance target under the *older* one (no regression on the compiler that already worked).
- Identify the specific reason for the regression with evidence — e.g. generated-assembly comparison, a specific optimization that fired under the old compiler and didn't under the new one (or vice versa — a "pessimization" is also a valid finding), or a specific standard-mandated behavior change between compiler versions that affects codegen.
- The source change must not alter the code's observable behavior — this is a pure performance fix, verified by the existing correctness test suite remaining green.
- Restore performance to within the documented target under the newer compiler, measured via the same benchmark harness used to detect the regression.

## What Success Looks Like

A precise causal account of the regression (down to the specific construct/optimization/behavior-change level, not "newer compilers are sometimes slower"), a source-level fix with no behavior change, and benchmark evidence that the fix restores the target under the newer compiler without regressing the older one.

## Self-Assessment Questions

- Did you actually inspect generated assembly (or an equivalent compiler-explorer-style comparison) to confirm your causal account, or infer it indirectly from the benchmark numbers alone?
- Is your fix specific to the actual mechanism you identified, or a broad "try several things until the number improves" change whose real cause you can't fully explain?
- Would your fix's rationale still hold up if a *third*, even newer compiler version were introduced — or is it narrowly tailored to exactly these two versions' specific behavior?
