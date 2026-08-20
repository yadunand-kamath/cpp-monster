# P-3.7 — Length-Prefixed TCP Request-Response Service

**Level:** 3 (Realistic utility) · **Category:** Networking · **Requires:** Ch01–09 · **Est. effort:** M (8-16h)

## Objective

Build a small TCP client and server, using plain blocking sockets (BSD sockets on Linux, Winsock on Windows) behind one shared interface, that exchange length-prefixed binary messages and support multiple concurrent clients via a thread-per-connection model. This is the on-ramp to the workbook's later networking projects: [P-5.3](../../level-5/concurrent-tcp-protocol-server/STATEMENT.md) revisits the same length-prefixed framing problem at 10,000-connection scale on a non-blocking event loop, and [P-5.5](../../level-5/http-client-pool-rate-limiter/STATEMENT.md) revisits partial-read/partial-write handling for a text protocol — this project's job is to make sure those harder problems aren't also your first exposure to sockets at all.

## Functional Requirements

1. A server that listens on a TCP port, accepts multiple concurrent client connections, and services each on its own thread (deliberately not the event-loop model — that arrives at P-4.1/P-5.3 as a distinct, harder technique).
2. A length-prefixed binary framing protocol (e.g. a 4-byte big-endian length prefix followed by that many payload bytes) shared by both client and server, correctly reassembling a complete frame regardless of how many `recv()` calls it takes to arrive.
3. A simple request-response behavior: the server reads one framed request and writes back one framed response (an echo, or a trivial transformation such as uppercasing the payload — pick one and document it) before reading the next request on that connection.
4. A client library function that connects, sends one framed request, and blocks for the framed response, usable both interactively and from a test.
5. One shared socket-handling interface (connect, send-framed, receive-framed, close) used by both platforms — Linux (`socket`/`connect`/`bind`/`listen`/`accept`/`send`/`recv`) and Windows (Winsock equivalents, including `WSAStartup`/`WSACleanup`), per Ch09's paired-platform material.
6. Graceful connection close: either side closing its end is detected by the other as a clean "connection closed" condition (a `recv()` returning 0), distinguished from an error.

## Input

Requests: framed binary payloads sent by a client over an established TCP connection.

## Output

Responses: framed binary payloads sent back by the server over the same connection.

## Constraints

- C++20, blocking sockets only — no non-blocking I/O, no `epoll`/`IOCP`/event loop (that's P-4.1's distinct technique). Thread-per-connection is the concurrency model here, and its limits (thread count ceiling well below P-5.3's 10,000-connection target) are expected and should be stated, not solved.
- Must build and run correctly on both Linux and Windows behind the one shared socket interface from Functional Requirement 5.
- Partial reads and partial writes are normal at this layer exactly as they are at every later networking project — a single `recv()`/`send()` completing with fewer bytes than requested must be handled correctly, not treated as an edge case.

## Edge Cases

- A client that sends a length prefix but disconnects before sending the full payload — the server must detect this as a truncated message (connection closed mid-frame), not block forever or misinterpret a partial payload as complete.
- A length prefix claiming an implausibly large payload size (e.g. larger than any reasonable message, possibly from a corrupted or malicious client) — rejected with a documented maximum frame size rather than attempting to allocate an unbounded buffer.
- Two clients connected simultaneously, each sending requests independently — each connection's thread must handle its own client without interference from the other.
- A client that connects and immediately disconnects without sending anything — the server must detect and clean up this connection without hanging.

## Error Handling

- `connect()` failing (server not listening, wrong port, refused) — reported to the caller as a clear, distinguishable connection error, not a generic failure or a hang.
- A malformed or oversized length prefix — the server closes that connection with a documented reason rather than crashing or corrupting handling of other connections.

## Acceptance Criteria

- A client can connect, send a request, and receive the correct framed response, verified for at least three different payload sizes (including one large enough to require multiple `recv()` calls).
- At least 20 clients connected concurrently, each completing several request-response round-trips correctly with no cross-talk between connections.
- The truncated-message and oversized-length-prefix edge cases are each demonstrated with a specific test.
- The implementation builds and passes its test suite on both Linux and Windows.

## Testing Requirements

- Single-client round-trip correctness across multiple payload sizes, including a payload deliberately larger than one `recv()` buffer's worth of bytes.
- The concurrent-clients test (at least 20 connections) with correctness verified per-connection.
- The truncated-message-mid-frame test (client disconnects after sending only the length prefix, or only part of the payload).
- The oversized-length-prefix rejection test.
- A clean-shutdown test: the server can be stopped while connections are active without leaving the process hung or connections silently dropped without the documented close behavior.

## Hints

### Hint 1 — Direction
Build the framing helpers (`send_framed`, `recv_framed`) as free functions operating on a raw socket handle, and get them fully correct and unit-testable in isolation (e.g. against a socket pair or a loopback connection) before writing any concurrency or accept-loop code — the length-prefix framing problem and the thread-per-connection problem are two separate concerns, and conflating them while debugging makes it hard to tell which one a bug belongs to.

### Hint 2 — Technique
`recv()` (and Winsock's equivalent) makes no promise about how many bytes it returns on a single call short of the requested buffer size — a correct `recv_framed` must loop, accumulating bytes into a buffer, until either the full expected length has been read or the connection closes. The same is true in reverse for `send()`: a single call is not guaranteed to send the entire buffer, and a correct `send_framed` must loop as well.

### Hint 3 — Implementation
For the shared cross-platform socket interface, the cleanest boundary is usually a thin wrapper type around the platform's native socket handle (`int` on POSIX, `SOCKET` on Windows) exposing only the operations this project needs (connect, send, recv, close) with platform-specific code confined entirely to that wrapper's `.cpp` — everything above it (framing, request handling, the accept loop) can then be written once against the wrapper's interface with no `#ifdef`s.

### Hint 4 — Debugging/Design
If the truncated-message test hangs instead of correctly detecting a closed connection, check whether your `recv_framed` loop distinguishes `recv()` returning 0 (peer closed cleanly) from `recv()` returning a positive byte count less than requested (more data is still coming, keep looping) — conflating these two is the most common way a length-prefixed protocol implementation ends up blocking forever on a connection that will never send more data.
