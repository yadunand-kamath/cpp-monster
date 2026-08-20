# P-5.3 — Solution

## Reference Architecture

The framing state machine, tested independently of networking per Hint 1:

```cpp
class FrameParser {
public:
    explicit FrameParser(std::size_t max_message_size) : max_message_size_(max_message_size) {}

    void feed(std::string_view bytes) { buffer_.append(bytes); }

    std::optional<std::string> next_frame() {
        if (state_ == State::kReadingLength) {
            if (buffer_.size() < 4) return std::nullopt;
            std::uint32_t length = decode_be_uint32(buffer_.data());
            if (length > max_message_size_) throw FrameProtocolError("message exceeds configured maximum"); // Hint 3
            pending_length_ = length;
            buffer_.erase(0, 4);
            state_ = State::kReadingPayload;
        }
        if (buffer_.size() < pending_length_) return std::nullopt; // partial payload — wait for more
        std::string frame = buffer_.substr(0, pending_length_);
        buffer_.erase(0, pending_length_);
        state_ = State::kReadingLength;
        return frame;
    }
private:
    enum class State { kReadingLength, kReadingPayload };
    std::string buffer_;
    std::size_t max_message_size_, pending_length_ = 0;
    State state_ = State::kReadingLength;
};
```

Per-connection state, showing the three independent pieces from Hint 2:

```cpp
struct Connection {
    FrameParser incoming{config.max_message_size};
    std::deque<std::byte> outgoing_queue;
    std::size_t queued_bytes = 0;
    std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();

    bool try_queue_write(std::span<const std::byte> data, const ServerConfig& config) {
        if (queued_bytes + data.size() > config.max_queued_write_bytes) {
            return false; // caller applies the configured backpressure policy — Hint 4
        }
        outgoing_queue.insert(outgoing_queue.end(), data.begin(), data.end());
        queued_bytes += data.size();
        return true;
    }
};

void Server::on_readable(ConnectionId id) {
    auto& conn = connections_.at(id);
    std::byte buf[4096];
    std::size_t n = socket_read(id, buf); // however many bytes happened to be available
    conn.last_activity = std::chrono::steady_clock::now();
    conn.incoming.feed(std::string_view(reinterpret_cast<char*>(buf), n));
    while (auto frame = conn.incoming.next_frame()) {
        auto response = handle_request(*frame); // dispatched to thread pool per P-4.3, if configured
        if (!conn.try_queue_write(response, config_)) {
            apply_backpressure_policy(id, config_.backpressure_policy); // drop or disconnect
        } else {
            event_loop_.watch_writable(id);
        }
    }
}
```

## Design Rationale

**Why does `FrameParser::next_frame()` check the length against `max_message_size_` immediately after decoding the 4-byte prefix, before transitioning to the payload-reading state?** This is the structural fix Hint 3 points at directly: if the check happened only after accumulating a payload-sized buffer, the "structurally impossible" claim in Acceptance Criteria wouldn't hold — a malicious length would already have driven an allocation attempt by the time it was rejected. Checking before any payload-sized buffer exists means the rejection path's memory cost is bounded by the fixed-size length prefix alone, regardless of how large a value that prefix claims.

**Why track `queued_bytes` as an explicit counter on `Connection` rather than querying the OS socket's send-buffer state?** The OS send buffer reflects the kernel's own (typically small, and not fully controllable per-write) buffering, not the data this server has decided to accept responsibility for sending. The actual unbounded-memory-growth risk described in Constraints is in *this process's own* queued-but-not-yet-written data — measuring and bounding that directly, independent of the kernel's behavior, is what makes the backpressure policy meaningful and testable in isolation.

**Why does `on_readable` loop calling `next_frame()` until it returns nothing, rather than processing at most one frame per readiness notification?** A single `read()` can and does return multiple complete messages' worth of bytes in one call (Edge Cases explicitly requires this to be handled) — processing only one frame per notification would leave already-received, already-parsed frames sitting unprocessed until some later spurious readiness notification arrived to "unstick" them, silently adding latency and complexity for no benefit over draining everything that's actually ready right now.

## Reference Implementation

The above covers frame parsing, per-connection state, and the read-path dispatch loop. Remaining work for the learner: the write-path (draining `outgoing_queue` on write-readiness notifications, handling partial writes transparently), idle/partial-frame timeout sweeping (a periodic timer registered with the event loop, checking `last_activity` across connections), the graceful-vs-immediate shutdown modes, connection-count-limit enforcement at accept time, and the load-testing client with percentile latency reporting.

## Testing Strategy

Reproduce the 10,000-connection scale test locally with explicit attention to OS-level limits (documented directly in this project's setup instructions, not left implicit) before attributing any failure at that scale to the server's own logic — Hint 4's environment-configuration point applies to your own development loop, not just to future readers.

## Performance Analysis

Per-connection memory overhead (the framing buffer, the outgoing queue, and fixed metadata) times the connection-count target is the dominant memory-scaling concern at 10,000 concurrent connections — this should be measured directly (not assumed) and reported, since it's the practical ceiling on how far this design scales before the event-loop model's other costs (fd/handle table size, epoll/IOCP internal bookkeeping) become the limiting factor instead.

## Failure Modes

- Accumulating payload bytes into a buffer sized by the claimed length prefix *before* validating that length against the configured maximum, silently reintroducing the unbounded-allocation vulnerability the design is supposed to structurally prevent.
- A backpressure policy that's checked only occasionally (e.g. on a periodic sweep) rather than at the moment each write is queued, allowing a burst of writes between sweeps to blow well past the configured bound before the policy has a chance to engage.
- Reusing one shared buffer across connections for reads (an optimization that seems reasonable for reducing allocations) without recognizing that doing so incorrectly ties one connection's read timing to another's if not handled with extreme care — safer to keep buffers genuinely per-connection unless a more sophisticated pooling scheme is deliberately designed and tested.

## Extensions

- TLS support via a pluggable transport layer beneath the framing logic, keeping the framing state machine itself transport-agnostic.
- Integrating [P-5.2](../coroutine-task-generator-library/STATEMENT.md)'s `task<T>` so request handlers can be written as coroutines awaiting further I/O, rather than purely synchronous callback-style handlers.
