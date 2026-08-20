# GP-1 — Reference Solution

## Reference Architecture

```
                         +------------------+
   Historical tape  ---> |                  |
                         |   Event Stream    |---> EventLoop (single-threaded,
   Live feed adapter --> |  (ordered ticks,  |      deterministic, drives Clock)
   (WebSocket, read-only)|  order acks,      |          |
                         |  timer events)    |          v
                         +------------------+     IStrategy (Strategy A / B)
                                                        |
                                                        v  OrderIntent(s)
                                                   RiskGate (8 ordered checks)
                                                        |
                                                        v  (if passed)
                                                  IBrokerAdapter
                                                   /          \
                                          SimulatedBroker   RealAdapter
                                          (default, fills   (sandbox only,
                                           against feed)     Phase 5, market-
                                                              data adapters are
                                                              read-only outside
                                                              this)
```

`Backtester` and `LiveHarness` are two thin drivers that both do the same thing: pull events from a source (a file, or a captured/live feed) in order and push them through the *same* `EventLoop` instance construction path. Neither driver contains any decision logic — if it did, "backtest vs live parity" would be a claim about two different code paths that happen to agree, not a structural guarantee.

## Design Rationale

**Why a single-threaded event loop rather than a multi-threaded pipeline for the decision path.** Determinism is the entire point of this project, and multi-threaded scheduling is a classical, well-documented source of nondeterminism (lock acquisition order, OS scheduler jitter, data races that only manifest under specific interleavings). A single thread consuming a strictly-ordered stream removes an entire category of bug before it can exist. The credible alternative — a lock-free multi-producer pipeline processed by worker threads pinned per-instrument — is real and is exactly what Phase 4 investigates for the *feed ingestion* side, but the decision core itself stays single-threaded even after Phase 4; only the hand-off queue in front of it is a Phase 4 optimization target.

**Why `IBrokerAdapter` separates market-data and order-placement concerns even for brokers that support both.** The project's safety requirement (paper-only, hard-enforced) and its architectural requirement (broker-agnostic, provably so) point at the same seam. If a single `KiteAdapter` class both streamed data and placed orders, "paper mode" would be a runtime flag deep inside a class that also has full order-placement capability compiled into the same binary — one missed check away from a real order. Splitting the interface so that a market-data-only adapter *cannot* place an order (the method doesn't exist on that type) turns a runtime safety property into a type-system property for every broker except the one adapter (Phase 5's sandbox adapter) that is explicitly built, tested, and constrained to be safe.

**Why `SimulatedBroker` is the default rather than a real sandbox from day one.** None of the requested Indian brokers (Zerodha, Kotak) offer a sandbox; only Upstox and Dhan do. Building the mandatory safety story around "trust broker X's sandbox to never place a real order" would make broker choice a safety decision, which is backwards. `SimulatedBroker` consumes the same real (or replayed) market data a real adapter would, and simulates fills using the exact fill logic the backtester already uses — so the student is exposed to real market data semantics and realistic fill behavior without any code path capable of transmitting an order to an exchange. The Upstox-sandbox adapter in Phase 5 is deliberately optional and additive: it proves the `IBrokerAdapter` abstraction wasn't accidentally shaped around `SimulatedBroker`'s convenience, but it is not where the safety guarantee lives.

**Why the risk gate is an ordered chain with distinct reason codes rather than a single "is this order OK" predicate.** A boolean gate can tell you an order was rejected; it can't tell you *why*, which matters both for debugging (a strategy generating orders that are silently dropped looks identical to a strategy that stopped generating orders) and for the Phase 2 test suite's requirement to assert which specific check fired. Ordering the checks cheapest-and-most-fundamental first (tradability, band, tick size) before expensive or stateful ones (margin lookup, drawdown state) is a minor efficiency choice; the harder-won property is that ordering is fixed and documented, so "which check fires first when two are violated" is a specified behavior, not an accident of iteration order.

## Reference Implementation

Left to the learner: the full domain model (instrument, tick, order, `OrderIntent`), the complete order-status transition table, both strategies' bar-by-bar computation, the risk gate's eight checks, the cost model's exact arithmetic, the WebSocket binary decoder, the backtester/live-harness drivers, the benchmark harness, and the sandbox adapter. The three sketches below establish the shape of the seams described above; none is a complete, compilable implementation, and each elides significant detail (error handling, the full method set, and most of the domain types) that the learner must design.

```cpp
// The seam determinism depends on: no implementation of Clock may read a
// hardware/OS clock on the decision path except LiveClock, and LiveClock
// reads the feed's timestamp, not the local machine's.
class Clock {
public:
    virtual ~Clock() = default;
    virtual Timestamp now() const = 0;
};

class ReplayClock final : public Clock {
public:
    void advance_to(Timestamp t) { current_ = t; }
    Timestamp now() const override { return current_; }
private:
    Timestamp current_{};
};

class LiveClock final : public Clock {
public:
    void on_packet_timestamp(Timestamp exchange_ts) { current_ = exchange_ts; }
    Timestamp now() const override { return current_; }
private:
    Timestamp current_{};
};
```

```cpp
// The broker-agnostic seam. Note placing an order is not on this interface --
// it lives on a separate, narrower interface that only SimulatedBroker and
// the Phase 5 sandbox adapter implement, so a market-data-only adapter is
// structurally incapable of sending an order.
class IMarketDataSource {
public:
    virtual ~IMarketDataSource() = default;
    virtual void subscribe(std::span<const InstrumentId> ids) = 0;
    // Delivers ticks to the event loop via a callback registered at construction.
};

class IOrderExecutor {
public:
    virtual ~IOrderExecutor() = default;
    virtual OrderId submit(const OrderIntent&) = 0;
    virtual void cancel(OrderId) = 0;
    virtual OrderStatus poll(OrderId) const = 0;
};

// SimulatedBroker implements both; a real read-only adapter implements only
// IMarketDataSource. Only the Phase 5 sandbox adapter implements IOrderExecutor
// against a real (sandboxed) broker, gated by the four-layer paper-only checks.
```

```cpp
// Risk gate: ordered checks, each returning a distinct reason code rather
// than a bool, so tests can assert *which* check fired.
enum class RiskReject {
    None, NotTradable, PriceBandViolation, TickSizeViolation,
    RateLimitExceeded, ExposureLimitExceeded, InsufficientMargin,
    DrawdownBreakerTripped, KillSwitchActive,
};

class RiskGate {
public:
    RiskReject evaluate(const OrderIntent& intent, const AccountState& acct) const {
        for (auto& check : checks_) {
            if (auto reject = check(intent, acct); reject != RiskReject::None) {
                return reject;
            }
        }
        return RiskReject::None;
    }
private:
    // Populated in the fixed, documented order from STATEMENT.md; each entry
    // is one check's function object. Left to the learner: the eight checks
    // themselves, and the DrawdownBreaker/KillSwitch's latching state machines.
    std::vector<std::function<RiskReject(const OrderIntent&, const AccountState&)>> checks_;
};
```

## Testing Strategy

Follow TESTS.md's phase grouping. The two tests worth over-investing in relative to their apparent size are the Phase 1 replay-determinism test and the Phase 3 parity test — both are cheap to write and are the tests that actually validate this project's central claim; every other test validates a more conventional property (a computation is correct, a check fires) that a reviewer could otherwise only take on faith. Write the parity test's diagnostic output (first-divergence index and full field dump of both sides) before you need it, not after the first time it fails — you will need it, and building it under the pressure of an actual failing test is worse than building it as part of the test's initial implementation.

## Performance Analysis

Phase 4's exit bar requires a stated budget, a baseline, and per-optimization before/after numbers — the specific numbers are the learner's to establish against their own hardware and synthetic load generator, not prescribed here. The methodological point worth stating: profile before optimizing, on this project specifically, because the two most "obviously slow-looking" candidates (a mutex-guarded queue at the feed hand-off, and per-tick heap allocation of `OrderIntent` objects) are not guaranteed to be the actual bottleneck at realistic Indian cash-equity tick rates (a single, even liquid, NSE equity instrument's tick rate is a small fraction of, say, a US options feed's) — the actual bottleneck at that load could as easily be the strategy's rolling-window recomputation (e.g. an O(n) regression recomputed from scratch every bar instead of updated incrementally). Optimize what the profile says, not what looks slow.

## Failure Modes

- **A staleness or timeout check calls a real wall clock.** Breaks backtest/live parity silently — see HINTS.md Hint 2 and 4. Caught by the CI grep-gate and the negative parity test (TESTS.md, Phase 3).
- **Two same-timestamp ticks processed in different relative order between live and replay paths.** The most likely genuine root cause of a parity-test failure that isn't a clock leak; requires an explicit, documented tie-breaking rule (e.g. arrival/sequence number) rather than relying on timestamp ordering alone.
- **A drawdown circuit breaker implemented as level-triggered instead of latching.** Equity recovering above the trip threshold silently re-enables trading with no operator awareness that a breach occurred — defeats the purpose of a circuit breaker. Caught by the Phase 2 latching-behavior test.
- **MIS square-off time or a price-band percentage hardcoded as a literal.** Silently produces a wrong-but-plausible session classification the next time the real value changes (these have changed multiple times in the recent past) — no crash, no obvious symptom, just quietly wrong risk-gate behavior. Mitigated structurally per Hint 3, not caught by any single test — this is why it's called out as a design constraint, not left to test coverage alone.
- **A real-order-capable adapter's transport constructed before the four paper-only layers are all checked.** Even one layer skipped in a refactor could permit a real order. Caught by the cross-cutting paper-only enforcement test (TESTS.md) that exercises every partial-combination of the four layers.

## Extensions

- A third strategy (e.g. a breakout/momentum rule) implemented against the same `IStrategy` interface with zero changes to the event loop, risk gate, or cost model — the strongest possible demonstration that the interface boundary is real.
- Portfolio-level risk (correlated-exposure limits across instruments, not just per-instrument) added as a ninth risk-gate check.
- A second real-broker adapter (in addition to the Phase 5 sandbox adapter) against a different broker's API, as a second independent proof that the abstraction generalizes rather than having been shaped by the first real adapter.
- A simple terminal or web dashboard rendering the live equity curve, open positions, and risk-gate rejection counts by reason code — genuinely useful for demoing this project, and entirely additive to everything above.
