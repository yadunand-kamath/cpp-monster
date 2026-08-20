# P-5.5 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch09's networking material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Get the response parser fully correct and unit-tested against raw byte strings (no real sockets involved at all) before writing any connection-pooling or rate-limiting code — status line, headers, then a fork into `Content-Length` or chunked body decoding. Every other piece of this project (pooling, retries, rate limiting) operates on top of "did we get a complete, correctly-parsed response," so a shaky parser will make every higher-level test flaky and hard to debug for the wrong reasons.

## Hint 2 — Technique

The connection pool's checkout/checkin discipline is the whole mechanism preventing two concurrent requests from sharing one connection: a connection is either "idle in the pool," "checked out to exactly one in-flight request," or "discarded" — never any other state, and never accessible from two places at once. A `std::mutex`-protected map from `host:port` to a list of idle connections, with checkout removing an entry (or creating a new connection) and checkin appending it back, is sufficient; no connection object should ever be reachable through the pool's idle list while also being actively used by a request.

## Hint 3 — Implementation

The token bucket's refill calculation on each `acquire()` call: `tokens = min(capacity, tokens + (now - last_refill).count() * rate_per_sec)`, then update `last_refill = now`. If `tokens >= 1`, consume one and return immediately; otherwise compute `wait_time = (1 - tokens) / rate_per_sec` and actually sleep/block for that duration before consuming — the bucket's state only needs updating at the moments `acquire()` is actually called, no background timer thread is required.

## Hint 4 — Debugging/Design

If your stale-connection test intermittently fails depending on exactly when the server happened to close its end, check whether your recovery logic treats *any* failure during a request against a reused (previously idle, checked back out) connection as "this connection was stale, retry fresh" — rather than trying to precisely diagnose whether the failure came from `send()` or `recv()` or where exactly, which varies by OS/timing and isn't reliably distinguishable from a genuine, non-staleness-related failure anyway; the safe general rule is "a request against a connection taken from the idle pool that fails at the network layer gets exactly one fresh-connection retry, transparently," which sidesteps needing to precisely characterize every possible way staleness can surface.
