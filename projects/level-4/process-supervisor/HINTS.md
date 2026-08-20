# P-4.6 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch09's process/signal material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Split this into a platform-specific `ChildProcess` type (launch, capture pipes, request graceful/forceful termination, wait for exit — one implementation using `fork`/`exec`/`waitpid`/signals, another using `CreateProcess`/`WaitForSingleObject`/`GenerateConsoleCtrlEvent`/`TerminateProcess`) and a platform-independent `Supervisor` state machine that only ever calls that type's interface. The state machine's restart/backoff policy tests then never need to actually deal with platform APIs at all.

## Hint 2 — Technique

A dedicated thread per output stream, blocked in a read loop for the child's entire lifetime, appending each read to a buffer or forwarding it to a log — this is the standard fix for the pipe-full deadlock in Edge Cases, since it guarantees something is always draining the pipe regardless of what else the supervisor's main logic is doing at that moment.

## Hint 3 — Implementation

The grace-period-then-escalate sequence is naturally expressed as: send the graceful signal, then wait for child exit with a bounded timeout (`waitpid` with `WNOHANG` polled against a deadline, or better, a wait in a dedicated thread combined with a timed condition variable; on Windows, `WaitForSingleObject` already accepts a timeout directly) — if the timeout elapses without the child having exited, only then send the forceful kill and wait again (this second wait can be unbounded, since a forceful kill's success is not something a well-behaved OS lets hang indefinitely).

## Hint 4 — Debugging/Design

If your rapid-crash-loop test shows the supervisor attempting to relaunch far faster than expected even with backoff configured, check whether your backoff calculation is being applied *before* each restart attempt or only being recorded *after* — a common bug is incrementing the attempt counter and computing the next delay only after the wait has already happened once with the base delay, which means the very first few crash-loop iterations occur at full speed before backoff catches up, which may or may not be your intended semantics but must be a deliberate, documented choice either way.
