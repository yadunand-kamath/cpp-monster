# Chapter 11 — Concurrency and the C++ Memory Model

## Crash Course

### Threads, jthread, and stop_token

`std::thread` launches a function on a new OS thread; forgetting to `join()` or `detach()` before the `thread` object destructs calls `std::terminate`. `std::jthread` (C++20) fixes the most common misuse by automatically joining in its destructor, and additionally carries a `std::stop_token`/`std::stop_source` pair: the thread's function can accept a `stop_token` and poll `token.stop_requested()` (or register a `std::stop_callback`) to cooperatively unwind, while the owner calls `request_stop()` — this is the standard, portable replacement for hand-rolled `std::atomic<bool> stop_flag` patterns, and it composes correctly with RAII since the token itself is a small copyable handle.

### Mutexes, Locks, and Condition Variables

A `std::mutex` guarantees mutual exclusion of a critical section; it must be paired with an RAII guard (`std::lock_guard`, or `std::unique_lock` when you need to unlock early, transfer ownership, or wait on a condition variable) rather than manual `lock()`/`unlock()`, because any exception or early return between a manual lock and unlock leaves the mutex held forever. `std::condition_variable::wait(lock, predicate)` atomically releases the lock while waiting and re-acquires it before returning, checking `predicate` in a loop to survive spurious wakeups — a `wait()` without a predicate is almost always a bug, since a thread can wake without anyone having satisfied the condition. `std::scoped_lock` (C++17) locks multiple mutexes simultaneously using a deadlock-avoidance algorithm, which is the correct tool whenever a critical section needs two or more locks at once — locking them separately, one at a time, is exactly the ordering hazard that causes deadlocks.

### Futures, Promises, and async

`std::promise<T>`/`std::future<T>` is a one-shot, one-value channel between a producer thread (which calls `promise.set_value(v)` or `set_exception(...)`) and a consumer (which calls `future.get()`, blocking until a value or exception arrives, and rethrows the exception in the consumer's thread if one was set). `std::async(policy, fn, args...)` packages "run this and give me a future" in one call; with `std::launch::async` it is guaranteed to run on a new thread, with `std::launch::deferred` it runs lazily on the calling thread at the first `.get()`/`.wait()` — mixing up which policy you asked for (or accepting the default, which lets the implementation choose) is a common source of "why didn't this run concurrently" bugs.

### Atomics, Data Races, and Happens-Before

A **data race** is two or more threads accessing the same memory location, at least one a write, with no synchronization establishing an ordering between them — this is undefined behavior in C++, not merely "the result is unpredictable." `std::atomic<T>` operations are individually indivisible and, critically, can additionally establish a **happens-before** relationship between threads when given the right memory order: a release store on one thread synchronizes with an acquire load of the same atomic on another thread that observes the stored value, and everything sequenced-before the release becomes visible to everything sequenced-after the acquire. Without an established happens-before edge, the compiler and hardware are free to reorder, cache, or elide memory operations in ways that make "obviously fine" code wrong.

### Memory Orders

C++ defines six memory orders, but they group into three practical tiers: `memory_order_relaxed` (atomicity only — no ordering guarantee with any other memory operation, useful for counters nobody synchronizes on); `memory_order_acquire`/`memory_order_release`/`memory_order_acq_rel` (the release-acquire pairing that builds a happens-before edge across exactly the two operations involved); and `memory_order_seq_cst` (the default — acquire-release semantics *plus* a single total global order agreed on by every thread for every `seq_cst` operation, which is easiest to reason about and is what you get if you never specify an order at all, at some real cost in synchronization overhead on multi-core hardware). `memory_order_consume` exists but is effectively unimplemented as specified by any major compiler and should not be used in new code. A memory-ordering bug is rarely reproducible on every run — it depends on cache-coherence timing that varies run to run, which is exactly why TSan (not "it passed 1000 times locally") is this chapter's actual grading tool.

### Lock-Free Programming

Lock-free code replaces mutexes with a loop around `compare_exchange_weak`/`compare_exchange_strong` (CAS): read the current value, compute a new value, and atomically swap only if nobody else changed it in between, retrying otherwise. `compare_exchange_weak` may fail spuriously even when the comparison would have succeeded (relevant on architectures without a native CAS, which retry via load-link/store-conditional) and should be used in a loop; `compare_exchange_strong` never fails spuriously but can cost more on such architectures. Lock-free code is not "faster by default" — it trades blocking (a thread waits) for a live but potentially wasted retry loop, and is only worth the complexity when profiling (Ch12) actually shows lock contention as the bottleneck, and even then it introduces its own hazards: the ABA problem (a value changes away and back before a CAS notices) is the canonical one, usually solved with a tagged/versioned pointer or hazard pointers.

### Safe Memory Reclamation: Hazard Pointers and Epoch-Based Reclamation

A lock-free structure's ABA mitigation (a tagged/versioned pointer) prevents a *stale CAS from succeeding incorrectly*, but it doesn't answer a separate, equally hard question: when is it actually safe to `delete`/free a node that one thread has unlinked, if some *other* thread might still be mid-dereference of a raw pointer to that same node, read just before the unlink happened? Freeing too early is a use-after-free race that no amount of ABA-safe CAS logic prevents, because the hazard is a reader holding a plain pointer, not a second CAS on the same memory. Two standard techniques solve this specific problem:

**Hazard pointers:** before dereferencing a shared pointer read from the lock-free structure, a thread publishes that pointer into a well-known, per-thread "hazard pointer" slot other threads can see. A thread wanting to reclaim (free) a node first scans every other thread's hazard-pointer slots; if no one has published that address, it's provably safe to free — if someone has, reclamation is deferred (typically onto a small retire-list, retried later) until it's no longer hazardous. The cost is a scan over all active threads' hazard slots on every reclamation attempt, and every reader must remember to publish/clear its hazard pointer around every access — an easy step to forget, silently reintroducing the exact use-after-free the technique exists to prevent.

**Epoch-based reclamation (EBR):** instead of tracking specific pointers, every thread periodically announces which global "epoch" (a monotonically increasing counter) it's currently operating in, by entering/leaving critical sections that bump a per-thread epoch marker. A node unlinked during epoch *N* is only actually freed once every thread's marker has advanced *past* epoch *N* — proving no thread could still hold a reference read during or before that epoch. This trades hazard pointers' per-pointer bookkeeping for a coarser, cheaper per-thread counter, at the cost of potentially longer reclamation delays (a single slow or stalled thread holds back reclamation for every unlinked node, across the whole structure, until it advances) — a tradeoff hazard pointers don't have, since hazard pointers' delay is scoped to the specific node a slow thread is actually touching.

Both techniques exist to answer the same question ("when is this unlinked node truly unreachable by any in-flight reader?") with different granularity/cost tradeoffs — neither is a drop-in replacement for the ABA-mitigating tagged pointer discussed above; a real lock-free structure with dynamic node lifetimes typically needs *both* an ABA-safe CAS discipline and a reclamation scheme like one of these two, addressing two genuinely separate hazards that happen to both surface in the same lock-free-stack-style code.

### Thread Pools, Queues, and Producer/Consumer

A thread pool is a fixed set of worker threads that pull work items from a shared queue rather than spawning a new OS thread per task — this amortizes thread-creation cost and bounds concurrency. A bounded producer/consumer queue must handle both "queue full" (producers block or are rejected) and "queue empty" (consumers block) without deadlocking, typically via a mutex plus two condition variables (not-full and not-empty) or a lock-free ring buffer with atomic head/tail indices. A work-stealing pool gives each worker its own deque and lets an idle worker "steal" from the back of a busy worker's deque, which reduces contention on a single shared queue at the cost of more complex per-worker bookkeeping.

### Deadlock, Starvation, Livelock, and False Sharing

Deadlock requires all of: mutual exclusion, hold-and-wait, no preemption, and a circular wait — break any one (most practically, impose a global lock-acquisition order, or use `std::scoped_lock` for multi-lock acquisition) and deadlock becomes impossible by construction. Starvation is a thread that is technically able to proceed but never actually gets scheduled/granted the resource, often because a naive lock or scheduler favors other threads indefinitely — different from deadlock (nobody's stuck, one thread is just perpetually unlucky). Livelock is two or more threads actively changing state in response to each other, forever, without net progress (e.g. two threads each backing off and retrying in a way that keeps colliding) — busy, but as unproductive as deadlock. False sharing is a correctness-adjacent performance hazard where two unrelated atomics happen to share a cache line, so every write from one thread invalidates the cache line for a thread only touching the *other* atomic — `std::hardware_destructive_interference_size` and explicit padding are the standard mitigation; this chapter introduces false sharing as a correctness/contention risk, Ch12 measures its actual cost.

### latch, barrier, and semaphore

`std::latch` is a single-use countdown gate: threads call `count_down()`, and any thread can `wait()` until the count reaches zero — useful for "wait until N workers have finished initializing." `std::barrier` is the reusable version: it cycles through phases, releasing all waiting threads together each time the count reaches zero and then resetting, and optionally runs a completion function on one thread between phases. `std::counting_semaphore<Max>`/`std::binary_semaphore` generalize a mutex into a resource count greater than one, and unlike a mutex, a semaphore's `release()` need not be called by the thread that called `acquire()` — useful for signaling between producer and consumer threads that aren't using a shared predicate.

## Common Misconceptions

1. **"If my code passes 1000 test runs, it doesn't have a data race."** A data race's manifestation depends on cache-coherence and scheduling timing that varies run to run, hardware to hardware, and optimization level to optimization level — passing N runs on one machine establishes nothing about the next run on a different core count or under a debugger's altered timing. Only a tool that observes memory accesses directly (TSan) or a correctness argument from the standard's happens-before rules can establish race-freedom; repeated success is not evidence.

2. **"`volatile` prevents data races / provides thread synchronization."** `volatile` in C++ only prevents the compiler from eliding or reordering accesses to that specific variable *within a single thread's view* (it exists for hardware registers and signal handlers) — it says nothing about atomicity, memory ordering across threads, or happens-before. Two threads incrementing a `volatile int` without an atomic or a mutex is still a data race and still undefined behavior.

3. **"`memory_order_seq_cst` is always correct, so I should never need to think about acquire/release."** `seq_cst` is always *correct* (it's the strongest order), but defaulting to it everywhere is not free — it forces a total global order across every `seq_cst` operation on every thread, which costs real synchronization overhead on multi-core hardware compared to a targeted acquire/release pairing that only orders the two operations that actually need to see each other. Using `seq_cst` as a safe starting point while learning is reasonable; treating it as the only order worth knowing is not.

4. **"A `std::atomic<T>` makes any operation on `T` atomic, including compound reads of multiple atomics."** Each individual atomic operation (a single load, store, or CAS on one atomic object) is atomic; a sequence of two separate atomic operations (read atomic A, then read atomic B) is not atomically a single unit unless something additionally guarantees no other thread can observe an inconsistent combination of A and B in between.

5. **"Lock-free is strictly better than lock-based, because locks are slow."** Lock-free code trades blocking for a retry loop under contention, adds substantial complexity (ABA, memory reclamation, correctness proofs that are genuinely hard to get right), and is frequently *slower* than a well-designed lock-based structure under low-to-moderate contention because CAS retries under contention can burn more cycles than a short-held lock. The decision belongs in Ch12's territory — measure, don't assume.

6. **"Deadlock is rare in practice, so lock ordering discipline is optional for a small codebase."** Deadlock frequency has nothing to do with codebase size and everything to do with whether two or more locks are ever acquired in inconsistent order anywhere in the program — a two-function, two-mutex codebase can deadlock exactly as reliably as a million-line one, and often does so only under load (rare interleavings), which is precisely why it survives code review and manual testing.

7. **"`condition_variable::wait()` without a predicate is fine as long as I only ever `notify_one()` when the condition is actually true."** Spurious wakeups (a thread waking with no corresponding `notify`) are permitted by the standard on real implementations, and even without spurious wakeups, a woken thread can lose a race to reacquire the lock against another thread that changes the condition back before it checks — `wait()` must always be called with a predicate (or in an explicit `while` loop re-checking the condition after every wake).

8. **"A tagged/versioned pointer that solves the ABA problem also makes it safe to free an unlinked node immediately."** No — ABA mitigation and safe memory reclamation solve two different hazards. A tagged pointer stops a *stale CAS* from spuriously succeeding on recycled memory; it says nothing about whether some other thread still holds a live, published, or in-flight raw pointer to the node you're about to free. Freeing immediately after unlinking, even with a fully ABA-safe CAS discipline, is still a use-after-free race unless a reclamation scheme (hazard pointers, epoch-based reclamation) has separately proven no reader is still in flight.

## Quick Checks

**11-QC1.** What specific guarantee does `std::jthread` provide over `std::thread` that eliminates the most common `std::thread` misuse?

**11-QC2.** Why must a condition variable's `wait()` be given a predicate (or wrapped in a `while` loop checking one) rather than called bare?

**11-QC3.** Define a data race precisely enough to distinguish it from "two threads touching the same variable."

**11-QC4.** What does a release store paired with an acquire load on the same atomic actually guarantee about memory operations that are not themselves atomic?

**11-QC5.** Why is `memory_order_relaxed` ever useful, given that it provides no ordering guarantee at all?

**11-QC6.** What is the ABA problem, and why does a plain CAS loop not detect it on its own?

**11-QC7.** Give the four conditions jointly necessary for deadlock, and name one practical technique that breaks one of them by construction.

**11-QC8.** Why is "passed 1000 CI runs" not evidence of race-freedom, in terms of what a data race's manifestation actually depends on?

**11-QC9.** What hazard does hazard-pointer/epoch-based reclamation solve that an ABA-safe tagged pointer does not?

**11-QC10.** In epoch-based reclamation, what specifically has to be true before a node unlinked during epoch *N* can be freed?

## Problems

### Level 1 — Recognition

**11-P01.** Given two code snippets — one spawning a `std::thread` that is neither joined nor detached before the enclosing scope ends, and one spawning a `std::jthread` in the identical situation — state which one calls `std::terminate` and which one safely joins automatically, and why.

---

**11-P02.** Given a critical section guarded by `mtx.lock(); /* ... */ mtx.unlock();` with a `return` statement in the middle of the critical section, identify the bug this pattern is prone to and name the RAII type that fixes it unconditionally.

---

**11-P03.** Given `std::async(std::launch::deferred, fn)` versus `std::async(std::launch::async, fn)`, state which one is guaranteed to run `fn` on a separate thread and which one may run `fn` on the calling thread, lazily, only when `.get()` or `.wait()` is first called.

---

**11-P04.** Given a snippet where two threads increment a plain (non-atomic) `int counter` with no mutex or atomic involved, state whether this is well-defined, undefined, or implementation-defined behavior, and name the exact rule being violated.

---

**11-P05.** Given the six C++ memory orders listed (`relaxed`, `consume`, `acquire`, `release`, `acq_rel`, `seq_cst`), identify which one is effectively unimplemented by major compilers and should not be used in new code, and which one is the default when no order is specified.

### Level 2 — Prediction

**11-P06.** A `std::latch done(3)` is created, three worker threads each call `done.count_down()` upon finishing initialization, and a fourth thread calls `done.wait()`. Predict what happens if a fifth, unplanned worker thread is accidentally also given a reference to `done` and calls `count_down()` an extra time beyond the three expected calls, given that `std::latch` is single-use and its counter cannot go negative in a well-formed program — state what UB or observable misbehavior this over-decrement risks.

---

**11-P07.** Two threads each try to acquire `mutexA` then `mutexB`, but thread 1 acquires them in the order A-then-B while thread 2 acquires them in the order B-then-A, using plain `lock()` calls (not `std::scoped_lock`). Predict the failure mode under the interleaving where thread 1 has acquired A and thread 2 has acquired B, each now waiting on the other's mutex.

---

**11-P08.** A producer thread calls `promise.set_value(42)` and a consumer thread later calls `future.get()`. Separately, a second scenario has the producer thread throw an exception that propagates out of the function running under `std::async` before any `set_value`/`set_exception` call happens explicitly. Predict what `future.get()` returns/does in each of the two scenarios.

---

**11-P09.** A counter is incremented by 8 threads, 100,000 times each, using `counter.fetch_add(1, std::memory_order_relaxed)`. Predict whether the final value of `counter` is guaranteed to be exactly 800,000, and explain what guarantee `memory_order_relaxed` does and does not provide here.

---

**11-P10.** A flag `std::atomic<bool> ready{false}` is set with `ready.store(true, std::memory_order_release)` by a producer after writing to a plain (non-atomic) `std::vector<int> data`, and a consumer spins on `while (!ready.load(std::memory_order_acquire)) {}` before reading `data`. Predict whether the consumer is guaranteed to see the producer's writes to `data`, and explain in terms of the happens-before edge this release/acquire pair establishes.

---

**11-P11.** The same scenario as 11-P10, but the producer uses `memory_order_relaxed` for the store and the consumer uses `memory_order_relaxed` for the load instead of release/acquire. Predict whether the consumer is still guaranteed to see the producer's writes to `data` once it observes `ready == true`, and explain why relaxed ordering is insufficient here even though the atomic operation itself is still atomic.

---

**11-P12.** A lock-free stack's `push` uses a CAS loop: read `head`, build a new node pointing at the read `head`, then `compare_exchange_weak(head, new_node)`. Predict what happens to a thread's retry loop under high contention from many concurrent pushers, in terms of how often the CAS is expected to fail and retry versus succeed on the first attempt.

---

**11-P13.** A `std::binary_semaphore sem{0}` is used so that thread A calls `sem.acquire()` and thread B, entirely unrelated to A's call stack, later calls `sem.release()`. Predict whether this is valid usage of a semaphore, and contrast it with whether the equivalent using a `std::mutex` (locked by A, "unlocked" by B) would be valid.

---

**11-P14.** A work-stealing thread pool has each worker pop tasks from the front of its own deque and, when its own deque is empty, attempt to steal from the back of another randomly chosen worker's deque. Predict what happens to overall throughput if all workers instead stole from the front of others' deques (the same end the owner pops from), in terms of contention.

---

**11-P15.** A bounded queue of capacity 4 is implemented with a single mutex and a single condition variable (`not_full`), but the implementer forgot a second condition variable for the empty case, instead having consumers busy-spin-check `while (queue.empty()) {}` without ever releasing the mutex during the spin. Predict what happens to producers attempting to push while a consumer is spin-waiting like this.

---

**11-P16.** Four threads each call `std::scoped_lock lock(mtxA, mtxB, mtxC)` in that exact argument order, with no thread ever locking any subset of these three mutexes individually or in a different order. Predict whether this program can deadlock on these three mutexes, and explain what `std::scoped_lock`'s multi-mutex constructor guarantees that a naive sequence of three individual `lock()` calls does not.

---

**11-P17.** A thread pool's task queue holds `std::function<void()>` tasks, and one submitted task throws an uncaught exception during execution inside a worker thread, with no try/catch anywhere in the worker's run loop. Predict what happens to the rest of the pool's workers and any tasks still queued.

---

**11-P18.** Two atomics, `a` and `b`, happen to be laid out on the same 64-byte cache line by the compiler/allocator, and two unrelated threads each hammer one of the two atomics in a tight loop with no logical relationship between the two variables. Predict what happens to each thread's throughput compared to if `a` and `b` were on separate cache lines, and name the phenomenon.

---

**11-P19.** A `std::future<T>` obtained from `std::async` is never `.get()`'d or `.wait()`'d, and the `future` object simply goes out of scope while the asynchronous task is still running. Predict whether the destructor blocks, and contrast this with a `future` obtained from a `std::packaged_task` under the same circumstance.

### Level 3 — Implementation

**11-P20.**
Write a producer/consumer pair using one `std::mutex` and two `std::condition_variable`s (`not_full`, `not_empty`) around a fixed-capacity `std::deque<int>` acting as a bounded queue of capacity 8. The producer pushes integers 0..99; the consumer pops and sums them. Verify the consumer's final sum equals the expected total and that the program terminates cleanly (producer and consumer both `join()`), run repeatedly under `ctest` to build confidence against nondeterministic hangs.

---

**11-P21.**
Implement the same bounded queue from 11-P20 as a class `BoundedQueue<T>` with `push(T)`/`T pop()` member functions encapsulating the mutex/condition-variable pair, and add a `try_push`/`try_pop` pair using `wait_for` with a short timeout that returns a `bool` indicating success rather than blocking indefinitely. Write a GoogleTest case exercising both the blocking and the timeout-based non-blocking paths.

---

**11-P22.**
Write a function `void increment_many(std::atomic<int>& counter, int times)` and launch 8 `std::jthread`s each calling it with `times = 100000` on a shared counter initialized to 0, using `memory_order_relaxed` for the `fetch_add`. Verify the final counter value is exactly 800,000, and explain in a comment why relaxed suffices here specifically (each operation is independently atomic and none of the threads need to observe each other's intermediate values, only the final total).

---

**11-P23.**
Implement a `SpinLock` class using a single `std::atomic<bool>` and a CAS loop (`compare_exchange_weak` with an exponential or `std::this_thread::yield()`-based backoff), exposing `lock()`/`unlock()`/`try_lock()` so it satisfies the `BasicLockable` requirements and can be used with `std::lock_guard<SpinLock>`. Verify correctness with a shared counter test (many threads incrementing a plain non-atomic int under the spinlock) and confirm the final count is exact.

---

**11-P24.**
Using a `std::atomic<bool> ready` and release/acquire ordering exactly as in 11-P10, write a small program where a producer thread fills a `std::vector<int>` of 1000 elements and then signals `ready`, and a consumer thread waits on `ready` before summing the vector. Run this under the `wsl-clang-tsan` preset from Ch10 and confirm it reports clean (no race), then deliberately change both operations to `memory_order_relaxed` and confirm whether TSan still reports clean or now reports a race (note: TSan detects the race pattern, not merely "wrong answer," so document what you actually observe).

---

**11-P25.**
Implement `std::latch`-based worker-startup synchronization: spawn 4 worker `std::jthread`s that each perform a fake "initialization" (a short sleep), then `count_down()` a shared `std::latch init_done(4)`; the main thread calls `init_done.wait()` before proceeding to signal all workers (via a shared `std::atomic<bool> go` or a `std::stop_token`) to begin their main loop. Verify, via an ordering assertion (e.g. a shared atomic timestamp/counter check), that no worker begins its main loop before all 4 have completed initialization.

---

**11-P26.**
Write a minimal work queue backed by `std::queue<std::function<void()>>` guarded by a mutex and condition variable, plus a fixed pool of 4 `std::jthread` workers pulling from it, accepting a `std::stop_token` so `request_stop()` on the pool causes all workers to drain remaining queued tasks and then exit cleanly (not abandon queued work). Submit 50 no-op-counting tasks, request stop, join all workers, and verify all 50 tasks' completion counter reached 50 before any worker exited.

---

**11-P27.**
Implement a two-mutex resource-transfer scenario (e.g. two `Account` objects each with their own `std::mutex`, and a `transfer(Account& from, Account& to, int amount)` function) using `std::scoped_lock(from.mtx, to.mtx)` rather than sequential individual locking, and write a stress test spawning many threads calling `transfer` in both directions between the same two accounts concurrently. Run under `wsl-clang-tsan` and confirm no deadlock and no race is reported across at least 10,000 transfer calls.

---

**11-P28.**
Write a `std::counting_semaphore<N>`-based bounded resource pool (e.g. simulating N=3 database connections): `acquire()` blocks if all 3 are in use, `release()` returns one to the pool, and a helper RAII `ConnectionLease` calls `acquire()` in its constructor and `release()` in its destructor. Stress-test with 10 threads each requesting a connection, holding it briefly, and releasing it, verifying via an atomic "currently in use" counter that it never exceeds 3.

---

**11-P29.**
Implement `std::barrier`-based phased computation: 4 worker threads each compute a partial sum over a disjoint slice of a shared array, arrive at a `std::barrier` with a completion function that combines the 4 partial sums into a running total, then proceed to a second phase reading that combined total. Verify the combined total after phase 1 is correct before any thread proceeds into phase 2, using an assertion inside phase 2 itself (not just a final check) to catch a thread that raced ahead.

**11-P30.**
Build a readers-writer pair using `std::shared_mutex`: multiple reader threads call `shared_lock`, a single writer thread calls `unique_lock`, protecting a shared `std::map<int, std::string>`. Write a stress test with 10 concurrent readers continuously looking up keys and 2 writers periodically inserting new keys, and verify under `wsl-clang-tsan` that no reader ever observes a torn/partial insert (e.g. a key present but its value not yet fully constructed) and that the run is race-clean.

---

**11-P31.**
Implement a one-shot `std::promise`/`std::future` pipeline chaining three stages: stage 1 computes a value and fulfills `promise<int> p1`, a second thread's task waits on `p1`'s future, transforms the value, and fulfills its own `promise<int> p2`, and a third thread waits on `p2`'s future and produces the final result. Verify the pipeline produces the correct final transformed value, and verify that if stage 1 throws instead of calling `set_value`, the exception correctly propagates through to the final `.get()` call rather than being silently lost.

### Level 4 — Debugging

**11-P32.** [DEBUG] A program spawns a `std::thread` inside a function, does not call `join()` or `detach()` on it, and the `thread` object goes out of scope at the end of the function while the thread may still be running. Running the program crashes with a message about `std::terminate`. Diagnose the exact cause and give two possible fixes (one minimal, one that changes the design), including why simply calling `detach()` is a fix that trades one problem for a different, more subtle one (loss of ownership/lifetime tracking of the detached thread).

---

**11-P33.** [DEBUG] A condition-variable-based producer/consumer occasionally (roughly 1 in a few thousand runs) hangs forever with the consumer stuck in `wait()`. The code calls `cv.wait(lock)` with no predicate argument, inside an `if (queue.empty())` check rather than a `while` loop. Diagnose the race between the `if` check and the actual wait (a producer can push and notify between the check and the wait call, or a spurious wakeup can occur), and state the fix.

---

**11-P34.** [DEBUG] A shared `std::atomic<bool> data_ready` is set via `store(true, std::memory_order_relaxed)` by a producer after writing a large struct to a plain (non-atomic, non-mutex-guarded) shared pointer's pointee, and a consumer, after observing `data_ready.load(std::memory_order_relaxed) == true`, reads fields from that struct and occasionally observes garbage/inconsistent values despite `data_ready` correctly reading `true`. Diagnose why relaxed ordering is insufficient here even though the boolean flag itself is never wrong, and state the precise fix (which memory orders, on which two operations).

---

**11-P35.** [DEBUG] Two threads each acquire two mutexes, `mtxA` and `mtxB`, but thread 1 does `lock_guard<mutex> g1(mtxA); lock_guard<mutex> g2(mtxB);` while thread 2, in a different function far away in the codebase, does `lock_guard<mutex> g1(mtxB); lock_guard<mutex> g2(mtxA);` for an unrelated-seeming reason. The program deadlocks intermittently under load, never in single-threaded testing. Diagnose the lock-ordering inconsistency across the two call sites and state the fix, explaining why this bug is specifically the kind that survives ordinary testing and code review (it requires the exact unlucky interleaving to manifest).

---

**11-P36.** [DEBUG] A lock-free stack's `pop()` reads `head`, reads `head->next`, and then does `compare_exchange_weak(head, head->next)` — but between the two reads, another thread pops the same node, frees it, and a third thread pushes a brand-new node that happens to be allocated at the same freed address. The CAS on the first thread then spuriously succeeds because `head` still compares equal to the (recycled) pointer value, corrupting the stack. Diagnose this as the ABA problem specifically (not a generic race), and describe one standard mitigation (tagged/versioned pointers, or a hazard-pointer/epoch-based reclamation scheme) at a conceptual level.

---

**11-P37.** [DEBUG] A thread pool's worker loop calls `task()` directly on each popped `std::function<void()>` with no surrounding try/catch, and a submitted task throws `std::runtime_error`. The entire process terminates via an uncaught-exception crash, taking down all in-flight work in every other worker too, not just the one task. Diagnose why an exception escaping one worker's task execution brings down the whole pool rather than just failing that one task, and state the fix (where exactly the try/catch belongs and what should happen to the exception — e.g. captured and reported back via the task's own future/promise).

---

**11-P38.** [DEBUG] A `std::latch` is constructed with an expected count of 4 for 4 worker threads, but due to a refactor, a 5th worker thread was added to the spawning code without updating the latch's construction argument, so `count_down()` is now called 5 times against a latch expecting only 4. The program's behavior on the 5th, extra `count_down()` call is undefined per the standard (decrementing below zero). Diagnose the root cause (a magic-number count decoupled from the actual thread-spawning code) and propose a fix that structurally prevents this class of drift (e.g. deriving the latch's count directly from the same variable/loop bound used to spawn the workers).

---

**11-P39.** [DEBUG] A benchmark comparing a mutex-protected counter against a lock-free CAS-loop counter under 32-way contention shows the "lock-free" version running measurably *slower* than the mutex version, contrary to the developer's expectation that lock-free is always faster. Diagnose why heavy CAS contention can be slower than a short critical section under a mutex (repeated failed CAS retries burning CPU cycles versus a mutex's OS-assisted blocking/wake), and state what this benchmark result should actually change about the developer's design decision (nothing here, without also considering Ch12's profiling discipline, justifies switching back purely from folklore either way — the fix is to keep measuring under the actual expected contention level).

---

**11-P40.** [DEBUG] A `std::shared_mutex`-protected map has readers taking `shared_lock` and writers taking `unique_lock`, but under a continuous stream of readers, a waiting writer is observed to starve for multiple seconds in a stress test even though each individual reader's critical section is short. Diagnose why an implementation that always prefers granting new shared locks over a waiting exclusive lock can starve writers indefinitely, and state a fix (e.g. a fairness policy that stops admitting new readers once a writer is waiting).

---

**11-P41.** [DEBUG] A work-stealing thread pool deadlocks under specific task-dependency patterns: task A, running on worker 1, blocks (via `future.get()`) waiting on task B's result, but task B is still sitting in worker 1's own queue, never picked up because worker 1 is blocked inside task A and no other worker happens to steal it in this particular run. Diagnose this as a scheduling/dependency deadlock distinct from a lock-ordering deadlock (no mutex is involved at all), and describe a structural fix (e.g. never blocking a worker thread on a future for a task that might still be unscheduled — using continuation-passing or having the blocked worker itself help drain the queue while waiting).

### Level 5 — Integration

**11-P42.**
Build a complete bounded MPMC (multi-producer, multi-consumer) queue supporting multiple producer and multiple consumer threads concurrently, implemented two ways behind the same interface: (a) a mutex-plus-two-condition-variables version, and (b) a lock-free ring-buffer version using atomic head/tail indices with `memory_order_acquire`/`release` at the right points. Stress-test both under `wsl-clang-tsan` with at least 4 producers and 4 consumers pushing/popping 100,000 total items, verifying every item is received exactly once (no duplication, no loss) and TSan reports clean for both implementations.

---

**11-P43.**
Build a work-stealing thread pool: N worker threads, each with its own lock-free (or mutex-protected, your choice, but documented) deque, submitting tasks that may themselves submit further tasks (recursive task submission, e.g. a parallel Fibonacci or parallel merge sort), with idle workers stealing from busy workers' deques. Verify correctness (the recursive computation produces the right answer) and verify, via a submitted-vs-completed task counter, that every submitted task actually completes exactly once, with graceful shutdown via `stop_token` once all tasks drain.

---

**11-P44.**
Diagnose and fix a producer/consumer system (supplied as a working-but-flawed starting implementation using a queue, mutex, and condition variables) that deadlocks under load roughly once per ten thousand runs — using only the `wsl-clang-tsan` preset and happens-before reasoning, not source-level guessing. Document the exact interleaving TSan's report (or a targeted stress harness looping the program under load) reveals, the specific fix, and a re-run of at least 10,000 iterations post-fix showing zero hangs.

---

**11-P45.**
Take the exception-unsafe thread pool from 11-P37/11-P17 and rebuild it so every submitted task's exceptions are captured via `std::promise::set_exception` and surfaced through the task's own `std::future`, such that one task throwing never terminates the pool or affects any other task's execution or result. Verify with a mixed batch of 100 tasks where every 10th one deliberately throws, confirming the other 90 complete normally and the 10 throwing ones' futures correctly rethrow on `.get()`.

---

**11-P46.**
Build a small parallel `for_each`-style utility, `parallel_for(first, last, chunk_size, fn)`, that partitions a range into contiguous chunks, submits one task per chunk to a thread pool (from 11-P26 or 11-P43), and blocks until all chunks complete — using `std::latch` (sized to the chunk count) rather than joining raw threads. Verify against a single-threaded reference implementation across several input sizes (including sizes not evenly divisible by `chunk_size`) that results match exactly.

---

**11-P47.**
Build a readers-writer synchronization primitive from scratch (not `std::shared_mutex`, to demonstrate understanding of the underlying construction) using a mutex, a condition variable, and reader/writer counters, allowing multiple concurrent readers but exclusive writers, without starving writers indefinitely under a continuous stream of readers (a fairness policy of your choice, documented). Stress-test with many reader threads and a smaller number of writer threads under `wsl-clang-tsan`, and specifically verify (via an atomic counter check inside the critical section) that no writer ever executes concurrently with any reader or another writer.

---

**11-P48.**
Instrument the MPMC queue from 11-P42 with false-sharing awareness: pad the head and tail atomic indices to separate cache lines using `alignas(std::hardware_destructive_interference_size)`, and build a small before/after comparison harness (using Ch10's benchmark scaffolding) demonstrating that unpadded head/tail indices sharing a cache line cause measurably higher contention under many-producer/many-consumer load than the padded version — treating this problem's correctness angle (no race either way) as separate from Ch12's later measured-cost treatment of the same phenomenon.

### Level 6 — Production

**11-P49.** Your team's codebase has 40+ places using raw `std::thread` with manual `join()` calls scattered across destructors, `catch` blocks, and early-return paths, and a recent incident traced back to one `std::terminate` crash from a missed `join()` on an exception path. Propose a migration policy: whether to mass-replace with `jthread`, what static-analysis or lint rule (if any) could catch a raw `std::thread` introduced after the policy takes effect, and how you'd sequence the migration without requiring a single big-bang PR touching all 40+ sites at once.

---

**11-P50.** A production service's thread pool occasionally exhibits a multi-second latency spike that correlates loosely with load but has never been reproduced under the TSan preset or any stress test run so far — TSan reports clean on every run attempted. Propose a diagnostic strategy for a suspected performance/scheduling issue (not necessarily a data race) that TSan is not designed to catch at all, including what tools or techniques (beyond TSan) you would reach for, and why "TSan is clean" cannot be used as evidence against a latency/contention hypothesis.

---

**11-P51.** Your organization is deciding whether to standardize on lock-based or lock-free implementations for all new shared-queue code going forward, based on one engineer's benchmark showing a lock-free version winning by 15% in a microbenchmark with 2 threads. Evaluate whether this microbenchmark result justifies a blanket policy, identify what additional information (contention level in production, thread count, task granularity, maintenance cost of lock-free correctness) should inform the actual decision, and propose a policy that avoids both extremes (always lock-free, never lock-free).

---

**11-P52.** A postmortem reveals that a deadlock reached production despite "passing all tests," because the specific lock-acquisition interleaving that triggers it requires two particular threads to be scheduled within a narrow, rare window that never occurred in CI's lower-core-count runners. Design a concrete process change (not a code fix for this one instance) that would raise the odds of catching this *class* of bug before production — addressing both detection (what kind of testing/tooling surfaces rare interleavings) and prevention (what design discipline reduces the number of possible interleavings that could deadlock in the first place).

### Level 7 — Principal Reasoning

**11-P53.** Design the concurrency architecture for a service that must process a high-throughput stream of independent requests with strict p99 latency requirements, where you must choose between: a thread-per-connection model, a fixed thread pool with a shared queue, and a work-stealing pool. Justify your choice against the specific latency requirement (not "which is generally best"), and identify the memory-ordering and synchronization discipline you would mandate for the chosen model's shared state, plus what you would explicitly forbid (e.g. "no `seq_cst` used as a substitute for reasoning about the actual ordering needed") in a codebase multiple engineers will maintain after you.

---

**11-P54.** Your organization ships a library exposing a thread pool as part of its public API, used by customers across a multi-year support window, some of whom will call into it from their own already-multithreaded applications. Design the thread-safety contract you would document and enforce at the API boundary (which operations are safe to call concurrently, which are not, how exceptions thrown inside customer-supplied task callables are handled and surfaced), and identify one thread-safety guarantee you would deliberately *not* make (accepting the corresponding limitation) in exchange for implementation simplicity or performance — justify that tradeoff concretely.

---

**11-P55.** A principal engineer on your team proposes replacing a correctness-critical, extensively reviewed lock-based data structure with a lock-free equivalent purely on the grounds that "lock-free is more modern and this is a high-traffic path." Write the design-review response: what evidence you would require before approving the change (measured contention data, not intuition), what verification burden a lock-free replacement specifically incurs that the lock-based version didn't (TSan across all memory orders used, a documented ABA/reclamation argument, stress-testing at production-scale contention, not just correctness at low contention), and under what circumstances you would approve the change versus push back on it.

### Level 5 — Integration (continued: safe memory reclamation)

**11-P56.** Extend the lock-free stack from 11-P36 (or build a minimal one) with a working epoch-based reclamation scheme: each thread announces entry/exit around any operation that dereferences a node pointer read from the stack, and a retire-list defers `delete` on unlinked nodes until every thread's announced epoch has advanced past the epoch the node was unlinked in. Demonstrate, via a stress test with concurrent poppers and one thread retiring nodes, that no node is ever freed while another thread might still be dereferencing it — and separately demonstrate (as a contrasting failure case, e.g. in a comment or a deliberately-broken build flag) what observably goes wrong (TSan report, or a crash under `-fsanitize=address`) if nodes are freed immediately on unlink with no reclamation scheme at all. State the one concrete cost this scheme adds that the original 11-P36 tagged-pointer-only version didn't have (a global epoch counter, per-thread announcement overhead, and reclamation latency tied to the slowest active thread).

## Integration Challenge — 11-IC1

You are given a producer/consumer system — multiple producer threads pushing work items into a shared bounded queue, and multiple consumer threads popping and processing them — that deadlocks under sustained load roughly once every ten thousand runs, and never under light load or in a debugger (which perturbs timing enough to avoid the interleaving). You are told only that it "works fine most of the time" and given the source and the `wsl-clang-tsan` preset from Ch10.

1. **Reproduce reliably.** Design a stress harness that runs the producer/consumer system repeatedly under representative load until the deadlock manifests, without relying on luck — state what "representative load" means here (thread counts, queue capacity, item volume) and why light load fails to trigger it.
2. **Diagnose using only the TSan harness and happens-before reasoning — no source-level guessing.** Identify the exact interleaving (which threads, which locks/condition-variables, in what order) that produces the deadlock, and explain why it requires the specific rare timing window it does.
3. **Fix it.** State the minimal correct fix (e.g. a `wait()` predicate omission, a lock-ordering inconsistency across two call sites, a missed `notify` under a specific interleaving) and explain why the fix eliminates the interleaving rather than merely making it statistically rarer.
4. **Prove the fix.** Re-run the stress harness for at least 10,000 iterations post-fix with zero hangs, and re-run under `wsl-clang-tsan` confirming a clean report — state why "zero hangs in 10,000 runs" is meaningfully stronger evidence here than it would be for a data race (per this chapter's own misconception about repeated-run evidence), given that a deadlock, once its triggering condition is truly removed, cannot recur at all, unlike a race whose absence in N runs is never proof of absence.

## Chapter Project

This chapter feeds directly into:
- **P-4.2 Bounded MPMC Queue** — its lock-based and lock-free variants are built and verified directly on 11-P20, 11-P21, and 11-P42.
- **P-4.3 Work-Stealing Thread Pool** — builds directly on 11-P26, 11-P43, and 11-P45's exception-safe task/future design.
- **P-5.3 Concurrent TCP Protocol Server** — consumes this chapter's thread-pool and backpressure (bounded-queue) discipline as its concurrency substrate.
