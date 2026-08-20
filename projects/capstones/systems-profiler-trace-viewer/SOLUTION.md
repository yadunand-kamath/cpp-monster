# C-3 — Solution

No single canonical solution — this elective capstone's platform-specific sampling mechanisms and trace-format design are open. This sketches one credible reference path at design-and-key-snippets depth.

## Reference Architecture

### Per-thread lock-free ring buffer for instrumented tracing (Hint 2)

```cpp
struct TraceEvent {
    uint64_t timestamp_ns;
    uint32_t thread_id;
    uint16_t depth;         // nesting depth at time of begin — Phase 1
    EventKind kind;         // kBegin, kEnd, kSampledFrame
    const char* name;       // interned string or raw pointer to a literal — no allocation on hot path
    uintptr_t address;      // for sampled frames, pre-symbolization
};

class ThreadLocalRingBuffer {
public:
    void push(const TraceEvent& e) {
        buffer_[write_index_ % capacity_] = e;  // single writer (this thread only) — no lock, no atomic needed
        ++write_index_;
    }
    std::vector<TraceEvent> drain() const {     // called from analysis/export thread, after this thread is quiesced
        size_t count = std::min<size_t>(write_index_, capacity_);
        std::vector<TraceEvent> out(count);
        for (size_t i = 0; i < count; ++i)
            out[i] = buffer_[(write_index_ - count + i) % capacity_];  // oldest-surviving to newest
        return out;
    }
private:
    std::vector<TraceEvent> buffer_;
    size_t write_index_ = 0;
    size_t capacity_;
};

thread_local ThreadLocalRingBuffer g_ring_buffer{/*capacity=*/4096}; // Hint 2: per-thread, zero cross-thread writes

struct ScopeTracer {
    ScopeTracer(const char* name) : name_(name) {
        g_ring_buffer.push({now_ns(), current_thread_id(), depth(), EventKind::kBegin, name, 0});
        ++tls_depth;
    }
    ~ScopeTracer() {
        --tls_depth;
        g_ring_buffer.push({now_ns(), current_thread_id(), depth(), EventKind::kEnd, name_, 0});
    }
    const char* name_;
};
#define TRACE_SCOPE(name) ScopeTracer _trace_scope_##__LINE__(name)
```

### Platform-divergent sampling (Ch09-style paired presentation)

```cpp
// Linux: SIGPROF-driven sampler on the target thread, or a separate sampler thread using
// /proc/[tid]/stat or a signal sent to the target — captures via backtrace()/unw_backtrace(),
// restricted to async-signal-safe calls only if run inside the signal handler itself.
void linux_sample_thread(pid_t tid) {
    // sends SIGPROF to tid; handler calls backtrace() (must be async-signal-safe-compatible)
    // and pushes raw addresses into a lock-free queue read by a non-signal-context thread
}

// Windows: SuspendThread + GetThreadContext + StackWalk64 — no signal-equivalent; the sampler
// thread directly suspends the target thread, walks its stack while genuinely stopped, resumes it.
void windows_sample_thread(HANDLE thread) {
    SuspendThread(thread);
    CONTEXT ctx{}; ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(thread, &ctx);
    STACKFRAME64 frame{};  // seeded from ctx
    while (StackWalk64(/*...*/, &frame, &ctx, /*...*/)) capture_frame(frame.AddrPC.Offset);
    ResumeThread(thread);
}
```

## Design Rationale

**Why per-thread ring buffers with no cross-thread synchronization, rather than one shared buffer?** The instrumented-tracing hot path (Hint 2) runs on every traced scope entry/exit across every thread — any lock or atomic contention point on that path directly costs the 3% overhead budget stated in Phase 3. Giving each thread an independently-written buffer removes contention entirely at capture time, moving all cross-thread coordination to the far less frequent drain/export step, where its cost is amortized rather than paid per-event.

**Why is Linux's sampling mechanism fundamentally different in kind from Windows's, not just differently named?** Linux's signal-based approach samples a thread by interrupting it asynchronously — the sampled thread's own execution is briefly diverted into a signal handler, which must therefore obey async-signal-safety rules (no allocation, no locks, no non-reentrant libc calls) since it could have interrupted literally any code, including code already holding a lock the handler itself might need. Windows's `SuspendThread`-based approach genuinely stops the target thread from another thread's perspective before inspecting it — no signal-handler-safety constraint applies, but the suspending thread must be careful never to suspend a thread that might be holding a lock the *sampler itself* needs (a classic suspend-based-sampler deadlock risk). These are different correctness constraints, not just different APIs for "pause and look."

**Why symbolize after capture (batch), not during capture (live)?** Symbolization (parsing DWARF/PDB and resolving an address) is comparatively expensive relative to the timestamp-and-ring-buffer-write cost of a single trace event. Doing it live would make the hot path's cost dependent on debug-info-lookup latency, defeating the low-overhead goal; doing it as a batch post-processing step (per Hint 3) keeps the hot path cheap and lets identical repeated addresses share one cached lookup.

## Reference Implementation

Left to the learner: the full cross-thread merge/drain logic that reconstructs a single time-ordered trace from many independent per-thread ring buffers; the chosen DWARF/PDB parsing library integration and the symbol-cache layer around it; the sampler's queue/handoff mechanism from signal-handler or suspended-thread context back to normal processing context; the trace viewer's rendering logic (HTML/timeline export and flame-graph-style hot-path aggregation); and the on-disk trace file format if persistence is implemented.

## Testing Strategy

Test instrumented tracing (Phase 1) fully in isolation first — it's deterministic and doesn't depend on any platform-specific sampling mechanism, so its correctness (nesting, thread attribution, ring-buffer wraparound) can be nailed down with ordinary unit tests before the platform-divergent, harder-to-test sampling code (Phase 2) is introduced. Validate sampling and symbolization against a synthetic, deliberately-named deep call chain (per the STATEMENT.md's Phase 2 exit bar) rather than a real, arbitrary workload first — a known expected answer makes it possible to say definitively "the symbolizer is correct" rather than merely "the symbolizer produced plausible-looking output."

## Performance Analysis

Per Hint 4, the timestamp-acquisition call itself is a frequent and non-obvious overhead contributor — `std::chrono::steady_clock::now()` maps to a genuinely cheap instruction on some platforms (e.g. `RDTSC`-backed) and a comparatively expensive syscall-adjacent path on others; measuring this call's cost in isolation, on both target platforms, before attributing overhead to the ring buffer design is the first diagnostic step if the Phase 3 budget isn't met.

## Failure Modes

- A signal handler (Linux sampling) calling a non-async-signal-safe function (e.g. anything that allocates, or most of the C++ standard library) — can deadlock or corrupt state unpredictably, and only manifests under specific, hard-to-reproduce timing (interrupting code that happened to be inside a lock the handler's own path needs).
- A `SuspendThread` (Windows sampling) call that suspends a thread currently holding a lock the *sampling* thread itself needs later (e.g. a shared symbol-cache lock) — a suspend-based deadlock, avoided by ensuring the sampler thread never needs any lock that application threads might be holding at an arbitrary suspension point.
- Symbolizing live, during the hot path, "just this once for a quick win" — reintroduces exactly the overhead-budget risk the batch-symbolization design in the Rationale exists to avoid.

## Extensions

- Out-of-process profiling (attaching to and sampling a separate target process), a substantially harder but realistic extension of the in-process-only baseline scope.
- Continuous/always-on low-rate sampling suitable for production deployment, with an even tighter overhead budget than this capstone's stated target.
