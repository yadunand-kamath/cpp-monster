# P-4.1 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch09's `epoll`-vs-IOCP material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Resist trying to make IOCP "look like" epoll (or vice versa) at the implementation level — they represent genuinely different models (readiness notification vs. completion notification) and forcing a false equivalence tends to produce an awkward implementation on whichever side you didn't design around first. Instead, design your loop's *public* callback contract around the one thing both models can honestly promise: "your registered operation's result is now available without blocking." Let each backend do whatever is natively appropriate to reach that point before invoking your common callback.

## Hint 2 — Technique

The clean way to fold timers into the same single wait call as I/O is to always compute "how long until the next timer deadline" before every wait, and pass that as the wait's timeout — after the wait returns (whether because I/O happened or because the timeout elapsed), always check for and fire any timers whose deadline has now passed, regardless of why the wait returned. This keeps timers and I/O genuinely on the same loop iteration rather than needing separate threads or polling.

## Hint 3 — Implementation

For the Windows cancellation race (an overlapped operation whose completion is already sitting in the IOCP queue at the moment you decide to cancel it), you generally cannot retract an already-queued completion from the OS's perspective — what you *can* do is tag each submitted operation with an identifier your loop still owns, mark that identifier "cancelled" in your own bookkeeping the moment `cancel()` is called, and when a completion for that identifier is eventually dequeued, check the cancelled flag before invoking any callback. This makes cancellation observably effective (the callback never fires) even though the underlying OS-level I/O might have technically completed.

## Hint 4 — Debugging/Design

If a CPU-usage-while-idle test unexpectedly shows near-100%-of-a-core usage, check the exact timeout value you're passing into your wait call on each iteration — a bug in computing "time until next timer deadline" that occasionally evaluates to zero or a negative duration (e.g. from a deadline that's already slightly in the past due to floating-point or duration-cast rounding) will make an otherwise-correct blocking wait call return immediately every time, which is indistinguishable from busy-polling in its effect even though the code "looks like" it's using a blocking API correctly.
