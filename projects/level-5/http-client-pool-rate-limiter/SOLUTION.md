# P-5.5 — Solution

## Reference Architecture

The response parser's chunked-decoding branch, the part most prone to off-by-one errors:

```cpp
std::optional<std::string> HttpResponseParser::decode_chunked_body() {
    std::string body;
    while (true) {
        auto line_end = buffer_.find("\r\n", cursor_);
        if (line_end == std::string::npos) return std::nullopt; // chunk-size line not fully received yet
        std::size_t chunk_size = std::stoul(buffer_.substr(cursor_, line_end - cursor_), nullptr, 16);
        std::size_t chunk_data_start = line_end + 2;
        if (chunk_size == 0) {
            if (buffer_.size() < chunk_data_start + 2) return std::nullopt; // final CRLF not yet received
            cursor_ = chunk_data_start + 2;
            return body; // terminating zero-length chunk — decoding complete
        }
        if (buffer_.size() < chunk_data_start + chunk_size + 2) return std::nullopt; // chunk data incomplete
        body.append(buffer_, chunk_data_start, chunk_size);
        cursor_ = chunk_data_start + chunk_size + 2; // skip chunk data + its trailing CRLF
    }
}
```

The connection pool's checkout/checkin discipline (Hint 2):

```cpp
class ConnectionPool {
public:
    std::unique_ptr<Connection> checkout(const std::string& host, std::uint16_t port) {
        std::lock_guard lock(mutex_);
        auto& idle_list = idle_connections_[{host, port}];
        if (!idle_list.empty()) {
            auto conn = std::move(idle_list.back());
            idle_list.pop_back();
            return conn; // reused — never simultaneously reachable from the idle list and a caller
        }
        return std::make_unique<Connection>(host, port); // fresh connection — Acceptance Criteria's reuse-count test
    }

    void checkin(std::unique_ptr<Connection> conn, bool keep_alive) {
        if (!keep_alive) return; // Connection: close honored — conn destructor closes the socket
        std::lock_guard lock(mutex_);
        idle_connections_[{conn->host(), conn->port()}].push_back(std::move(conn));
    }
private:
    std::mutex mutex_;
    std::map<std::pair<std::string, std::uint16_t>, std::vector<std::unique_ptr<Connection>>> idle_connections_;
};
```

The stale-connection recovery wrapper (Hint 4's "one fresh-connection retry, transparently" rule):

```cpp
HttpResponse HttpClient::send_request(const HttpRequest& request) {
    auto conn = pool_.checkout(request.host, request.port);
    bool from_pool = conn->was_reused();
    try {
        auto response = send_over_connection(*conn, request);
        pool_.checkin(std::move(conn), response.keep_alive);
        return response;
    } catch (const NetworkError&) {
        if (!from_pool) throw; // a fresh connection failing is a real error, not staleness
        auto fresh_conn = std::make_unique<Connection>(request.host, request.port);
        auto response = send_over_connection(*fresh_conn, request); // exactly one retry, per Hint 4
        pool_.checkin(std::move(fresh_conn), response.keep_alive);
        return response;
    }
}
```

## Design Rationale

**Why does `decode_chunked_body` return `std::nullopt` (rather than throwing or blocking) whenever it needs more bytes than are currently buffered?** This mirrors the framing-parser pattern from [P-5.3](../concurrent-tcp-protocol-server/SOLUTION.md) directly: the parser is a pure function of "bytes fed so far," entirely decoupled from where those bytes come from or when more will arrive. Returning "not enough yet" rather than blocking keeps the parser usable both in fast unit tests (feeding pre-chosen byte chunks) and in real socket-driven code (feeding whatever a `recv()` call happened to produce), which is exactly Hint 1's stated reason for building and testing it standalone first.

**Why does the stale-connection recovery logic retry only once, and only when the failed connection came from the pool (was reused)?** A connection freshly created for this very request failing indicates a real problem (server down, network unreachable) that a same-instant retry won't fix and shouldn't mask. A connection taken from the idle pool failing is much more likely to be exactly the staleness scenario this project's Edge Cases describes (the server silently closed it sometime after it was checked in) — retrying once with a guaranteed-fresh connection cleanly distinguishes "was this connection just old" from "is the server actually unreachable," without needing to precisely diagnose which OS-level call surfaced the failure, matching Hint 4's reasoning.

**Why is the token bucket's state only updated lazily, at `acquire()` call time, rather than via a background refill timer?** A background timer thread would need its own lifecycle management (start, stop, synchronization with `acquire()` calls) for no actual benefit — the bucket's fill level at any instant is fully and correctly determined by "how much time has elapsed since it was last checked," so computing that lazily on demand gives identical correctness with substantially less code and no extra thread.

## Reference Implementation

The above covers chunked decoding, pool checkout/checkin, and the stale-connection retry wrapper. Remaining work for the learner: the status-line/header parsing portion of `HttpResponseParser`, the `Content-Length`-framed body path (simpler than chunked, but still must handle partial reads), the token-bucket's `acquire()` method itself (per Hint 3's formula), the general retry-with-jitter policy wrapping `send_request` (distinguishing idempotent from non-idempotent methods per Constraints), and the header-count/header-line-length sanity limits.

## Testing Strategy

Build a small controllable test HTTP server (able to serve `Content-Length` and chunked responses, simulate a close-after-N-requests staleness scenario, and simulate failing-then-succeeding for retry tests) early — nearly every test in TESTS.md depends on some variant of this server, so getting its controllability right first avoids retrofitting test infrastructure mid-project, the same lesson [P-4.6](../../level-4/process-supervisor/SOLUTION.md)'s testing strategy drew for its helper child process.

## Performance Analysis

Connection reuse's benefit is avoiding TCP handshake (and, if TLS were in scope, handshake) cost per request — measurable directly by comparing per-request latency with pooling enabled versus disabled against the same test server. The rate limiter's overhead per `acquire()` call should be negligible (a few arithmetic operations under a lock) compared to the actual network I/O each request performs, so it should not become a bottleneck itself even at high configured rates.

## Failure Modes

- A chunked decoder that doesn't account for the trailing `\r\n` after each chunk's data (only after the chunk-size line), producing an off-by-two error that silently corrupts the decoded body's chunk boundaries.
- A connection pool where checkin is called even when the response indicated `Connection: close`, causing a request against that (now server-closed) connection to fail unnecessarily — checkin must respect the keep-alive signal, not add every used connection back unconditionally.
- Retrying a POST request by default, silently violating the idempotency safety default and potentially causing a duplicate side-effecting operation on the server if the original request had actually succeeded but its response was merely lost.

## Extensions

- HTTP/1.1 pipelining (sending multiple requests before reading their responses) as a further throughput optimization, with its own correctness subtleties around response ordering.
- TLS support via a pluggable transport layer, mirroring the extension noted in [P-5.3](../concurrent-tcp-protocol-server/SOLUTION.md).
