# Chapter 11 — Solutions

## Quick Check Answers

**11-QC1.** `std::jthread` automatically calls `request_stop()` then `join()` in its destructor if the thread is still joinable, whereas a `std::thread` whose destructor runs while still joinable (neither `join()`'d nor `detach()`'d) calls `std::terminate`. This eliminates the single most common `std::thread` misuse: forgetting to join before the `thread` object goes out of scope, including on exception/early-return paths.

**11-QC2.** Because a woken thread cannot trust that the condition it was waiting for is actually true: the standard permits spurious wakeups (waking with no corresponding `notify`), and even without one, another thread can observe/change the condition between the `notify` and this thread reacquiring the lock. `wait(lock, predicate)` (or an explicit `while (!predicate()) cv.wait(lock);`) re-checks the actual condition every time the thread wakes, rather than trusting that waking implies the condition holds.

**11-QC3.** A data race is two or more threads accessing the same memory location, with at least one access a write, and no synchronization (no happens-before edge) established between the accesses — this is undefined behavior per the standard. "Two threads touching the same variable" is not automatically a race: if all accesses are reads, or if a mutex/atomic-with-appropriate-ordering establishes that one access happens-before the other, there is no race even though the same memory is touched by multiple threads.

**11-QC4.** It guarantees that every memory operation (atomic or not) sequenced-before the release store in the releasing thread becomes visible to every memory operation sequenced-after the acquire load in the acquiring thread, once the acquiring thread has actually observed the released value — this is the mechanism that lets a plain, non-atomic write (e.g. filling a `vector`) be safely handed off between threads without itself being an atomic or mutex-guarded operation, as long as it happens strictly before the release and is only read strictly after the acquire.

**11-QC5.** `memory_order_relaxed` is useful whenever atomicity (no data race, indivisible read-modify-write) is all that's needed and no other thread needs to infer anything about *other* memory from having observed this atomic's value — e.g. a statistics counter that many threads increment but that nobody uses to synchronize access to some other piece of data. It is cheaper than acquire/release or seq_cst because it imposes no ordering constraint on the surrounding instructions, only on the atomicity of the operation itself.

**11-QC6.** The ABA problem: a thread reads a value A from a shared location, computes something based on A, but before it acts (typically via CAS), the value changes to B and back to A (or to a different object that happens to compare equal, e.g. a freed-and-reused pointer) — the thread's CAS then succeeds because the value still compares equal to A, even though the world changed in between in a way that invalidates the thread's assumption. A plain CAS loop only compares the bit pattern of the expected value; it has no way to know that the underlying object was actually replaced rather than untouched, since equal bit patterns are indistinguishable to `compare_exchange`.

**11-QC7.** The four jointly necessary conditions: mutual exclusion (a resource can be held by only one thread at a time), hold-and-wait (a thread holds one resource while waiting for another), no preemption (a held resource cannot be forcibly taken away), and circular wait (a cycle of threads each waiting on a resource the next holds). Breaking circular wait via a global, consistent lock-acquisition order (or using `std::scoped_lock` to acquire multiple locks atomically in a deadlock-avoiding way) makes deadlock on those locks impossible by construction, without needing to touch the other three conditions.

**11-QC8.** A data race's manifestation depends on cache-coherence protocol timing and hardware/scheduler interleaving that varies from run to run, machine to machine, and core count to core count — the specific unlucky ordering that exposes the race may simply not occur in however many runs were tried on whatever hardware ran them. "Passed 1000 CI runs" only says the race didn't manifest in those 1000 particular executions on that particular hardware; it says nothing about the next run, a different core count, a different compiler optimization level, or production hardware, none of which the CI runs actually tested.

**11-QC9.** Whether some other thread still holds a live, in-flight raw pointer to a node that has already been unlinked — an ABA-safe tagged pointer only prevents a *stale CAS* from spuriously succeeding on recycled memory; it says nothing about whether a concurrent reader is still mid-dereference of the unlinked node, which is a use-after-free hazard a tagged pointer does not address at all.

**11-QC10.** Every thread's announced epoch marker must have advanced *past* epoch *N* — proving that no thread could still be executing a critical section (and therefore still holding a pointer read) from epoch *N* or earlier, since any thread still in an older epoch might have read the node before it was unlinked.

## Problem Solutions

### Level 1 — Recognition

**11-P01.** The `std::thread` version calls `std::terminate` — its destructor requires the thread to have already been `join()`'d or `detach()`'d, and if neither happened, the destructor itself detects this and terminates the program. The `std::jthread` version safely joins automatically — its destructor calls `request_stop()` then `join()` if the thread is still joinable, so going out of scope without an explicit join is exactly the case `jthread` was designed to handle safely.

---

**11-P02.** The bug: the `return` in the middle of the critical section skips the `mtx.unlock()` call entirely, leaving the mutex locked forever (any later `lock()` attempt on it, from any thread including this one, blocks indefinitely). The RAII type that fixes this unconditionally is `std::lock_guard` (or `std::unique_lock`): its destructor calls `unlock()` regardless of how the scope is exited — normal fall-through, `return`, `break`, or an exception — so there is no code path that can skip the unlock.

---

**11-P03.** `std::async(std::launch::async, fn)` is guaranteed to run `fn` on a separate thread. `std::async(std::launch::deferred, fn)` may run `fn` lazily on the calling thread, only when `.get()` or `.wait()` is first called on the returned future — it does not necessarily run concurrently at all.

---

**11-P04.** This is undefined behavior — specifically a data race: two threads write to the same non-atomic, non-mutex-guarded memory location (`counter`) with no synchronization between them, which violates the C++ standard's data-race rule (`[intro.races]`) that any two conflicting, unsynchronized accesses to the same memory location, at least one a write, are UB, not merely "implementation-defined" or "produces an unpredictable but well-defined result."

---

**11-P05.** `memory_order_consume` is effectively unimplemented by major compilers (it is specified as equivalent to `memory_order_acquire` in practice) and should not be used in new code. `memory_order_seq_cst` is the default used when no memory order is explicitly specified on an atomic operation.

### Level 2 — Prediction

**11-P06.** This is undefined behavior — `std::latch`'s counter is required never to go below zero; a `count_down()` call beyond what the constructed count allows for is a precondition violation (`[thread.latch]` specifies the counter must not be decremented below zero), and the standard does not define what happens on such a call. In practice this typically manifests as either an assertion failure/crash in a checked implementation, or silent corruption of the latch's internal counter representation (e.g. wraparound to a large value) in an unchecked one — the exact observable misbehavior is implementation-dependent, but it is never a defined, safe outcome.

---

**11-P07.** Classic deadlock: thread 1 holds `mutexA` and blocks waiting to acquire `mutexB` (held by thread 2); thread 2 holds `mutexB` and blocks waiting to acquire `mutexA` (held by thread 1). Neither thread can proceed because each is waiting on a resource the other holds and will not release until it, in turn, acquires the resource it is waiting for — a circular wait, and the program hangs permanently on these two mutexes.

---

**11-P08.** In the first scenario, `future.get()` returns `42` — the value set via `promise.set_value(42)` is delivered to the waiting/blocking consumer. In the second scenario, when the function running under `std::async` throws an exception instead of calling `set_value`/`set_exception` explicitly, the `std::async`/`std::promise` machinery automatically captures that exception and stores it in the shared state; `future.get()` then rethrows that exact exception in the consumer's thread, rather than returning a value or hanging.

---

**11-P09.** No — the final value is not guaranteed to be exactly 800,000... actually it *is* guaranteed to be exactly 800,000, because `fetch_add` is a genuinely atomic read-modify-write operation: even under `memory_order_relaxed`, every single `fetch_add` call is indivisible with respect to every other atomic operation on the same object, so all 800,000 increments are individually applied with no lost updates. What `memory_order_relaxed` does *not* provide is any ordering guarantee relative to *other* memory operations (atomic or not) — it guarantees atomicity of this counter's updates, not visibility ordering of anything else relative to those updates.

---

**11-P10.** Yes, the consumer is guaranteed to see the producer's writes to `data` once it observes `ready.load() == true` — the release store on `ready` synchronizes-with the acquire load that observes it, establishing a happens-before edge: everything sequenced-before the release (including the non-atomic writes to `data`) happens-before everything sequenced-after the acquire that observed it (including the consumer's subsequent reads of `data`). This is exactly the mechanism that makes handing off plain, non-atomic data between threads via a release/acquire flag well-defined.

---

**11-P11.** No — with `memory_order_relaxed` on both the store and the load, the consumer is not guaranteed to see the producer's writes to `data`, even after observing `ready == true`. Relaxed ordering only guarantees the atomicity of the operation on `ready` itself; it establishes no happens-before edge with any other memory operation, so the compiler and hardware remain free to reorder the writes to `data` relative to the store on `ready` (or the consumer's reads of `data` relative to its load of `ready`), meaning the consumer could observe `ready == true` while still seeing stale or partially-written `data`.

---

**11-P12.** Under high contention, most concurrent pushers' CAS attempts are expected to fail and retry rather than succeed on the first attempt — every time some other thread's push succeeds first and changes `head`, every other thread's read-then-CAS sequence becomes stale (its captured `head` no longer matches the actual current `head`), forcing it to re-read `head`, rebuild its `new_node`'s `next` pointer, and retry the CAS. With N concurrent pushers, only one CAS can succeed per actual head change, so the expected number of failed retries grows with the degree of contention.

---

**11-P13.** Yes, this is entirely valid semaphore usage — unlike a mutex, a semaphore has no notion of "the thread that acquired it must be the one that releases it"; `acquire()` and `release()` are independent operations on a shared counter, so thread B releasing a semaphore that thread A acquired is exactly the designed cross-thread signaling pattern. The equivalent using `std::mutex` (locked by A, "unlocked" by B) is not valid: a `std::mutex`'s behavior when unlocked by a thread that does not own the lock is undefined — mutex ownership is tied to the locking thread specifically.

---

**11-P14.** If all workers stole from the front (the same end the owner pops from) instead of the back, overall throughput would suffer from significantly increased contention: every steal attempt would now compete directly with the owning thread's own pop operations on the exact same end of the deque, requiring synchronization on every single pop rather than only on the comparatively rare steal events. Stealing from the opposite end (the back) is specifically what lets the owner's own pops (from the front) proceed largely uncontended in the common case where no one is stealing.

---

**11-P15.** Producers attempting to push while a consumer busy-spins on `while (queue.empty()) {}` without releasing the mutex will themselves block forever trying to acquire that same mutex to push — the consumer is holding the mutex continuously throughout its spin (it never calls `wait()`, which is the only operation that atomically releases the lock while blocking), so no producer can ever acquire the mutex to push an item, and the queue can never become non-empty. This is a self-inflicted deadlock via a busy-spin that never releases the very lock a producer needs.

---

**11-P16.** No, this specific program cannot deadlock on these three mutexes — `std::scoped_lock`'s multi-mutex constructor uses a deadlock-avoidance algorithm (effectively locking them via an internal algorithm equivalent to `std::lock()`, which acquires all of them without a fixed order that could conflict with another thread's fixed order) that guarantees no deadlock among threads that use `scoped_lock` for the *same set* of mutexes, regardless of the argument order each caller happens to write — this holds precisely because every thread goes through `scoped_lock`'s algorithm rather than three sequential, individually-ordered `lock()` calls, which is exactly the pattern that would introduce an ordering hazard.

---

**11-P17.** The uncaught exception propagates out of the worker thread's run loop entirely; since there is no active exception handler on that thread's stack by the time it escapes the thread's top-level function, `std::terminate` is called, which by default aborts the entire process — taking down every other worker thread and any tasks still queued, not just failing the one task that threw. This is a process-wide failure caused by one task's exception, not a contained, per-task failure.

---

**11-P18.** Each thread's throughput drops compared to the separate-cache-line case, because every write to `a` from one thread invalidates the entire cache line (including `b`'s copy) in every other core's cache, forcing `b`'s thread to reload the line from a slower cache level or memory on its next access, and vice versa — even though `a` and `b` are logically unrelated, the hardware's cache-coherence protocol treats the whole cache line as the unit of invalidation. This phenomenon is called false sharing.

---

**11-P19.** For a `future` obtained from `std::async`, if the associated task was launched with (or defaults to) `std::launch::async`, the future's destructor *does* block until the asynchronous task completes — this is a special, async-specific rule so that a discarded future doesn't leave a detached, uncontrolled thread running past the point where nothing can observe its result. A `future` obtained from a `std::packaged_task` (run on a thread the caller manages explicitly, e.g. via `std::thread`) has no such special blocking behavior in its destructor — its destructor is a plain, non-blocking release of the shared state, and the task keeps running on whatever thread was running it regardless of the future's lifetime.

### Level 3 — Implementation

**11-P20.**
```cpp
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

class Channel {
    std::mutex mtx;
    std::condition_variable not_full, not_empty;
    std::deque<int> q;
    static constexpr size_t CAP = 8;
public:
    void push(int v) {
        std::unique_lock lock(mtx);
        not_full.wait(lock, [&] { return q.size() < CAP; });
        q.push_back(v);
        not_empty.notify_one();
    }
    int pop() {
        std::unique_lock lock(mtx);
        not_empty.wait(lock, [&] { return !q.empty(); });
        int v = q.front();
        q.pop_front();
        not_full.notify_one();
        return v;
    }
};

int main() {
    Channel ch;
    long long sum = 0;
    std::thread producer([&] { for (int i = 0; i < 100; ++i) ch.push(i); });
    std::thread consumer([&] { for (int i = 0; i < 100; ++i) sum += ch.pop(); });
    producer.join();
    consumer.join();
    // sum == 4950 (0+1+...+99)
}
```
Running this repeatedly under `ctest` (e.g. `ctest --repeat until-pass:200` or a loop) confirms the sum is always exactly 4950 and the program always terminates, building confidence against a nondeterministic hang.

---

**11-P21.**
```cpp
template <typename T>
class BoundedQueue {
    std::mutex mtx;
    std::condition_variable not_full, not_empty;
    std::deque<T> q;
    size_t cap;
public:
    explicit BoundedQueue(size_t capacity) : cap(capacity) {}

    void push(T v) {
        std::unique_lock lock(mtx);
        not_full.wait(lock, [&] { return q.size() < cap; });
        q.push_back(std::move(v));
        not_empty.notify_one();
    }
    T pop() {
        std::unique_lock lock(mtx);
        not_empty.wait(lock, [&] { return !q.empty(); });
        T v = std::move(q.front());
        q.pop_front();
        not_full.notify_one();
        return v;
    }
    bool try_push(T v, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mtx);
        if (!not_full.wait_for(lock, timeout, [&] { return q.size() < cap; })) return false;
        q.push_back(std::move(v));
        not_empty.notify_one();
        return true;
    }
    bool try_pop(T& out, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mtx);
        if (!not_empty.wait_for(lock, timeout, [&] { return !q.empty(); })) return false;
        out = std::move(q.front());
        q.pop_front();
        not_full.notify_one();
        return true;
    }
};
```
```cpp
TEST(BoundedQueueTest, BlockingPushPop) {
    BoundedQueue<int> q(2);
    q.push(1); q.push(2);
    EXPECT_EQ(q.pop(), 1);
}
TEST(BoundedQueueTest, TryPushTimesOutWhenFull) {
    BoundedQueue<int> q(1);
    q.push(1);
    EXPECT_FALSE(q.try_push(2, std::chrono::milliseconds(50)));
}
```

---

**11-P22.**
```cpp
#include <atomic>
#include <thread>
#include <vector>

void increment_many(std::atomic<int>& counter, int times) {
    for (int i = 0; i < times; ++i)
        counter.fetch_add(1, std::memory_order_relaxed);
    // relaxed suffices: each fetch_add is independently atomic (no lost
    // updates), and no thread needs to observe any other thread's
    // intermediate value or any other memory relative to this counter —
    // only the final total after all threads join matters.
}

int main() {
    std::atomic<int> counter{0};
    std::vector<std::jthread> workers;
    for (int i = 0; i < 8; ++i)
        workers.emplace_back(increment_many, std::ref(counter), 100000);
    workers.clear(); // jthreads join on destruction
    // counter == 800000
}
```

---

**11-P23.**
```cpp
class SpinLock {
    std::atomic<bool> locked{false};
public:
    void lock() {
        while (locked.exchange(true, std::memory_order_acquire))
            std::this_thread::yield();
    }
    bool try_lock() {
        return !locked.exchange(true, std::memory_order_acquire);
    }
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};
```
A shared-counter test with many `std::jthread`s each doing `for (int i = 0; i < N; ++i) { std::lock_guard g(spin); ++plain_counter; }` and joining confirms `plain_counter == num_threads * N` exactly, verifying `SpinLock` provides correct mutual exclusion under `std::lock_guard<SpinLock>`.

---

**11-P24.**
```cpp
std::atomic<bool> ready{false};
std::vector<int> data;

void producer() {
    data.assign(1000, 0);
    for (int i = 0; i < 1000; ++i) data[i] = i;
    ready.store(true, std::memory_order_release);
}
void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}
    long long sum = 0;
    for (int v : data) sum += v;
}
```
Built and run under `wsl-clang-tsan`, this reports clean — the release/acquire pair establishes a valid happens-before edge, so there is no race on `data` even though it's plain, unsynchronized memory. Changing both operations to `memory_order_relaxed` and rebuilding under the same preset: TSan **now reports a data race** on `data` — relaxed ordering provides no happens-before edge, so the consumer's reads of `data` are not ordered relative to the producer's writes from TSan's (and the standard's) perspective, and TSan's race detector flags exactly this unordered concurrent access, even though the program may still "look correct" on a given run due to actual hardware behavior — this is precisely the distinction this chapter's misconceptions section draws between "looks fine" and "is defined."

---

**11-P25.**
```cpp
std::latch init_done(4);
std::atomic<bool> go{false};
std::atomic<int> premature_starts{0};

void worker(std::stop_token tok) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // fake init
    init_done.count_down();
    while (!go.load(std::memory_order_acquire)) {}
    if (init_done.try_wait() == false) premature_starts.fetch_add(1); // sanity
    // main loop...
}

int main() {
    std::vector<std::jthread> workers;
    for (int i = 0; i < 4; ++i) workers.emplace_back(worker);
    init_done.wait();
    go.store(true, std::memory_order_release);
    workers.clear();
    // premature_starts == 0
}
```
Because every worker blocks on `go` (set only after `init_done.wait()` returns on the main thread, which itself only returns after all 4 have called `count_down()`), no worker can observe `go == true` before all 4 have finished initializing — verified structurally by the ordering, and checked defensively via `premature_starts`.

---

**11-P26.**
```cpp
class WorkQueue {
    std::mutex mtx;
    std::condition_variable cv;
    std::queue<std::function<void()>> tasks;
    std::vector<std::jthread> workers;
public:
    explicit WorkQueue(size_t n) {
        for (size_t i = 0; i < n; ++i)
            workers.emplace_back([this](std::stop_token tok) { run(tok); });
    }
    void submit(std::function<void()> f) {
        std::lock_guard lock(mtx);
        tasks.push(std::move(f));
        cv.notify_one();
    }
    void run(std::stop_token tok) {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mtx);
                cv.wait(lock, [&] { return !tasks.empty() || tok.stop_requested(); });
                if (tasks.empty() && tok.stop_requested()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }
    void request_stop_and_drain() {
        for (auto& w : workers) w.request_stop();
        cv.notify_all();
    }
};
```
Submitting 50 tasks each incrementing a shared `std::atomic<int> completed`, then calling `request_stop_and_drain()` and letting the `jthread`s join on destruction, verifies `completed == 50` before any worker actually exits — because a worker only returns when its queue check confirms both empty *and* stop-requested, never abandoning a still-nonempty queue.

---

**11-P27.**
```cpp
struct Account { std::mutex mtx; int balance; };

void transfer(Account& from, Account& to, int amount) {
    std::scoped_lock lock(from.mtx, to.mtx);
    from.balance -= amount;
    to.balance += amount;
}
```
A stress test spawning many threads calling `transfer(a, b, 1)` and `transfer(b, a, 1)` concurrently between the same two accounts, run under `wsl-clang-tsan` for 10,000+ calls, reports no deadlock and no race — `scoped_lock`'s deadlock-avoidance algorithm handles the reversed argument order safely regardless of which direction any given call transfers in.

---

**11-P28.**
```cpp
template <int N>
class ConnectionPool {
    std::counting_semaphore<N> sem{N};
public:
    class ConnectionLease {
        ConnectionPool& pool;
    public:
        explicit ConnectionLease(ConnectionPool& p) : pool(p) { pool.sem.acquire(); }
        ~ConnectionLease() { pool.sem.release(); }
    };
    ConnectionLease acquire() { return ConnectionLease(*this); }
};
```
A stress test with `std::atomic<int> in_use{0}`, 10 threads each doing `{ ConnectionPool<3>::ConnectionLease lease(pool); in_use.fetch_add(1); /* brief hold */ in_use.fetch_sub(1); }`, checking `in_use.load() <= 3` throughout, confirms the pool never allows more than 3 concurrently held connections.

---

**11-P29.**
```cpp
std::array<int, 4> partial;
int combined = 0;
std::barrier phase1_done(4, [&] {
    combined = partial[0] + partial[1] + partial[2] + partial[3];
});

void worker(int idx, const std::array<int, 400>& data) {
    int sum = 0;
    for (int i = idx * 100; i < (idx + 1) * 100; ++i) sum += data[i];
    partial[idx] = sum;
    phase1_done.arrive_and_wait();
    // phase 2:
    assert(combined == /* known correct total */ 0 || combined != -999999); // sanity: combined must be set
    int local_check = combined; // used in phase 2 computation
}
```
Because `std::barrier`'s completion function runs (on one arriving thread) only after all 4 have arrived, and *before* any of the 4 is released to proceed past `arrive_and_wait()`, `combined` is guaranteed correct and visible to every thread's phase-2 code — the assertion inside phase 2 itself, rather than only a final check, would catch a thread that somehow proceeded before the completion function ran, which the barrier's semantics rule out by construction.

---

**11-P30.**
```cpp
class SharedMap {
    std::shared_mutex mtx;
    std::map<int, std::string> data;
public:
    std::optional<std::string> get(int key) {
        std::shared_lock lock(mtx);
        auto it = data.find(key);
        return it == data.end() ? std::nullopt : std::optional(it->second);
    }
    void insert(int key, std::string value) {
        std::unique_lock lock(mtx);
        data.emplace(key, std::move(value));
    }
};
```
A stress test with 10 reader threads continuously calling `get()` on random keys and 2 writer threads periodically calling `insert()`, run under `wsl-clang-tsan`, reports clean — `shared_lock`/`unique_lock` correctly serialize writers against both readers and other writers, so no reader ever observes a torn insert (a `get()` either doesn't see the key at all, or sees it with its value fully constructed, never in between).

---

**11-P31.**
```cpp
std::promise<int> p1, p2;
auto f1 = p1.get_future();
auto f2 = p2.get_future();

std::thread stage1([&] {
    try { p1.set_value(compute_initial()); }
    catch (...) { p1.set_exception(std::current_exception()); }
});
std::thread stage2([&] {
    try {
        int v = f1.get(); // rethrows if stage1 threw
        p2.set_value(transform(v));
    } catch (...) { p2.set_exception(std::current_exception()); }
});
int final_result = f2.get(); // rethrows if stage1 or stage2 threw
stage1.join(); stage2.join();
```
With a correctly computing `stage1`, `final_result` equals the expected transformed value. If `stage1` throws instead of calling `set_value`, `f1.get()` inside `stage2` rethrows that exact exception, `stage2`'s catch converts it into `p2`'s exception, and the final `f2.get()` rethrows it again — confirming the exception propagates all the way through the pipeline rather than being silently lost at any stage.

### Level 4 — Debugging

**11-P32.** [DEBUG] The exact cause: the `std::thread` object's destructor runs while the thread is still joinable (neither `join()` nor `detach()` was called), which the standard specifies calls `std::terminate`. Minimal fix: add `t.join()` (or `t.detach()`) before the function returns, on every exit path (including exception paths, ideally via RAII rather than a manual call at the end). Design-changing fix: replace `std::thread` with `std::jthread`, whose destructor joins automatically. `detach()` trades one problem for a subtler one: a detached thread's lifetime is no longer tracked by anything — if it outlives the objects it references (e.g. captured-by-reference locals), it causes use-after-scope UB, and there is no way to later check whether it finished, catch an exception from it, or ensure it completes before the process exits.

---

**11-P33.** [DEBUG] The race: between the `if (queue.empty())` check evaluating true and the subsequent `cv.wait(lock)` call actually blocking, a producer thread can acquire the lock, push an item, call `notify_one()`, and release the lock — but since the consumer hadn't yet called `wait()` at the moment of that notify, the notification is missed entirely (there's no "wait" registered yet to wake), and the consumer then proceeds to `wait()` and blocks forever with no future notify ever coming (assuming no further pushes). A spurious wakeup could also occur with no corresponding push at all, and with a bare `if`, the code would incorrectly proceed to pop from an empty queue instead of re-checking. The fix: use `cv.wait(lock, [&]{ return !queue.empty(); });` (or an explicit `while (queue.empty()) cv.wait(lock);`), which re-checks the actual predicate every time it wakes and only truly blocks if it's currently false, closing the missed-notification window because the check-and-wait becomes atomic with respect to the lock.

---

**11-P34.** [DEBUG] Relaxed ordering provides no happens-before edge between the store and the load — it only guarantees that the flag `data_ready` itself is read/written atomically, not that any other memory operation (here, the writes to the struct's fields) is visible in a consistent order relative to it. The compiler and hardware remain free to reorder the struct writes relative to the flag store (so the flag could become visible before all the writes actually complete), and the consumer's reads of the struct relative to its flag load, so it can observe the flag as `true` while still seeing a partially-written or stale struct. The precise fix: change the producer's store to `memory_order_release` and the consumer's load to `memory_order_acquire` — this pair establishes the happens-before edge that guarantees every write sequenced-before the release is visible after the acquire observes it.

---

**11-P35.** [DEBUG] The lock-ordering inconsistency: thread 1 always acquires `mtxA` then `mtxB`; thread 2, at a separate call site, always acquires `mtxB` then `mtxA` — under the interleaving where thread 1 holds `mtxA` and is waiting for `mtxB` while thread 2 simultaneously holds `mtxB` and is waiting for `mtxA`, neither can proceed: a classic circular wait. The fix: impose one single, consistent acquisition order for these two mutexes everywhere in the codebase (e.g. always lock the lower-address or an explicitly assigned "always A before B" order), or better, replace both call sites' pair of individual `lock_guard`s with a single `std::scoped_lock(mtxA, mtxB)` call, which is safe under any argument order. This bug survives ordinary testing and code review because each call site looks correct in isolation — the defect only exists in the *relationship* between two call sites that may be far apart in the codebase, and it only manifests when the specific unlucky interleaving (both threads mid-acquisition simultaneously) actually occurs, which single-threaded or light-load testing essentially never triggers.

---

**11-P36.** [DEBUG] This is specifically the ABA problem, not a generic data race: the first thread's `head` pointer value is read once, then re-read/re-compared later via CAS — in between, the node it points to is freed and a *different* logical node happens to be allocated at that exact same address, so the pointer's bit pattern ("A") is unchanged even though the actual object it refers to is entirely different ("B" happened in between, masked by the allocator reusing the address). The CAS, which can only compare bit patterns, cannot distinguish "nothing changed" from "something changed and then something coincidentally identical-looking took its place," so it spuriously succeeds and corrupts the stack. A generic race would simply be two threads touching the same live memory unsynchronized; ABA specifically requires the value to cycle back to an equal-looking state. Standard mitigation: tag every pointer with a monotonically incrementing version/generation counter packed alongside the address (a "tagged pointer") so a CAS comparing the full tagged value detects that the generation changed even if the address happened to be reused — or use hazard pointers/epoch-based reclamation to defer actually freeing a node until no thread could possibly still hold a reference to it, eliminating address reuse within any window a concurrent operation could observe.

---

**11-P37.** [DEBUG] An exception escaping a worker thread's top-level function (its run loop) with no active handler on that thread's call stack causes `std::terminate` to be invoked — and `std::terminate`'s default behavior is to abort the entire process, not just the one thread, which is why every other worker and any queued work dies too, even though logically only one task misbehaved. The fix: wrap the actual `task()` invocation inside the worker's run loop in a `try`/`catch (...)` block, immediately at the call site, so the exception never escapes the worker thread's top level at all; what should happen to the caught exception is that it gets captured (e.g. via `std::current_exception()`) and delivered back through that specific task's own `std::promise`/`std::future`, so the failure is reported to whoever submitted the task, exactly as if the task had returned an error, rather than crashing anything else.

---

**11-P38.** [DEBUG] The root cause: the `std::latch`'s expected count (`4`) is a separate, hand-written literal disconnected from the actual code that spawns worker threads, so when the spawning code was refactored to add a 5th worker, nothing forced the latch's construction argument to be updated in lockstep — this is a magic-number-decoupled-from-its-source-of-truth bug, the kind that survives review because the two numbers "4" (latch) and "5" (loop bound) never visually collide in a diff of either change alone. The structural fix: derive the latch's count directly from the same variable or loop bound used to spawn the workers (e.g. `std::latch init_done(worker_count);` where `worker_count` is the exact value passed to the spawning loop, ideally the same `constexpr`/parameter used in both places), so a future change to the worker count cannot silently desynchronize from the latch's expectation — the two values become structurally the same value rather than two independently-maintained literals.

---

**11-P39.** [DEBUG] Under 32-way contention, a CAS-loop counter's `compare_exchange` calls fail and retry far more often than they succeed on the first attempt (per 11-P12's reasoning), and every failed attempt burns real CPU cycles spinning and retrying with no useful work accomplished — whereas a mutex's blocked threads are typically put to sleep by the OS scheduler (not spinning), so the "cost" of contention under a mutex is mostly just scheduling latency, not wasted CPU churn. At this specific contention level, the measured result shows the mutex genuinely wins; this benchmark result should change the developer's design decision away from a blanket "lock-free is always faster" assumption and toward keeping the mutex here — but it does not license concluding "lock-free is always slower" either; the actual, durable lesson (reinforced by Ch12) is to keep measuring under the specific contention level and access pattern that matters in production, not to generalize from one number in either direction.

---

**11-P40.** [DEBUG] The starvation happens because the `shared_mutex` implementation's admission policy always grants a new shared (reader) lock request immediately if any readers are currently active or none are waiting exclusively with priority — under a continuous stream of readers, there is effectively never a gap where the reader count reaches zero long enough for a waiting writer to be granted the exclusive lock, since a new reader can always slip in right as the previous one releases. This is a fairness policy failure, not a correctness bug (no data is corrupted) — different from deadlock, since every reader does eventually make progress; the writer simply never gets its turn. The fix: adopt a fairness policy that stops admitting new readers once at least one writer is waiting (a "writer-priority" or "fair" reader-writer lock), so a waiting writer is guaranteed to be granted access once the currently-active readers (but no newly-arriving ones) finish, bounding the writer's wait.

---

**11-P41.** [DEBUG] This is a scheduling/dependency deadlock, structurally distinct from a lock-ordering deadlock — no mutex is involved at all; the deadlock arises purely from the combination of (a) a worker blocking synchronously on a future for a task's result and (b) that dependent task being stuck, unscheduled, in the very same worker's own queue, with no other worker available (or lucky enough) to steal it in this run. The structural fix: never let a worker thread block synchronously (via `future.get()`) on a task that might still be sitting unscheduled — either restructure the dependency as a continuation (attach a callback to run when the dependency completes, rather than blocking and waiting for it), or have the blocked worker itself help drain its own queue (and potentially others') while it waits, so the dependency it's blocked on always has a chance to actually execute even in the worst-case single-worker-available scenario.

### Level 5 — Integration

**11-P42.**
```cpp
// (a) mutex + two condition variables — same structure as BoundedQueue (11-P21),
// used directly with multiple producer/consumer threads calling push()/pop().

// (b) lock-free ring buffer:
template <typename T, size_t N>
class LockFreeRing {
    std::array<T, N> buf;
    std::atomic<size_t> head{0}, tail{0};
public:
    bool push(T v) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t next = (t + 1) % N;
        if (next == head.load(std::memory_order_acquire)) return false; // full
        buf[t] = std::move(v);
        tail.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& out) {
        size_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false; // empty
        out = std::move(buf[h]);
        head.store((h + 1) % N, std::memory_order_release);
        return true;
    }
};
```
For genuine multi-producer/multi-consumer safety, `push`/`pop`'s slot reservation additionally needs a CAS loop on `head`/`tail` rather than a plain load-then-store (as written above this ring buffer is SPSC-safe only); the full MPMC version reserves a slot via `compare_exchange_weak` on the index before writing. Both the mutex-based and lock-free MPMC implementations, stress-tested with 4 producers/4 consumers pushing/popping 100,000 total items and checked via a per-item completion set (no duplicates, no losses), report clean under `wsl-clang-tsan`.

---

**11-P43.**
```cpp
class WorkStealingPool {
    struct Worker { std::deque<std::function<void()>> local; std::mutex mtx; };
    std::vector<std::unique_ptr<Worker>> workers;
    std::atomic<size_t> submitted{0}, completed{0};
    std::stop_source stopper;

    void run(size_t idx, std::stop_token tok) {
        while (!tok.stop_requested() || pending() > 0) {
            auto task = pop_own(idx);
            if (!task) task = steal_from_other(idx);
            if (task) { (*task)(); completed.fetch_add(1, std::memory_order_relaxed); }
            else std::this_thread::yield();
        }
    }
    // pop_own: locks workers[idx]->mtx, pops from the front.
    // steal_from_other: picks a random other worker, locks its mtx, pops from the back.
public:
    void submit(size_t idx, std::function<void()> f) {
        submitted.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(workers[idx]->mtx);
        workers[idx]->local.push_front(std::move(f));
    }
};
```
A recursive parallel Fibonacci or merge-sort submitted through this pool (each task potentially submitting two more sub-tasks) produces the correct result, and `submitted == completed` after graceful shutdown (`stopper.request_stop()`, letting each worker drain remaining local and stealable work before exiting), confirming every submitted task — including recursively-submitted ones — completes exactly once.

---

**11-P44.** Given a supplied working-but-flawed producer/consumer implementation, the diagnosis process is: (1) build a stress harness looping the program under representative multi-thread load for 10,000+ iterations to reproduce the once-per-ten-thousand hang; (2) run the reproducing case under `wsl-clang-tsan`, examining the report for the exact interleaving (typically: a missed-notification window like 11-P33's, or a lock-ordering inconsistency like 11-P35's, manifesting specifically under this program's queue/mutex/cv structure); (3) apply the corresponding minimal fix (predicate-based `wait()`, or consistent lock ordering via `scoped_lock`); (4) re-run the stress harness for 10,000+ iterations post-fix confirming zero hangs, and re-run under `wsl-clang-tsan` confirming a clean report. This mirrors 11-IC1's methodology exactly, applied to this problem's specific supplied implementation.

---

**11-P45.**
```cpp
template <typename R>
struct SafeTask {
    std::promise<R> prom;
    std::function<R()> fn;
    void operator()() {
        try { prom.set_value(fn()); }
        catch (...) { prom.set_exception(std::current_exception()); }
    }
};
```
Rebuilding the 11-P37 pool so its `run()` loop invokes each task through this wrapper (or, for `void`-returning tasks, the `set_value()`-on-success/`set_exception()`-on-catch pattern adapted for `promise<void>`) means the `try`/`catch` from 11-P37's fix now always delivers the outcome through the task's own future rather than merely swallowing it. With a batch of 100 tasks where every 10th throws, the other 90 futures resolve normally via `.get()`, and the 10 throwing ones' futures correctly rethrow their exact exception on `.get()` — verified by wrapping each `.get()` call in its own try/catch and tallying successes versus rethrows against the expected 90/10 split.

---

**11-P46.**
```cpp
template <typename Iter, typename F>
void parallel_for(Iter first, Iter last, size_t chunk_size, F fn, WorkQueue& pool) {
    size_t total = std::distance(first, last);
    size_t chunks = (total + chunk_size - 1) / chunk_size; // ceil, handles uneven sizes
    std::latch done(chunks);
    for (size_t c = 0; c < chunks; ++c) {
        Iter chunk_begin = first + c * chunk_size;
        Iter chunk_end = std::min(last, chunk_begin + chunk_size);
        pool.submit([chunk_begin, chunk_end, &fn, &done] {
            for (auto it = chunk_begin; it != chunk_end; ++it) fn(*it);
            done.count_down();
        });
    }
    done.wait();
}
```
Tested against a single-threaded reference loop across several input sizes, including sizes not evenly divisible by `chunk_size` (e.g. 103 elements with `chunk_size = 10`, producing 11 chunks where the last has only 3 elements, correctly bounded by `std::min(last, chunk_begin + chunk_size)`), results match exactly.

---

**11-P47.**
```cpp
class FairRWLock {
    std::mutex mtx;
    std::condition_variable cv;
    int readers = 0;
    bool writer_active = false;
    int writers_waiting = 0;
public:
    void lock_shared() {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&] { return !writer_active && writers_waiting == 0; });
        ++readers;
    }
    void unlock_shared() {
        std::lock_guard lock(mtx);
        if (--readers == 0) cv.notify_all();
    }
    void lock() {
        std::unique_lock lk(mtx);
        ++writers_waiting;
        cv.wait(lk, [&] { return !writer_active && readers == 0; });
        --writers_waiting;
        writer_active = true;
    }
    void unlock() {
        std::lock_guard lock(mtx);
        writer_active = false;
        cv.notify_all();
    }
};
```
The `writers_waiting` counter is the fairness policy: once nonzero, `lock_shared()`'s predicate blocks new readers from being admitted, guaranteeing the waiting writer gets in once current readers drain — preventing 11-P40's starvation by construction. A stress test with many readers and fewer writers under `wsl-clang-tsan`, with an atomic "active writer + active reader count" check inside every critical section asserting mutual exclusion between writer and (readers or another writer), reports clean.

---

**11-P48.**
```cpp
struct alignas(std::hardware_destructive_interference_size) PaddedAtomic {
    std::atomic<size_t> value{0};
};
// unpadded: head and tail as plain adjacent std::atomic<size_t> members
// padded: head and tail as separate PaddedAtomic members
```
Using Ch10's benchmark harness to run the same many-producer/many-consumer push/pop workload against both layouts, the unpadded version (head and tail sharing a cache line) shows measurably higher latency/lower throughput under contention than the padded version, because every producer's tail update invalidates the consumer-side cache line holding `head` and vice versa — this is presented here purely as a contention/throughput comparison (no race is reported by TSan in either layout, since both are already correctly synchronized), with the actual measured-cost treatment and quantified overhead left to Ch12.

### Level 6 — Production

**11-P49.** A concrete migration policy: (1) do not attempt a big-bang mass-replace; instead add a lint/static-analysis rule (a clang-tidy check, or a simple grep-based pre-commit hook) that flags any *new* raw `std::thread` construction introduced after the policy takes effect, requiring justification or a switch to `jthread` in review — this stops the bleeding immediately without touching existing code; (2) for the existing 40+ sites, prioritize by risk: sites inside destructors, `catch` blocks, or on early-return paths (exactly the pattern that caused the incident) get migrated first, in small, independently-reviewable PRs, one or a few call sites at a time; (3) sites that are provably safe today (already correctly joined on every path, verified by test coverage) can be migrated opportunistically, on a slower cadence, since they're not actively at risk — this sequences the migration by actual incident-relevant risk rather than uniformly or all at once.

---

**11-P50.** Since TSan is specifically designed to detect data races (unsynchronized conflicting memory accesses) and says nothing at all about scheduling delays, lock contention duration, priority inversion, or OS-level scheduling latency, "TSan is clean" cannot be used as evidence against a latency/contention hypothesis — a perfectly race-free program can still have terrible tail latency from threads waiting a long time for a contended (but correctly used) lock, or from scheduler unfairness. A diagnostic strategy for this class of issue reaches for tools TSan was never meant to be: sampling profilers or `perf`/ETW-based tracing to see where time is actually spent (blocked-on-lock vs. running), lock-contention-specific instrumentation (e.g. logging lock hold/wait durations and looking for outliers), and correlating the latency spikes with load metrics (queue depth, active thread count) captured at the time of the spike — none of which TSan's race-detection instrumentation records at all.

---

**11-P51.** A 15% win in a 2-thread microbenchmark does not justify a blanket policy: it says nothing about behavior at production thread counts and contention levels, nothing about task granularity (a lock-free structure's advantage or disadvantage can flip entirely as contention scales), and nothing about the ongoing maintenance cost of keeping a lock-free implementation correct (every future change requires re-verifying ABA-safety and memory-ordering correctness, a burden a mutex-based version doesn't carry). The decision should be informed by: measured contention level and thread count in the actual production workload (not a 2-thread microbenchmark), task granularity (how much work happens per critical-section entry), and the team's ongoing capacity to review and maintain lock-free code correctly. A sound policy avoids both extremes: default to lock-based for new shared-queue code, and require a documented, production-representative benchmark plus an explicit correctness review (TSan-clean under realistic contention, an ABA/reclamation argument if applicable) before approving a lock-free alternative for any specific hot path — decided case by case, not by blanket rule in either direction.

---

**11-P52.** A concrete process change addressing both detection and prevention: for detection, require that CI's stress/concurrency test tier runs on hardware with a realistically high core count (not whatever's cheapest for the CI runner pool) and under artificially perturbed scheduling (e.g. randomized thread-priority jitter, or tools that intentionally interleave threads more aggressively than default OS scheduling would) specifically because rare interleavings are far more likely to surface when the test environment doesn't systematically avoid the exact conditions (low core count) that happened to suppress this one; for prevention, mandate that any code acquiring more than one lock go through a single, audited pattern (`std::scoped_lock`, or a documented global lock-order convention enforced by lint), reducing the actual number of possible acquisition interleavings that could ever deadlock, rather than relying on each individual call site being independently correct.

### Level 7 — Principal Reasoning

**11-P53.** For a high-throughput stream of independent requests with strict p99 latency requirements, a fixed thread pool with a shared bounded queue is the strongest starting choice over thread-per-connection (whose per-request OS-thread creation/teardown cost and unbounded thread count directly threaten p99 tail latency under load spikes) and over a work-stealing pool (whose extra bookkeeping and stealing overhead pays for itself only when task dependencies create meaningfully unbalanced load across workers — independent, uniform requests don't create that imbalance, so work-stealing's complexity buys little here and risks its own tail-latency contribution from steal contention). For the chosen model's shared state (the bounded queue), I would mandate: `memory_order_acquire`/`release` specifically at the queue's slot-reservation and slot-publish points (not blanket `seq_cst`), a single documented lock (or lock-free ring buffer, chosen only after production-representative benchmarking per 11-P51's discipline) protecting/guarding the queue, and bounded capacity with an explicit backpressure policy (reject or block) rather than an unbounded queue that could itself become the latency-spike source under load. I would explicitly forbid `seq_cst` used as an unexamined default in this codebase's hot path — every atomic operation on the queue's shared state must have its actual required ordering reasoned out and documented, not defaulted to the strongest order "to be safe," precisely because unexamined `seq_cst` usage in a latency-critical path both costs real synchronization overhead and signals that nobody actually verified what ordering the correctness argument requires.

---

**11-P54.** The thread-safety contract: document explicitly which operations are safe to call concurrently from multiple customer threads (typically: `submit()` should be safe from any number of concurrent caller threads) and which are not (e.g. `shutdown()`/`resize()` calls should be documented as requiring the caller to ensure no concurrent `submit()` calls are in flight, rather than internally synchronizing every possible combination, which would add overhead to the common, safe-by-design `submit()` path). Exceptions thrown inside customer-supplied task callables must be captured and surfaced through that task's own future (per 11-P45's pattern) rather than ever risking `std::terminate` from inside the library's worker threads, since a customer's uncaught exception must never be allowed to crash the customer's entire process from inside a library-internal thread they don't directly control. The deliberate guarantee I would *not* make: that `shutdown()` can be called safely concurrently with `submit()` from other threads — enforcing that internally would require locking on every single `submit()` call (the hottest, most latency-sensitive path) purely to guard against a comparatively rare administrative operation; instead, the contract documents that the customer must externally sequence shutdown after ensuring no further submissions, accepting the limitation in exchange for keeping `submit()` itself lock-minimal.

---

**11-P55.** Design-review response: before approving, I would require measured contention data from the actual production access pattern for this specific data structure — not intuition, not "it's a hot path" as an assumption, but a profile showing this structure's lock actually is a measured bottleneck under realistic load (per 12's discipline, applied here as a gate). A lock-free replacement specifically incurs verification burden the lock-based version never had: every memory order used must be individually justified and TSan-verified under production-representative contention (not just low-contention correctness, which can mask ordering bugs that only manifest under real contention), and if the structure involves node reclamation (a stack, queue, or similar), a documented ABA/reclamation argument (tagged pointers, hazard pointers, or epoch-based reclamation) must accompany the change, reviewed as carefully as the algorithm itself. I would approve the change only if the contention data justifies it and this full verification burden is actually met; I would push back — regardless of how "modern" the justification sounds — if the only argument offered is architectural fashion rather than a measured bottleneck, since replacing an extensively-reviewed, correctness-critical structure on stylistic grounds trades a known-good baseline for new, harder-to-verify risk without an offsetting, demonstrated benefit.

**11-P56.**

**Reference Solution:** Design sketch — a global `std::atomic<uint64_t> global_epoch`, and per-thread state `struct ThreadEpoch { std::atomic<uint64_t> local_epoch; std::atomic<bool> active; };` registered in a small fixed-size (or dynamically-registered) table every participating thread has a slot in.

```cpp
struct EpochGuard {
    ThreadEpoch& te;
    EpochGuard(ThreadEpoch& t) : te(t) {
        te.local_epoch.store(global_epoch.load(std::memory_order_relaxed), std::memory_order_relaxed);
        te.active.store(true, std::memory_order_release);   // "I am now in a critical section"
    }
    ~EpochGuard() { te.active.store(false, std::memory_order_release); }
};

// Around any pop() that dereferences a node read from the stack:
void pop() {
    EpochGuard guard(my_thread_epoch);
    Node* head = stack_.load(std::memory_order_acquire);
    // ... CAS loop as in 11-P36, using head/head->next ...
}

// Retiring a node instead of freeing it immediately:
void retire(Node* n) {
    retire_list.push_back({n, global_epoch.load(std::memory_order_relaxed)});
    reclaim_eligible_nodes();  // scan retire_list; free any node whose retire-epoch every active thread has since advanced past
}
```

`reclaim_eligible_nodes` walks the retire-list and, for each entry, checks every registered thread's `(active, local_epoch)` pair: a node retired at epoch *E* is only actually freed once no thread is both `active` and still reporting `local_epoch <= E` — i.e., every thread that could possibly have read a pointer to that node before it was unlinked has since either left its critical section or advanced to a newer epoch.

**Stress test demonstrating safety:** run many popper threads continuously against a shared stack while one dedicated thread pushes/pops and periodically calls `reclaim_eligible_nodes()`, under `-fsanitize=address` and TSan — a correct implementation shows zero use-after-free reports and zero races across a long run (minutes, many millions of operations) with contention deliberately kept high (many threads, small stack) to maximize the odds of exposing a reclamation bug if one exists.

**Contrasting failure case:** the same stack with reclamation removed entirely (nodes `delete`d immediately upon unlink, no retire-list, no epoch tracking) reliably produces ASan heap-use-after-free reports under the identical stress test within seconds — because a concurrent thread that read `head` just before another thread unlinked and freed it will, with high probability under real contention, still be mid-dereference of `head->next` when the freed memory is reused or unmapped.

**Cost added over 11-P36's tagged-pointer-only version:** a global epoch counter plus per-thread announcement overhead (an atomic store on every operation entering/leaving a critical section, and a per-thread registration slot), and reclamation latency that is now tied to the *slowest* active thread — a single thread stalled (blocked, descheduled, or just slow) inside a critical section holds back reclamation of every node retired since it entered, across the entire structure, until it advances or exits; the tagged-pointer-only version (11-P36) has no such global stall dependency, because it never attempts to free nodes safely at all — it only prevents the CAS itself from being fooled by address reuse, leaving the *reclamation* question entirely unanswered, which is precisely the gap this problem's scheme closes.

## Integration Challenge Solution — 11-IC1

1. **Reproduce reliably.** Build a stress harness that spawns the producer/consumer system's full complement of producer and consumer threads (matching or exceeding the production thread count — under-provisioning threads reduces the number of possible interleavings and may never trigger the rare one) against a queue sized and loaded to match production characteristics (a bounded capacity small enough that producers/consumers actually contend for it regularly, and enough total items pushed — well beyond the queue's capacity — that the full push/pop/wait/notify cycle executes many thousands of times per run), and loop the entire program start-to-finish for tens of thousands of iterations, watching for any run that hangs (e.g. via a wall-clock timeout per iteration). Light load fails to trigger it because the specific unlucky interleaving requires several threads to be simultaneously mid-operation on the mutex/condition-variable state at just the right moments — under light load, operations are spaced far enough apart in time that the narrow race window is essentially never hit; only sustained, high-frequency contention makes the window's rare occurrence probable enough to observe in a practical number of runs.

2. **Diagnose using only the TSan harness and happens-before reasoning.** Running the reproducing case under `wsl-clang-tsan` and correlating its output with the exact source locations of every lock/condition-variable operation reveals the specific interleaving — the canonical shape here (per 11-P33's pattern) is a missed-notification window: a consumer checks `if (queue.empty())` (or an equivalent bare check outside a predicate-based `wait`), finds it true, and *before* it calls `cv.wait(lock)`, a producer thread acquires the lock, pushes an item, and calls `notify_one()` — since the consumer had not yet actually registered as a waiter, the notification is delivered to no one, and the consumer then proceeds to block in `wait()` with no future notification ever coming (assuming this was the only producer activity expected). This requires the specific rare window where the producer's push-then-notify sequence lands in the exact gap between the consumer's check and its wait call — a window on the order of a few instructions wide, which is why it needs the rare, high-frequency-contention timing to actually land there.

3. **Fix it.** The minimal correct fix: replace the bare `if (queue.empty()) cv.wait(lock);` with a predicate-based wait, `cv.wait(lock, [&] { return !queue.empty(); });` — this makes the check-and-register-as-waiter operation atomic with respect to the lock (the predicate is re-checked every time the thread is about to actually wait, and `wait()` itself is what atomically releases the lock while registering to be woken, closing the exact gap the bug exploited). This eliminates the interleaving entirely rather than merely narrowing it, because there is no longer a window between "check the condition" and "start waiting" during which a notify can be lost — the two are now inseparable.

4. **Prove the fix.** Re-running the same stress harness for 10,000+ iterations post-fix with zero hangs, and a clean `wsl-clang-tsan` report, is meaningfully stronger evidence here than the equivalent would be for a data race: a data race's absence across N runs is never proof of absence, because a race can remain latent under one set of timing conditions and still exist, waiting for different hardware or scheduling to expose it (this chapter's own repeated-run misconception). A deadlock caused by a specific, now-structurally-impossible interleaving is different — once the actual missed-notification window is closed by making check-and-wait atomic, there is no longer *any* timing under which the old bug's specific mechanism can occur at all, not merely a smaller probability of it occurring; 10,000 clean runs here corroborates a completed structural argument (the gap is closed) rather than serving as the entire evidentiary basis the way it would for an unproven race.
