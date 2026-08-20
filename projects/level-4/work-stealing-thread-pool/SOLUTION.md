# P-4.3 — Solution

## Reference Architecture

```cpp
class ThreadPool {
public:
    explicit ThreadPool(std::size_t n_workers);

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        if (shutting_down_.load(std::memory_order_acquire))
            throw std::runtime_error("submit() called after shutdown");
        auto task = std::make_shared<std::packaged_task<R()>>(
            [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
                return f(args...);
            });
        auto fut = task->get_future();
        auto* worker = current_worker_or_least_loaded();
        worker->local_deque.push_top([task] { (*task)(); }); // exception captured by packaged_task
        return fut;
    }

    void request_stop() { stop_source_.request_stop(); }
    void shutdown(bool drain_pending);
    std::stop_token stop_token() const { return stop_source_.get_token(); }
private:
    struct Worker {
        WorkStealingDeque<std::function<void()>> local_deque;
        std::jthread thread;
    };
    std::vector<std::unique_ptr<Worker>> workers_;
    std::stop_source stop_source_;
    std::atomic<bool> shutting_down_{false};
};
```

Each worker's run loop, showing the "steal when own deque is empty, then park if no one has work" shape from Hint 1/4's cooperative-waiting principle:

```cpp
void worker_loop(Worker& self, std::span<std::unique_ptr<Worker>> all_workers, std::stop_token tok) {
    while (!tok.stop_requested()) {
        if (auto task = self.local_deque.pop_top()) { (*task)(); continue; }
        if (auto stolen = try_steal_from_any(all_workers, self)) { (*stolen)(); continue; }
        park_briefly(); // e.g. condition_variable wait with a short bound, not busy-spin
    }
}
```

The deque's steal-vs-owner-pop arbitration (structurally mirroring [P-4.2](../bounded-mpmc-queue/STATEMENT.md)'s slot-claiming compare-exchange):

```cpp
std::optional<T> WorkStealingDeque<T>::steal() {
    std::size_t b = bottom_.load(std::memory_order_acquire);
    std::size_t t = top_.load(std::memory_order_acquire);
    if (b >= t) return std::nullopt; // empty from the thief's view
    T item = buffer_[b % buffer_.size()]; // tentative read
    if (b + 1 == t) {
        // last item: race with the owner's own pop() — only one side may win
        if (!bottom_.compare_exchange_strong(b, b + 1, std::memory_order_seq_cst))
            return std::nullopt; // owner won the race
    } else {
        bottom_.store(b + 1, std::memory_order_relaxed);
    }
    return item;
}
```

## Design Rationale

**Why does the owner use `top_`/push-pop while thieves use `bottom_`/steal, rather than both sides sharing one end?** Having both sides contend for the same end would make every owner operation potentially contend with every thief operation — the entire performance argument for work-stealing over a shared-queue design (which this project explicitly rules out in Constraints) depends on the owner's hot path being normally uncontended. Splitting the ends means the owner only pays a synchronization cost when the deque is nearly empty and a thief happens to be racing for the same last item — a rare event under a healthy, well-loaded pool, not the common case.

**Why is the last-item case (`b + 1 == t`) handled with a compare-exchange while the general steal case just stores?** When more than one item remains, a thief incrementing `bottom_` can't collide with the owner, who operates on `top_` — there's slack between the two ends. Only when exactly one item remains does the owner's pop and a thief's steal target the *same* item, which is precisely the race Hint 3 identifies as the hard correctness moment; the compare-exchange is what ensures exactly one side wins that specific contested case, while the uncontested general case avoids paying for a compare-exchange it doesn't need.

**Why does a worker that's out of local and stealable work park briefly rather than busy-spin, and why "briefly" rather than an unbounded wait?** An unbounded busy-spin wastes CPU exactly as the idle-CPU acceptance criterion forbids. A bounded park (a short condition-variable wait, or a spin-then-park hybrid) balances "don't burn CPU when there's truly nothing to do" against "don't add excessive wake-up latency when new work does arrive shortly after" — an unconditionally long or indefinite park risks a newly-submitted task sitting unnecessarily long before any idle worker notices it, unless every submission also explicitly signals waiting workers, which is a reasonable alternative design worth documenting if chosen instead.

## Reference Implementation

The above covers the pool's submission path, the worker run-loop shape (including the deadlock-avoidance principle from Hint 4 — a worker that's idle looks for its own work, then steals, before ever parking), and the deque's core steal-vs-pop race arbitration. Remaining work for the learner: the owner-side `push_top`/`pop_top` implementation (simpler than `steal`, since only the owner ever touches `top_`, but still needing correct interaction with the shared `bottom_`/`top_` comparison for the empty-check), `shutdown()`'s two modes (draining requires letting `worker_loop` finish local + stealable work before observing the stop request; immediate mode requests the stop token and returns without waiting for queued-but-unstarted work), and the "worker waiting on a future should also run available work" mechanism referenced in Hint 4 for the recursive-subtask test.

## Testing Strategy

Validate the deque's steal/pop race in isolation (many threads hammering a single small deque with concurrent push/pop/steal, checking no item is ever run twice or lost) before trusting the full pool's higher-level stress tests — isolating the deque's correctness from the pool's scheduling policy makes failures much easier to localize.

## Performance Analysis

Under a balanced workload, each worker's own push/pop is O(1) and typically uncontended; steals are O(1) amortized but pay a compare-exchange (and potential retry) specifically at the empty/near-empty boundary. The measured claim in Acceptance Criteria — that an artificially unbalanced submission pattern still results in redistributed work — is precisely the payoff for this added complexity over a single-shared-queue design.

## Failure Modes

- Both the owner and a thief reading the same last item without proper arbitration, running it twice or corrupting the deque's bookkeeping.
- A worker that blocks on a future without also being willing to run other pending/stealable work, causing a whole-pool deadlock under recursive task spawning.
- An unbounded busy-spin disguised as "waiting for work," failing the idle-CPU-usage acceptance criterion despite appearing correct functionally.

## Extensions

- Priority or affinity hints influencing which worker a task is initially placed on, while still allowing stealing as the rebalancing mechanism.
- A `parallel_for`/`parallel_reduce` convenience layer built on top of recursive task-spawning, demonstrating the pool's suitability for data-parallel algorithms.
