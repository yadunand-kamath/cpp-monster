# P-5.3 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch09's I/O and Ch11's concurrency material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Build and thoroughly test the framing state machine (`FrameParser` in the tests above) completely independently of any networking code first — feed it byte strings directly in a unit test, in whatever arbitrary chunk boundaries you like, and confirm it correctly reassembles frames. Only once that's solid should you wire it up to actual socket reads via the event loop; this separation is what makes the tricky "message split across reads" logic debuggable without needing a live TCP connection to reproduce every case.

## Hint 2 — Technique

Each connection needs exactly three pieces of state tracked by the server: an incoming `FrameParser` (fed whatever bytes the latest `read()` produced), an outgoing byte queue (appended to when a response is ready to send, drained by write-readiness notifications from the event loop), and a small metadata block (last-activity timestamp for idle/partial-frame timeout tracking, current queued-byte count for backpressure). None of these need to know about any other connection — connections should be fully independent, which is also what makes the "one slow client doesn't affect others" requirement structurally true rather than something you have to work hard to preserve.

## Hint 3 — Implementation

For the maximum-message-size check, read only the fixed-size length prefix first (the event loop's readiness-driven read naturally supports "accumulate bytes until you have at least N," reusing the same accumulate-then-decide pattern the payload phase uses) and validate it against your configured maximum *before* transitioning the parser's internal state machine into "now accumulating payload bytes" — if the check fails, the frame is rejected and the connection closed right there, with no payload-sized buffer ever allocated.

## Hint 4 — Debugging/Design

If your backpressure test doesn't show the configured policy actually engaging, check whether you're tracking queued-byte count independently of the OS socket send buffer, or conflating the two — the OS send buffer is typically much smaller than what you'd want your policy threshold to be, and your own outgoing queue (Hint 2) is what should be measured against your configured `max_queued_write_bytes`, not merely how much the OS itself happens to be buffering, since your queue is what will actually grow unboundedly in process memory if a client stops reading.
