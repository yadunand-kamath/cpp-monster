# P-4.6 — Process Supervisor

**Level:** 4 (Systems component) · **Category:** Systems · **Requires:** Ch01–09,11 · **Est. effort:** L (16-24h)

## Objective

Build a supervisor that launches a child process, captures its stdout/stderr, monitors its lifetime, and automatically restarts it with exponential backoff on unexpected exit — while supporting both graceful (`SIGTERM`/`GenerateConsoleCtrlEvent`) and forceful (`SIGKILL`/`TerminateProcess`) termination, on both Linux and Windows.

## Functional Requirements

1. Launch a child process with a specified command line, capturing its stdout and stderr into readable streams (not just inherited to the console) — the supervisor must be able to observe and log the child's output as it happens, not only after the child exits.
2. Detect child exit (normal exit code, or abnormal termination) promptly, without busy-polling.
3. On unexpected exit (any exit the supervisor didn't itself request), automatically restart the child, with exponential backoff between restart attempts (bounded by a configurable maximum delay and, optionally, a maximum restart-attempt count).
4. Support two termination modes distinguishable by the caller: a graceful request (`SIGTERM` on Linux, `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT)` or equivalent on Windows) giving the child a bounded grace period to exit on its own, escalating to a forceful kill (`SIGKILL` on Linux, `TerminateProcess` on Windows) only if the grace period elapses without exit.
5. Expose current supervised-process state (not running / starting / running / stopping / restart-backoff) and basic history (restart count, last exit code, last exit time) queryable at runtime.
6. A documented, deliberate stop condition — an explicit "stop supervising" call that does not trigger the automatic-restart logic (distinguishing "the supervisor asked it to stop" from "it died on its own").

## Input

A command line (executable path + arguments), and supervisor configuration (backoff parameters, grace period duration, optional max restart attempts).

## Output

Captured child stdout/stderr (streamed, not just buffered until exit), queryable supervisor state, and restart/exit history.

## Constraints

- C++20. Must not busy-poll for child exit detection — Linux via `waitpid`/`SIGCHLD`-driven or a dedicated waiting thread blocked in `waitpid`; Windows via `WaitForSingleObject` on the process handle (blocking, in a dedicated thread, or integrated into an event loop).
- Must correctly distinguish supervisor-requested stop from unexpected exit — the same underlying OS-level exit notification occurs in both cases, so this distinction must be tracked in the supervisor's own state, not inferred from the OS.
- Backoff delay must actually grow between consecutive unexpected restarts (not be a fixed retry interval) and must be capped, not grow unbounded.

## Edge Cases

- The child process ignores the graceful termination signal entirely (a hung or misbehaving child) — the grace-period timeout must still fire and escalate to forceful termination; the supervisor must not itself hang waiting indefinitely for a child that won't cooperate.
- The child exits extremely quickly, faster than the supervisor's launch bookkeeping completes ("fork bomb"-adjacent rapid-crash-loop scenario) — backoff must actually engage rather than the supervisor attempting to relaunch at effectively unbounded frequency.
- The child writes enough stdout/stderr fast enough to fill an OS pipe buffer before the supervisor reads it — the supervisor must not deadlock (a classic pipe-full deadlock if the parent is simultaneously blocked writing something to the child while the child blocks writing to a full stdout pipe the parent isn't draining).
- The supervisor process itself is asked to shut down while a child is mid-restart-backoff — the backoff wait must be interruptible, not a plain `sleep()` that ignores a shutdown request.

## Error Handling

- The command line refers to a nonexistent executable or one lacking execute permission — reported as a distinguishable launch failure (not silently treated as "child exited with some code"), and this specific failure mode should also engage backoff (a permanently-broken command line would otherwise retry at unbounded frequency forever).
- A grace-period termination request sent to a process that has, unbeknownst to the supervisor, already exited (race between the supervisor's decision and the OS's own notification) — must not crash or throw an unhandled platform error, since this is a legitimate, expected race.

## Acceptance Criteria

- A child that exits normally with code 0 is not restarted (respecting a configurable "exit 0 counts as a deliberate stop" policy, if that's the chosen design) or is restarted per policy — whichever policy is chosen must be explicit and tested.
- A child that crashes repeatedly triggers restarts with measurably increasing backoff delay between attempts, capped at the configured maximum.
- A graceful-termination request against a cooperative child results in the child exiting on its own within the grace period, without ever escalating to forceful kill.
- A graceful-termination request against an uncooperative (signal-ignoring) child results in the grace period elapsing and forceful termination actually occurring.
- Captured stdout/stderr from the child is observable by the supervisor while the child is still running, not only after it exits.

## Testing Requirements

- Tests using a small helper child program (built as part of the test suite) whose behavior is controllable via arguments (exit immediately with a given code; ignore SIGTERM/CTRL_BREAK and loop; print a burst of output and exit; sleep for a duration).
- The restart-with-backoff test, measuring actual inter-attempt delays.
- The graceful-then-forceful escalation test against both a cooperative and an uncooperative helper child.
- The deliberate-stop-does-not-restart test.
- The pipe-buffer-fill stress test (helper child writes enough output to exceed typical OS pipe buffer size) confirming no deadlock.

## Hints

### Hint 1 — Direction
Separate the supervisor into two concerns that are easy to accidentally entangle: "how do I launch/monitor/terminate one OS process" (inherently platform-specific — `fork`+`exec` and `waitpid` versus `CreateProcess` and `WaitForSingleObject`) and "what is the restart/backoff policy given a sequence of exit events" (entirely platform-independent state-machine logic). Keeping the second concern ignorant of the first's platform details is what makes this project's core logic testable without actually spawning real child processes for every policy test.

### Hint 2 — Technique
For draining a child's stdout/stderr without deadlocking on a full pipe, a dedicated reader thread per stream (continuously reading into a buffer/log as data arrives, for the lifetime of the child) is the simplest robust technique — it decouples "the child is producing output" from "the supervisor happens to be ready to consume it right now," which is exactly the coupling that causes the pipe-full deadlock described in Edge Cases.

### Hint 3 — Implementation
Exponential backoff is typically `delay = min(base_delay * 2^attempt_count, max_delay)`, reset back to `base_delay` once the child has stayed alive past some "considered stable" duration (otherwise a child that crashes once after running successfully for hours gets punished with a long backoff for what may have been an unrelated one-off issue) — decide and document your stability-reset policy explicitly rather than letting backoff grow monotonically forever across the supervisor's whole lifetime.

### Hint 4 — Debugging/Design
If your graceful-termination test against an uncooperative child hangs instead of correctly escalating to forceful kill, check whether your grace-period wait is actually running on a timer independent of the child's exit notification — a common bug is structuring the wait as "block until child exits OR timeout" using a mechanism where the timeout path was never properly wired to actually fire the escalation, silently reducing the whole grace-period mechanism to an unbounded wait.
