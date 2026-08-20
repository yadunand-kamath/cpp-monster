# BC-3 — A Handle Leak That Only Happens on Windows

**Placement:** After Chapter 9 (Systems Programming: Linux and Windows Side by Side) · **Format:** Blind Challenge — requirements only, no canonical solution, no named target concepts.

## Premise

> "This service leaks handles under load — on Windows only."

You are handed a small service (a long-running process that opens and closes various OS resources — files, and/or sockets, and/or other kernel handles — as part of normal request handling) that runs correctly on Linux under sustained load, but on Windows, under the same sustained load, its handle count climbs without bound until the process eventually fails to open any further resource. Your task is to find the leak, fix it, and prove it's fixed — on Windows specifically, without regressing the Linux behavior that already works.

## What You're Given

- A working service with a load-generation script that drives sustained request traffic against it.
- The service builds and appears to run correctly on both platforms under light load; the leak only becomes observable under sustained load, and only on Windows.
- No hint about which resource type is leaking or which code path is responsible.

## Requirements

- Identify the specific code path and specific resource type responsible for the leak, with evidence (not a guess) — e.g. a handle-count measurement tied to a specific reproducible sequence of operations.
- Fix the leak such that handle count remains bounded (does not grow without bound) under sustained load, verified by running the load generator for a duration long enough that the original leak would have been clearly observable.
- The fix must not regress Linux behavior — the equivalent resource-management logic (which may or may not share code with the Windows path, depending on the service's existing platform-abstraction design) must continue to work correctly there.
- Explain, in writing, specifically why this leak manifests on Windows but not Linux given the code as written — a correct fix without a correct explanation of the platform divergence does not meet this requirement.

## What Success Looks Like

A bounded handle count on Windows under sustained load over an extended run, no Linux regression, and a precise written explanation connecting the bug to a genuine Windows-vs-Linux model difference (not just "Windows is different").

## Self-Assessment Questions

- Is your explanation of *why* this is Windows-specific grounded in an actual model difference (e.g. handle inheritance defaults, a resource-acquisition/release asymmetry that only matters under Windows's particular cleanup timing), or is it closer to "it works on Linux so I didn't look there"?
- Did you confirm the leak is actually fixed by observing bounded handle count over a long run, or only that it takes longer to manifest now?
- Is there a code path you didn't exercise under load that could have the same class of bug?
