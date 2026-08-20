# P-5.3 — Concurrent TCP Protocol Server

**Level:** 5 (Production-style) · **Category:** Networking · **Requires:** Ch01–11 · **Est. effort:** XL (24-32h)

## Objective

Build a TCP server for a simple length-prefixed binary protocol that handles thousands of concurrent connections with backpressure-aware writes and measured tail latency — built on [P-4.1](../../level-4/event-loop/STATEMENT.md)'s event loop and, ideally, [P-4.3](../../level-4/work-stealing-thread-pool/STATEMENT.md)'s thread pool for request processing.

This project is structured in phases. Each phase has its own exit bar; don't move on until the current one is met.

## Phase 1 — Single-Connection Framing

Get the length-prefixed framing protocol correct against one connection at a time, including partial reads/writes at arbitrary chunk boundaries. Exit bar: a test feeds the parser a message split at every possible byte offset and confirms correct reassembly in every case.

## Phase 2 — Concurrency at Scale

Move onto the event loop and handle many connections concurrently, reaching the 10,000-connection target. Exit bar: a load-testing client sustains 10,000 concurrent connections against the server with no crashes, no connection drops attributable to the server, and no unbounded memory growth over a sustained run.

## Phase 3 — Backpressure

Add the bounded per-connection write-queue policy for slow/malicious clients. Exit bar: a deliberately slow-reading test client causes the server to apply its documented drop-or-disconnect policy rather than growing memory unboundedly — demonstrated with a memory ceiling that holds under a stress test with several such slow clients.

## Phase 4 — Shutdown Semantics

Implement and distinguish graceful vs. immediate shutdown. Exit bar: graceful shutdown under load lets in-flight requests complete and closes cleanly with no dropped in-flight data; immediate shutdown terminates promptly regardless of in-flight state.

## Phase 5 — Measured Tail Latency

Build the load-testing client and report p50/p99/p999 latency under a realistic concurrent load profile. Exit bar: a documented latency report at the 10,000-connection scale, with methodology, and at least one case where a design decision (e.g. thread-pool sizing) is justified by its effect on p99.

## Functional Requirements

1. A length-prefixed binary framing protocol (e.g. 4-byte big-endian length prefix followed by that many payload bytes) — correctly reassembling complete frames from a TCP byte stream, which delivers data in arbitrary chunk boundaries unrelated to message boundaries.
2. Handle at least 10,000 concurrent client connections on a single server process (a scale target, not merely "works with a handful of connections") using the event-loop's readiness/completion-based I/O rather than a thread-per-connection model.
3. Backpressure-aware writes: when a client's socket send buffer is full (a slow or malicious client not reading fast enough), the server must not unboundedly buffer outgoing data in process memory for that connection — apply a documented policy (bounded per-connection write queue with a drop or disconnect policy once exceeded) rather than allowing one slow client to consume unbounded server memory.
4. Correctly handle partial reads and partial writes at every layer — a single `read()`/`recv()` or `write()`/`send()` call completing with fewer bytes than requested is normal, expected behavior, not an error condition, and the framing/writing logic must handle it transparently.
5. Graceful and immediate shutdown modes distinguishable by the operator: graceful (stop accepting new connections, allow in-flight requests to complete, then close), immediate (close everything now).
6. Measured p50/p99/p999 request latency under load, reported by an included load-testing/benchmark client.

## Input

TCP connections from clients sending length-prefixed binary frames; a server configuration (listen address/port, backpressure queue limits, worker thread count).

## Output

Length-prefixed binary response frames per the protocol's request/response semantics (a simple echo or transform is sufficient — protocol richness is not this project's point); reported latency percentiles from the load-test client.

## Constraints

- C++20, building on [P-4.1](../../level-4/event-loop/STATEMENT.md)'s event loop for connection I/O (not a naive thread-per-connection model, which would not scale to the 10,000-connection target on typical hardware).
- Must not read an unbounded amount of data into memory before framing is complete for a single message (a length-prefix value from a misbehaving/malicious client claiming an enormous message size must be rejected, not trusted and used to drive an unbounded allocation).
- Backpressure policy must be explicit, configurable, and tested — not an accidental emergent behavior of whatever buffering happens to occur.

## Edge Cases

- A client that connects and sends nothing (idle connection) — must not consume disproportionate server resources merely by existing, and should be subject to a configurable idle timeout.
- A client that sends a partial frame and then never sends the rest — must not block other connections' processing (a fundamental requirement of the event-loop-based design, but explicitly worth testing) and should eventually time out per a documented policy.
- A malicious client sending a length-prefix value far exceeding any reasonable message size (e.g. claiming a 4GB message) — rejected cleanly, not causing an unbounded allocation attempt.
- The server reaching its configured maximum connection count — new connection attempts beyond that point handled per a documented policy (reject immediately vs. queue), not simply degrading unpredictably.
- A client disconnecting abruptly (TCP RST) mid-request — cleaned up correctly without corrupting other connections' state or leaking the closed connection's resources.

## Error Handling

- Malformed frame data (e.g. length prefix present but claimed length inconsistent with a protocol-level sanity bound) — the connection is cleanly closed with the error logged, not causing a crash or corrupting other connections.
- Bind failure (port already in use, insufficient permissions) at startup — a clear, immediate, actionable error rather than a silent failure to listen.

## Acceptance Criteria

- The server sustains 10,000 concurrent connections (using an appropriately configured OS file-descriptor/handle limit) under a load-testing client, processing a representative request rate without crashing or degrading catastrophically.
- The backpressure policy is demonstrated: an artificially slow-reading client causes the configured policy (bounded queue → drop or disconnect) to engage, verified by observing the server's behavior and resource usage under that condition, rather than unbounded memory growth.
- p50/p99/p999 latency numbers are reported from a real load-test run, with methodology (concurrency level, request rate, duration) documented.
- A malicious oversized-length-prefix frame is rejected without the server allocating memory proportional to the claimed (bogus) size.
- Both shutdown modes behave as specified under a live load test.

## Testing Requirements

- Protocol framing correctness tests: a message split across multiple `recv()` calls at arbitrary boundaries reassembles correctly; multiple messages arriving in a single `recv()` call are correctly split apart.
- The oversized-length-prefix rejection test.
- The backpressure-engagement test against an artificially slow client.
- The idle-connection and partial-frame timeout tests.
- The 10,000-connection scale test (may require OS-level file-descriptor/handle limit configuration, documented as a setup prerequisite).
- The load-test client's latency measurement, run and its output captured.

## Hints

### Hint 1 — Direction
Treat framing as a small, independent state machine per connection — "how many bytes of the length prefix have I received so far," then "how many bytes of the payload have I received so far" — entirely separate from the event loop's readiness notifications. The event loop just tells you "this socket is readable, go read what's available"; the framing state machine decides what to do with however many bytes that particular read happened to produce, which might be zero new complete frames, one, or several.

### Hint 2 — Technique
For backpressure, maintain a bounded per-connection outgoing byte queue; when a write would exceed that bound, apply your documented policy (e.g. drop the newest data and log it, or forcibly close the connection) rather than growing the queue further — and only re-register the socket for write-readiness with the event loop when there's actually data queued to send, avoiding a busy-loop of "writable but nothing to write" notifications.

### Hint 3 — Implementation
Reject an oversized length prefix immediately upon reading just the 4-byte prefix itself — before attempting to read any payload bytes at all — by comparing it against a configured maximum message size and closing the connection if it's exceeded; this way the "unbounded allocation from a malicious length value" failure mode is structurally impossible, since no buffer sized by that value is ever allocated in the rejection path.

### Hint 4 — Debugging/Design
If your 10,000-connection scale test fails with connection or file-descriptor exhaustion errors well before reaching 10,000, check your OS-level limits first (`ulimit -n` on Linux, the process handle-count behavior on Windows) — this is very often an environment configuration gap rather than a bug in your server, but you must document the required configuration explicitly as a setup prerequisite rather than silently assuming a default that most systems don't actually have.
