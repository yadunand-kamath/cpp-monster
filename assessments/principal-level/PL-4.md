# PL-4 — "Our C++ Build Takes 40 Minutes. Design the Fix."

**Format:** Principal-level design problem. This document is a rubric for self- or peer-evaluation, not a solution. This prompt is deliberately under-specified — part of the exercise is recognizing what information you'd need before proposing a fix, rather than jumping straight to a generic answer.

## The Prompt

You're told: "our C++ build takes 40 minutes. Design the fix." No further detail is given up front. Before proposing any specific technical change, produce:

1. A list of the specific questions you would need answered before a real fix could be responsibly proposed (this list is itself a required deliverable, not throat-clearing before the "real" answer).
2. At least two or three *plausible, meaningfully different* root-cause hypotheses for a 40-minute C++ build, each with what evidence would confirm or rule it out.
3. Given a stated (invented, but internally consistent and realistic) answer to your own diagnostic questions, a concrete design for reducing the build time, with an estimate of the improvement and why you believe that estimate.
4. An explicit discussion of what you would *not* do, and why — common but often-wrong reflexive fixes for slow C++ builds, and why they don't apply (or do apply) to your invented scenario.

## Evaluation Rubric

### Problem framing (this is the part most answers skip)
- ☐ Actually produces a real diagnostic question list before proposing a fix — questions like: how many translation units, how much of the 40 minutes is compilation vs linking, is it a clean build or an incremental build being measured, how many of those minutes are parallelizable across cores already, what's the current toolchain and build system, is precompiled-header or module usage already present.
- ☐ The root-cause hypotheses are genuinely different in kind (e.g. "header bloat / redundant include graph causing excessive re-parsing," "link-time bottleneck from a small number of enormous translation units or heavy LTO settings," "the build system itself isn't parallelizing effectively even though the compiler work could be," "template instantiation bloat" ) — not three variations on the same guess.
- ☐ Each hypothesis states what specific evidence (a profiling tool, a `-ftime-trace`-style breakdown, a link-time breakdown, an include-graph analysis) would distinguish it from the others, rather than treating diagnosis as guesswork.

### Architecture and correctness
- ☐ The proposed fix is tied specifically to the invented-but-stated diagnostic answer from step 3 — a fix that would be proposed identically regardless of the diagnosis hasn't actually engaged with the diagnosis step.
- ☐ The improvement estimate has a stated rationale (even if approximate) connecting the specific mechanism (e.g. "reducing redundant header parsing across N translation units by X" ) to the claimed time savings — a bare number with no mechanism behind it doesn't meet this bar.
- ☐ Addresses whether the fix is a one-time win or compounds/decays over time as the codebase grows (e.g. does a precompiled-header strategy keep paying off as more files are added, or does it need active maintenance to stay effective).

### Tradeoffs and judgment
- ☐ The "what I would not do" section identifies at least one commonly-reached-for but often-inappropriate fix (e.g. "just enable unity/jumbo builds everywhere" or "just throw more CPU cores at it" or "just disable optimizations for local dev builds") and explains the actual conditions under which it would or wouldn't help — demonstrating judgment about when a popular fix is a red herring.
- ☐ Considers organizational/process cost of the proposed fix (e.g. a large-scale header refactor is a real fix but a real cost) alongside its technical merit, not purely the technical dimension.

### Communication
- ☐ A reader could tell, from the document, exactly what evidence led from "40 minutes, unknown cause" to the specific chosen fix — the reasoning chain is traceable, not just asserted.

## Suggested Self-Check

If you'd been handed a *different* invented diagnostic answer in step 3 (say, the opposite of what you assumed — link-bound instead of parse-bound, or vice versa), would your step-3 fix design have to change substantially? If your answer only makes sense for one specific diagnosis, that's a sign you engaged with the diagnostic step for real rather than writing the fix you wanted to write first and retrofitting a diagnosis to justify it.
