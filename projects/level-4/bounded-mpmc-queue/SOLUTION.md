# P-4.2 — Solution

## Reference Architecture

Lock-based variant (the correctness/throughput baseline):

```cpp
template <typename T>
class LockBasedQueue {
public:
    explicit LockBasedQueue(std::size_t capacity) : buf_(capacity) {}

    bool try_push(T v) {
        std::lock_guard lock(mu_);
        if (closed_ || count_ == buf_.size()) return false;
        buf_[(head_ + count_) % buf_.size()] = std::move(v);
        ++count_;
        not_empty_.notify_one();
        return true;
    }

    bool push(T v) { // blocks until space or closed
        std::unique_lock lock(mu_);
        not_full_.wait(lock, [&] { return closed_ || count_ < buf_.size(); });
        if (closed_) return false;
        buf_[(head_ + count_) % buf_.size()] = std::move(v);
        ++count_;
        not_empty_.notify_one();
        return true;
    }

    void close() {
        std::lock_guard lock(mu_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
private:
    std::mutex mu_;
    std::condition_variable not_empty_, not_full_;
    std::vector<T> buf_;
    std::size_t head_ = 0, count_ = 0;
    bool closed_ = false;
};
```

Lock-free variant, following the Vyukov per-slot-sequence-number design (Hint 2/3), showing the acquire/release pairing that is the entire correctness crux:

```cpp
template <typename T>
class LockFreeQueue {
    struct Slot {
        std::atomic<std::size_t> sequence;
        T data;
    };
public:
    explicit LockFreeQueue(std::size_t capacity) : slots_(capacity) {
        for (std::size_t i = 0; i < capacity; ++i)
            slots_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool try_push(T v) {
        std::size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = slots_[pos % slots_.size()];
            std::size_t seq = slot.sequence.load(std::memory_order_acquire);
            std::intptr_t diff = (std::intptr_t)seq - (std::intptr_t)pos;
            if (diff == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break; // this producer now owns the slot
            } else if (diff < 0) {
                return false; // queue full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed); // another producer moved ahead
            }
        }
        Slot& slot = slots_[pos % slots_.size()];
        slot.data = std::move(v);
        slot.sequence.store(pos + 1, std::memory_order_release); // publish: data visible before this
        return true;
    }
    // try_pop mirrors try_push, claiming a slot whose sequence == pos+1 and
    // releasing it back to sequence == pos + slots_.size() for reuse.
private:
    std::vector<Slot> slots_;
    std::atomic<std::size_t> enqueue_pos_{0}, dequeue_pos_{0};
};
```

## Design Rationale

**Why does `try_push` load a slot's `sequence` with `acquire` but store it with `release`, rather than using `relaxed` throughout for speed?** The `release` store after writing `slot.data` is what guarantees any thread that subsequently `acquire`-loads that same sequence value observes the write to `slot.data` that happened-before it — this acquire/release pairing is the *entire* mechanism establishing a happens-before relationship between one thread's write and another thread's read of the same memory. Using `relaxed` for either half would remove that guarantee, allowing a consumer to observe the updated sequence number (meaning "data is ready") while still seeing stale or partially-written data in the slot — a real, if intermittent, torn-read bug that a single-threaded or lightly-loaded test would likely never surface.

**Why compare-exchange on `enqueue_pos_` with `relaxed` ordering, given the argument above?** `enqueue_pos_` itself does not carry any *data* that needs to be published to another thread — it's purely a coordination counter deciding *which* producer gets *which* slot index. The actual data-visibility guarantee is carried entirely by the per-slot `sequence` field's acquire/release pair, so `enqueue_pos_`'s own ordering can stay relaxed without weakening correctness — a good illustration of why "use `seq_cst` everywhere to be safe" is not the same reasoning as "understand exactly which specific ordering constraint each atomic op needs to enforce."

**Why validate the lock-free variant against a lock-based baseline built first, rather than trusting the lock-free stress test in isolation?** A stress test can pass "by accident" if it happens not to exercise the exact race window a subtle bug depends on — having a trusted-correct baseline lets you compare behavior (checksums, counts, timing) under identical concurrent workloads, isolating "is my test harness itself sound" from "is my lock-free implementation correct," which is exactly the risk Hint 1 flags.

## Reference Implementation

The above covers the lock-based queue in full and the lock-free `try_push`'s complete claim-then-publish logic, including the ordering rationale that is this project's central learning objective. Remaining work for the learner: `try_pop`'s mirrored slot-claiming logic (claiming a slot whose sequence equals `pos + 1`, then releasing it back to `pos + capacity` so it becomes available for a future wraparound push), the blocking `push`/`pop` forms for the lock-free variant (typically implemented via a brief spin-then-park-on-a-futex/condition-variable hybrid, since a purely lock-free queue has no natural blocking primitive of its own), and `close()`'s interaction with blocked waiters in the lock-free variant.

## Testing Strategy

Run the identical stress-test workload (same producer/consumer counts, same item counts, same checksum verification) against both variants — this maximizes the value of the lock-based baseline as a correctness cross-check, and makes the throughput benchmark's comparison meaningful since both variants were exercised identically.

## Performance Analysis

The lock-based variant's throughput is bounded by mutex acquisition and condition-variable wake latency under contention; the lock-free variant avoids blocking syscalls on the hot path entirely but pays for compare-exchange retry loops under high contention on the shared position counters. Report actual measured numbers per the Acceptance Criteria rather than assuming the lock-free variant wins — at high producer/consumer counts with a small queue capacity, contention on the shared `enqueue_pos_`/`dequeue_pos_` atomics can produce retry storms that are not obviously faster than a well-implemented mutex.

## Failure Modes

- Using `relaxed` ordering on the per-slot sequence field, producing intermittent, hard-to-reproduce data corruption that light testing won't reveal.
- A `close()` implementation that doesn't reliably wake threads blocked in the lock-free variant's blocking `push`/`pop`, leaving them stuck forever.
- Trusting a passing stress test on its own as proof of lock-free correctness, without also running under ThreadSanitizer — many real lock-free bugs are timing-dependent enough to pass most naive stress runs.

## Extensions

- A `try_push`/`try_pop` variant returning richer diagnostic information (e.g. "failed due to full" vs "failed due to closed") rather than a single boolean/optional.
- Feeding this queue directly into [P-4.3](../work-stealing-thread-pool/STATEMENT.md)'s thread pool as its task submission channel.
