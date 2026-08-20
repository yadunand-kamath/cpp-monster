# P-3.7 — Hints

### Hint 1 — Direction
Get a single-client, single-request round trip fully working end to end (connect → send one small framed message → receive the framed response → close) before adding a second client, a second request on the same connection, or any edge case at all. Sockets have enough platform-specific ceremony (`WSAStartup`, address structures, byte-order conversion for the length prefix) that you want that ceremony proven correct in the simplest possible scenario first.

### Hint 2 — Technique
Network byte order (big-endian) versus host byte order matters for the length prefix specifically — `htonl`/`ntohl` (or their Winsock equivalents, which have the same names) convert between them. Skipping this works by accident on nothing (both a same-machine test and a real cross-machine deployment would break identically on a little-endian host if the prefix is written in host order), so it's worth getting right from the first line of framing code rather than treating it as a later fix.

### Hint 3 — Implementation
Thread-per-connection means the accept loop's only job is: accept a connection, hand the resulting socket to a newly spawned thread running the request-response loop, and go back to accepting. Keep the per-connection thread's lifetime entirely self-contained (it owns its socket, it exits when the socket closes) so the server doesn't need any shared mutable state between connection threads for this project's scope — that shared-state problem is what P-4.x's concurrency-focused projects are for.

### Hint 4 — Debugging/Design
If concurrent clients occasionally see garbled or cross-connection data, check whether any buffer, socket handle, or other per-connection state is accidentally being captured by reference into a thread lambda from a loop variable, or otherwise shared across connection threads instead of being distinct per connection — a length-prefixed protocol implemented correctly in isolation will still corrupt under concurrency if two connections' request-handling logic quietly share a buffer.
