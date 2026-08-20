# P-3.7 — Solution

## Reference Architecture

A thin `Socket` RAII wrapper (owns one native handle, closes on destruction) with platform-specific `.cpp`s behind one header; `send_framed`/`recv_framed` free functions operating on that wrapper; a `Server` that accepts and spawns a detached-but-tracked thread per connection; a `Client` that connects once and exposes a blocking `send_request`.

```cpp
class Socket {
public:
    explicit Socket(NativeHandle h) : handle_(h) {}
    Socket(Socket&& other) noexcept : handle_(std::exchange(other.handle_, kInvalid)) {}
    ~Socket() { if (handle_ != kInvalid) platform_close(handle_); }
    NativeHandle native() const { return handle_; }
private:
    NativeHandle handle_;
};

// Loops until the full length is read or the peer closes — a single recv()
// call is never assumed to deliver the whole prefix or the whole payload.
std::optional<std::string> recv_framed(const Socket& s, std::uint32_t max_frame_bytes) {
    std::uint32_t net_len;
    if (!recv_exact(s, &net_len, sizeof(net_len))) return std::nullopt;  // peer closed before header
    std::uint32_t len = ntohl(net_len);
    if (len > max_frame_bytes) return std::nullopt;                     // oversized — reject, don't allocate
    std::string payload(len, '\0');
    if (!recv_exact(s, payload.data(), len)) return std::nullopt;       // peer closed mid-payload
    return payload;
}
```

The server's accept loop:

```cpp
void Server::run() {
    while (!stop_requested_) {
        Socket client = accept_connection(listen_socket_);
        std::thread([this, client = std::move(client)]() mutable {
            while (auto request = recv_framed(client, kMaxFrameBytes)) {
                std::string response = handle_request(*request);  // e.g. uppercase
                if (!send_framed(client, response)) break;         // client gone; exit thread
            }
        }).detach();
    }
}
```

## Design Rationale

**Why a thread per connection instead of the event-loop model this workbook builds in P-4.1?** This project's job is to isolate the socket/framing/partial-I/O learning curve from the event-loop learning curve — conflating both in one project would make it hard to tell, when something breaks, which of the two unfamiliar techniques caused it. Thread-per-connection is also a completely legitimate real design for the connection-count range this project targets (dozens, not thousands); its ceiling is exactly why P-5.3 later revisits the same framing problem on an event loop when the target becomes 10,000 connections.

**Why reject an oversized length prefix before allocating any buffer, rather than trying to allocate and letting it fail?** A malicious or corrupted prefix claiming `0xFFFFFFFF` bytes would otherwise drive an allocation request large enough to be a denial-of-service vector on its own (or to throw `std::bad_alloc` in a way that's easy to forget to handle per-connection) — checking the claimed length against a documented ceiling *before* touching the allocator turns an attacker-controlled allocation size into a simple, cheap comparison.

**Why does `recv_exact` distinguish "0 bytes returned" from "fewer bytes than requested but more than 0"?** These mean fundamentally different things on a TCP socket: 0 means the peer has closed its end and no more data will ever arrive on this connection; a positive-but-short read just means the rest hasn't arrived on the wire yet and another `recv()` call will get more. Treating them the same way is the single most common bug in a hand-rolled length-prefixed protocol, and it's exactly Hint 4's warning.

## Reference Implementation

The above covers the framing primitives and the accept-loop shape. Remaining work for the learner: the platform-specific `Socket` construction/connect/listen/accept functions (POSIX `socket`/`bind`/`listen`/`accept` vs. Winsock's `WSAStartup`-gated equivalents), `send_framed`'s mirror-image loop over `send()`, the `Client` class wrapping one connection with a blocking `send_request`, and the truncated-connection/oversized-prefix observability hooks the hidden tests rely on (e.g. a simple atomic counter incremented at each rejection point, exposed for tests to assert against).

## Testing Strategy

Test the framing primitives (`send_framed`/`recv_framed`) against a loopback connection or a socket pair in isolation before layering the accept loop and threading on top — this mirrors Hint 1's sequencing and means a framing bug and a concurrency bug can never be confused with each other during debugging.

## Performance Analysis

Not the focus of this project (that's P-5.3's job) — thread-per-connection's cost model is dominated by per-thread stack memory and OS scheduling overhead, both of which scale linearly and become the documented ceiling well before 10,000 connections; no optimization of that ceiling is expected here.

## Failure Modes

- Confusing `recv()` returning 0 (clean close) with a short read (more data pending) — the classic bug this project exists to surface before it's buried under event-loop complexity at P-5.3.
- Allocating a buffer sized directly from an unvalidated length prefix, reopening the same unbounded-allocation risk the oversized-prefix rejection exists to close.
- Forgetting `WSAStartup`/`WSACleanup` pairing on Windows, which can cause the first socket call to fail in a way that's easy to misdiagnose as a firewall or permissions issue.

## Extensions

- Add a configurable per-connection idle timeout, closing connections that haven't sent a request in N seconds — a natural bridge toward the backpressure/resource-bounding concerns P-5.3 makes a hard requirement.
- Swap the uppercase transform for a small structured request/response format (e.g. a tagged union of a few request kinds) as a warm-up for P-5.5's HTTP request/response parsing.
