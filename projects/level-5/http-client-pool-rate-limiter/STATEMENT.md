# P-5.5 — HTTP/1.1 Client with Connection Pool & Rate Limiter

**Level:** 5 (Production-style) · **Category:** Networking · **Requires:** Ch01–11 · **Est. effort:** L (16-24h)

## Objective

Build an HTTP/1.1 client library — request construction, response parsing (including chunked transfer encoding), persistent-connection pooling with keep-alive reuse, a token-bucket rate limiter, and retry-with-jitter — the client-side counterpart to [P-5.3](../concurrent-tcp-protocol-server/STATEMENT.md)'s server, and a good vehicle for understanding why a real HTTP client is meaningfully more than "open a socket and send some bytes."

This project is structured in phases. Each phase has its own exit bar; don't move on until the current one is met.

## Phase 1 — Request/Response Correctness

Get well-formed request construction and response parsing working for the simple `Content-Length`-framed case, one request per connection. Exit bar: a test against a real (or faithfully simulated) HTTP/1.1 server round-trips several request shapes (different methods, headers, bodies) correctly.

## Phase 2 — Chunked Transfer Encoding

Add chunked-response decoding. Exit bar: a response with `Transfer-Encoding: chunked`, including a chunk boundary split across multiple `recv()` calls, decodes to the correct body content.

## Phase 3 — Connection Pooling

Add keep-alive reuse and correct handling of `Connection: close` from either side. Exit bar: a test proves a second request to the same host reuses the existing TCP connection (no new connect() call observed), and a `Connection: close` response correctly triggers actual closure rather than reuse.

## Phase 4 — Rate Limiting and Retry

Add the token-bucket limiter and retry-with-jitter. Exit bar: a burst of requests exceeding the configured rate is measurably throttled to the configured limit; a request against a server simulating transient failures succeeds after backoff-with-jitter retries, and a multi-client simulation shows retries are not synchronized in lockstep.

## Phase 5 — Concurrent Robustness

Confirm correctness under partial reads/writes and back-to-back responses on a reused connection under concurrent client use. Exit bar: a stress test issuing many concurrent requests across a shared connection pool completes with every response correctly matched to its request and no cross-response data corruption.

## Functional Requirements

1. Construct and send well-formed HTTP/1.1 requests (method, path, headers, optional body) and correctly parse responses, including status line, headers, and body.
2. Correctly handle chunked transfer encoding on responses (`Transfer-Encoding: chunked`) — decoding the chunk-size-prefixed body format into the actual response body content, distinct from the simpler `Content-Length`-framed case.
3. Connection pooling with keep-alive reuse: a connection to a given host:port is, by default, kept open and reused for a subsequent request to the same host rather than establishing a new TCP connection each time — respecting `Connection: close` (from either side) as a signal to actually close rather than reuse.
4. A token-bucket rate limiter: requests are throttled to a configured maximum rate (e.g. N requests per second, with a documented burst-allowance policy), applied client-side before a request is even sent, not merely observed after the fact.
5. Retry with jitter: a request that fails with a retryable error (e.g. connection reset, a 5xx response if configured to treat those as retryable) is retried up to a configured maximum attempt count, with exponential backoff *and* randomized jitter between attempts (to avoid many clients retrying in lockstep against a struggling server — the "thundering herd" problem).
6. Correct handling of a response body that arrives across multiple `recv()` calls (partial reads), and of multiple responses on a reused keep-alive connection arriving back-to-back.

## Input

An HTTP request specification (method, URL, headers, body); client-wide configuration (rate limit, retry policy, connection pool size limits).

## Output

A parsed HTTP response (status, headers, body) or a distinguishable error/failure after exhausting retries.

## Constraints

- C++20. No third-party HTTP library (e.g. no libcurl, no cpp-httplib) for the core request/response/parsing/pooling logic — sockets and parsing are written by you, since that's the point; a lower-level portable sockets wrapper reused from earlier chapters/projects is fine.
- The rate limiter must actually delay/block requests exceeding the configured rate (not merely count them for reporting) — verified by measuring actual request timing under a burst of requests exceeding the configured limit.
- Retry logic must not retry non-idempotent requests (e.g. POST) by default unless explicitly configured to allow it — a documented, deliberate safety default, not an oversight.

## Edge Cases

- A server response that never sends a final `Content-Length`-satisfying amount of body nor a proper chunked terminator (a malformed or truncated response) — must be detected as an error, not silently returned as if it were a complete, valid response.
- A keep-alive connection that the server has actually closed (or that has gone stale/idle for a long time) being reused for a new request — the reuse attempt must detect the failure and transparently establish a fresh connection rather than surfacing a confusing low-level socket error to the caller.
- Multiple concurrent requests from the caller's code targeting the same host — the connection pool must correctly hand out either a genuinely idle existing connection or establish a new one, never handing the same connection to two concurrently in-flight requests.
- A burst of requests arriving well beyond the token bucket's capacity all at once — later requests in the burst must be delayed according to the documented policy, not dropped silently or allowed through unthrottled.
- A response header line that's abnormally long or a response with an implausible number of headers — bounded per a documented sanity limit, not permitted to drive unbounded parsing-buffer growth.

## Error Handling

- A connection attempt that fails outright (host unreachable, connection refused) — reported as a distinguishable error, and, if retries are configured, subject to the same retry-with-jitter policy as an in-flight request failure.
- Retry exhaustion (all configured attempts failed) — reported as a final, clearly-distinguishable failure carrying enough information about the last attempt's failure to be useful for the caller.

## Acceptance Criteria

- Chunked transfer encoding is correctly decoded against a response using multiple chunks of varying sizes, verified against the actual (unchunked) intended body content.
- Connection reuse is demonstrated: sending two requests to the same host and observing (via instrumentation, e.g. counting actual TCP connection establishments) that only one connection was created.
- The rate limiter's throttling is demonstrated: a burst of requests exceeding the configured rate shows measurably spaced-out actual send times matching the configured rate, not all firing immediately.
- Retry-with-jitter is demonstrated against a test server configured to fail the first N attempts and succeed on attempt N+1, with observed retry delays showing both growth and non-identical (jittered) spacing across repeated test runs.
- A stale/closed reused connection is transparently recovered from (a fresh connection established) without the caller seeing a raw socket-level error.

## Testing Requirements

- Request construction and response parsing correctness tests (status line, headers, `Content-Length`-framed body).
- The chunked transfer encoding decode test with multiple chunk sizes.
- The connection-pooling reuse-count test.
- The rate-limiter timing test.
- The retry-with-jitter test against a controllable test server.
- The stale-connection transparent-recovery test.
- The malformed/truncated-response detection test.

## Hints

### Hint 1 — Direction
Build the HTTP response parser as a pure, socket-independent state machine first (feed it byte chunks directly in unit tests, exactly as [P-5.3](../concurrent-tcp-protocol-server/STATEMENT.md)'s framing parser was built and tested) — status line, then headers, then body (dispatching to either `Content-Length`-based or chunked decoding depending on which header was present) — before wiring it up to real sockets.

### Hint 2 — Technique
Model the connection pool as a map from `host:port` to a set of currently-idle, previously-used connections plus a count of currently-checked-out ones; a request "checks out" a connection (creating a fresh one if none idle are available and the pool isn't at its per-host limit), uses it, and either "checks it back in" as idle (if the response indicated the connection should stay open) or discards it (if `Connection: close` was signaled, or if the request/response cycle failed). This checkout/checkin discipline is what prevents two concurrent requests from ever being handed the same connection.

### Hint 3 — Implementation
A token-bucket rate limiter's core state is just two numbers: current token count and last-refill timestamp. On each request attempt, first compute how many tokens should have accumulated since the last refill (elapsed time × configured rate, capped at the bucket's maximum capacity — this is the "burst allowance"), add them, then either consume one token immediately (if at least one is available) or compute and wait exactly the delay needed until one more token would become available.

### Hint 4 — Debugging/Design
If your stale-connection-reuse test doesn't correctly recover and instead surfaces a raw error to the caller, check where you're detecting the staleness — a connection closed by the server some time ago will not necessarily fail immediately on your next `send()`; often the failure only appears on the subsequent `recv()` (or as a `send()` failure only after enough data triggers a TCP-level reset response), so recovery logic needs to catch failures at whichever specific step actually surfaces them and retry with a fresh connection from there, not assume the failure will conveniently appear on the very first operation against the stale connection.
