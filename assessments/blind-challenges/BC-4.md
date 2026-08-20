# BC-4 — A Queue That Passes 10,000 Test Runs and Fails Weekly in Production

**Placement:** After Chapter 11 (Concurrency and the C++ Memory Model) · **Format:** Blind Challenge — requirements only, no canonical solution, no named target concepts.

## Premise

> "This queue passes 10,000 test runs and fails in production once a week."

You are handed a concurrent queue implementation with an existing test suite that passes reliably, every time, across thousands of repeated runs on your machine. In production, under real load and real thread-scheduling behavior, it exhibits a rare but real correctness failure roughly once a week (a lost item, a duplicated item, or a crash — you are not told which in advance). Your task is to find the actual race, understand why the existing tests don't catch it, and fix it — and to explain why "passes reliably in testing" and "is correct" were not the same claim here.

## What You're Given

- A concurrent queue implementation and its existing (currently green, thousands-of-runs-reliable) test suite.
- A description of the production failure mode's rough shape (which of: lost item, duplicated item, or crash — provided at the start, since diagnosing *that* much is not itself the point of this challenge) but not its cause.
- Access to ThreadSanitizer and any other diagnostic tooling covered in this workbook.

## Requirements

- Identify the specific race condition responsible, with a reproduction that demonstrates it reliably (even if the reproduction requires exaggerating timing, e.g. via deliberately injected delays, thread pinning, or a stress multiplier — the point is a *deterministic-enough-to-debug* repro, not matching production's exact one-a-week rate).
- Fix the race such that the reproduction you built no longer demonstrates the failure, and the fix is verified clean under ThreadSanitizer across a large number of stress-test runs.
- Explain in writing specifically why the existing test suite, despite thousands of clean runs, did not catch this — what about the race's timing window made it invisible to the existing tests' interleaving patterns.
- The fix must not degrade the queue's documented performance characteristics in a way inconsistent with its intended use (a fix that "works" by adding a global lock around everything, defeating the point of a concurrent queue, does not meet this requirement unless you argue convincingly that no better fix exists).

## What Success Looks Like

A reliable reproduction of the actual race, a ThreadSanitizer-clean fix, a precise explanation of the timing window the original tests missed, and a fix that preserves the queue's concurrency-related value proposition.

## Self-Assessment Questions

- Did you find the specific memory-ordering or synchronization gap responsible, or did you make a broad, defensive change that happens to make the symptom go away without a clear causal account of why?
- Could your reproduction technique (exaggerated delays, thread pinning, etc.) be added to the permanent test suite in some form, so this class of bug is caught by CI in the future rather than requiring another production incident?
- Is there a *related* race elsewhere in the same file that your fix doesn't address, now that you understand the pattern that produced this one?
