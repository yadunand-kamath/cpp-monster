# P-4.1 — Single-Threaded Event Loop (epoll + IOCP)

**Level:** 4 (Systems component) · **Category:** Systems · **Requires:** Ch01–09 · **Est. effort:** L (16-24h)

## Objective

Build a single-threaded event loop that multiplexes socket/file I/O and timers behind one interface, backed by `epoll` (readiness-based) on Linux and IOCP (completion-based) on Windows — two fundamentally different concurrency models unified into the same observable API, including support for scheduled timers and mid-flight cancellation.

## Functional Requirements

1. Register interest in a file descriptor/socket for readability and/or writability (Linux) or issue an overlapped operation (Windows), and invoke a callback when the corresponding event/completion occurs.
2. Support one-shot and repeating timers, integrated into the same loop (not a separate thread), with correct ordering relative to I/O events (a timer firing must not starve pending I/O indefinitely, and vice versa).
3. Support cancellation of a previously-registered I/O interest or timer before it fires — cancellation must be safe even if the corresponding event is concurrently "in flight" at the OS level (e.g. an IOCP completion already queued but not yet dequeued at cancellation time).
4. Run the loop until explicitly stopped, with a `stop()` that can be called from within a callback (stopping after the current iteration) or is otherwise documented as to its exact semantics.
5. Correctly handle a registered fd/socket being closed while still registered — either require the caller to deregister first (documented contract) or detect and handle the OS-level implications (e.g. `epoll` automatically removing a closed fd) explicitly.
6. Provide the loop's core abstraction as one platform-independent public interface, with `epoll` and IOCP as swapped-out backend implementations selected at compile time.

## Input

Callback registrations against file descriptors/sockets/timers, submitted from the same thread the loop runs on (single-threaded design — no cross-thread registration in the base requirement, though see Extensions).

## Output

Callback invocations as their corresponding I/O readiness/completion or timer expiry occurs, for as long as the loop runs.

## Constraints

- C++20. Correct, working implementations required on both platforms — this is a required, not optional, paired-platform application of Ch09.
- Single-threaded per loop instance (one loop, one thread, running it) — this project is explicitly about the event-multiplexing model, not about building a thread pool on top of it (that's [P-4.3](../work-stealing-thread-pool/STATEMENT.md)).
- Must not busy-poll — both backends must block (with a bounded or unbounded wait, respecting the next timer's deadline) when there is genuinely nothing to do.

## Edge Cases

- Two timers scheduled for the exact same deadline — a documented, deterministic tie-breaking order (e.g. registration order).
- A callback that registers a *new* I/O interest or timer from within its own invocation — must be picked up correctly on a subsequent loop iteration, not lost or double-registered.
- A callback that cancels a *different*, not-yet-fired registration from within its own invocation.
- Cancelling a Windows overlapped operation that has already completed at the OS level but whose completion hasn't yet been dequeued from the IOCP — the callback must not fire after cancellation is requested, or the exact race window must be clearly documented if eliminating it entirely proves impractical.

## Error Handling

- Registering an invalid/already-closed fd — a clear, immediate error, not a corrupted loop state.
- An OS-level error surfaced during an I/O wait (e.g. `epoll_wait` returning an error) — reported to the caller in a way that doesn't silently terminate the loop without explanation.

## Acceptance Criteria

- A working demonstration multiplexing at least: one listening socket accepting connections, several established connections doing read/write I/O, and at least one repeating timer — all correctly interleaved on a single thread, with measured evidence the CPU is not spinning (near-zero CPU usage while idle, verified via a system monitoring tool or an internal busy-loop counter).
- The cancellation edge case (Windows overlapped-operation-already-completed race) is explicitly tested and its actual observed behavior documented.
- The loop correctly runs for an extended period (e.g. tens of thousands of simulated I/O events and timer firings) without leaking file descriptors/handles or growing memory unboundedly.

## Testing Requirements

- Timer-ordering tests (single timer, multiple timers at different deadlines, same-deadline tie-break).
- I/O readiness/completion tests using real sockets (loopback TCP is sufficient — no external network dependency).
- The register-from-within-a-callback and cancel-from-within-a-callback tests.
- A long-running stress test checking for fd/handle leaks and unbounded memory growth.
- The Windows-specific cancellation race test, documented with its actual observed outcome.

## Hints

### Hint 1 — Direction
`epoll` tells you "this fd is ready for the operation you asked about" and you then perform the I/O yourself; IOCP tells you "the operation you already submitted has completed, here's the result" — these are opposite models (readiness vs. completion) and forcing one to look exactly like the other at the point of use is usually a mistake. Instead, design your public callback contract around what both models can agree to promise ("your operation is now ready to complete without blocking," which for epoll means "do the read/write now," and for IOCP means "here's the result of the read/write that already happened") and let each backend do whatever native-shaped work is needed to deliver on that promise.

### Hint 2 — Technique
For unifying timers with I/O waiting without a separate thread, compute the next timer deadline before each wait call and pass it as that wait's timeout (`epoll_wait`'s timeout parameter; `GetQueuedCompletionStatus`'s timeout parameter) — if the wait returns due to timeout rather than an I/O event, process any timers whose deadline has passed, then loop back to waiting with the *next* deadline recomputed.

### Hint 3 — Implementation
For safe cancellation on Windows given the queued-but-not-dequeued completion race, consider tagging every submitted operation with a generation/version number or a "cancelled" flag checked when its completion is eventually dequeued — even if the OS can't truly "un-submit" an in-flight operation, your loop can still recognize a stale completion belonging to a cancelled registration and simply not invoke its callback, achieving the required *observable* behavior even if the underlying OS-level operation technically still ran to completion.

### Hint 4 — Debugging/Design
If your loop's CPU usage under idle conditions is unexpectedly high, check whether your wait timeout calculation is defaulting to something like 0 or a very small value in some code path — this is the most common way an event loop accidentally becomes a busy-poll despite superficially "using" the correct blocking wait API, since a technically-blocking call with a near-zero timeout behaves indistinguishably from spinning.
