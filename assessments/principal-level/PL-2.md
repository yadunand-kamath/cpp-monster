# PL-2 — Design a Storage Engine for a Write-Heavy Time-Series Workload

**Format:** Principal-level design problem. This document is a rubric for self- or peer-evaluation, not a solution.

## The Prompt

Design a storage engine specifically for a write-heavy time-series workload: a very high rate of small, append-mostly writes (each write tagged with a timestamp, arriving in roughly-but-not-strictly increasing time order), queried predominantly by time-range scans over a specific series, with retention/expiry of old data being a first-class, routine operation rather than an exception. Assume this is a component underneath a larger monitoring/observability system, not a general-purpose database.

Your design should address, at minimum:
- The on-disk data organization and why it fits this specific workload's read/write shape (contrast at least briefly with a general-purpose engine's organization, and explain why time-series changes the calculus).
- How out-of-order or late-arriving writes are handled, if your design's write path assumes rough time order.
- Retention/expiry as a designed-in operation, not an afterthought — how old data is identified and reclaimed, and what that costs.
- Durability and crash recovery.
- Compression strategy, if any, and its interaction with the query path (can compressed data be queried without full decompression?).
- At least one explicit, named tradeoff, with the rejected alternative and why.

## Evaluation Rubric

### Problem framing
- ☐ Explicitly identifies what makes this workload different from a general-purpose OLTP or OLAP workload (append-heavy, time-ordered, range-queried, retention-heavy) and lets that shape the design, rather than presenting a generic LSM-tree or B+tree design with "time series" as a label.
- ☐ States concrete assumptions about write rate, series cardinality, and query patterns.

### Architecture and correctness
- ☐ The on-disk organization is described with enough specificity (block/chunk structure, indexing scheme) that its read and write amplification characteristics could be reasoned about from the description.
- ☐ Out-of-order write handling is addressed explicitly — either a stated mechanism for it, or an explicit, justified decision that the system doesn't support it and what happens when it occurs anyway.
- ☐ Retention/expiry is described as a concrete mechanism (e.g. whole-block deletion aligned to time boundaries versus fine-grained per-record deletion) with its own cost analysis — a design that says "just delete old rows" without addressing the cost of doing so at this write volume is incomplete.
- ☐ Crash recovery is addressed with a specific mechanism, not just "there's a WAL."

### Tradeoffs and judgment
- ☐ Compression's tradeoff against query-path complexity (or decompression cost) is addressed if compression is proposed at all.
- ☐ At least one tradeoff is named with a real rejected alternative and a specific reason.
- ☐ The design acknowledges a specific workload shift (e.g. suddenly write-order becomes very out-of-order, or query patterns shift to point-lookups rather than range scans) that would break its core assumption, and says what would have to change.

### Communication
- ☐ A reader could predict, from the document, roughly what a write costs and what a time-range query costs, structurally (not necessarily exact numbers).
- ☐ Retention behavior over time (what the system looks like after a year of continuous operation at the stated write rate) is addressed, not just steady-state behavior at time zero.

## Suggested Self-Check

Pick the single query pattern your design handles worst, and explain honestly how bad "worst" actually is — a design that's excellent for the common case but silent about its worst case hasn't been fully thought through. This connects directly to [C-1](../../projects/capstones/embeddable-persistent-kv-store/STATEMENT.md) — that capstone's storage-engine architecture decision (LSM vs B+tree) and this prompt's are closely related design spaces; working through C-1 first, or alongside this, will surface real tradeoffs a purely theoretical design pass can miss.
