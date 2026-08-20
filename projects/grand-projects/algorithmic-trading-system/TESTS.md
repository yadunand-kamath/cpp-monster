# GP-1 — Test Plan

Tests are grouped by phase. Within a phase, tests are unordered unless noted. "Exit-bar test" marks the specific test each phase's STATEMENT.md exit bar names — everything else is supporting coverage.

## Phase 1 — Domain Model, Order State Machine, Deterministic Event Loop

- **Exit-bar test — replay determinism:** feed a fixed, hand-authored synthetic tick sequence (≥200 events, at least two events sharing an exchange timestamp) through the event loop in two separate process invocations; assert the two produced `OrderIntent` sequences are byte-identical, including field order and any floating-point-adjacent field (there should be none — this test is partly a check that there are none).
- **Exit-bar test — state transition coverage:** table-driven test asserting every documented status string is reachable and every documented terminal state (`COMPLETE`, `CANCELLED`, `REJECTED`) is absorbing (no further transition possible). Include at least three sequences with a skipped or unexpected intermediate state (e.g. `OPEN PENDING` → `COMPLETE` with no `OPEN` in between) and assert none deadlock, throw, or silently misclassify the terminal outcome.
- Tick-size lookup returns the correct bucket at each boundary price (₹249.99, ₹250.00, ₹999.99, ₹1000.00, etc.) — off-by-boundary is the likely bug here.
- Price-band check correctly classifies a CAS vs non-CAS instrument's session state at 15:14:59, 15:15:00, 15:15:01, 15:29:59, 15:30:00, 15:30:01 IST.
- Event loop rejects (at construction, not silently) an out-of-order event stream — a test asserting this is a hard precondition, not a best-effort tolerance.
- Fuzz/property test: random valid event-stream permutations that preserve per-instrument order still produce the same final positions and P&L as the canonical ordering (cross-instrument order is legitimately unconstrained; same-instrument order is not).

## Phase 2 — Strategy Interface, Risk Gate, Cost Model, Backtester

- Strategy A (dual-MA/ATR): unit test the crossover detection and trailing-stop monotonic-tightening property directly against a hand-constructed price series where the correct entry/exit bars are known by hand calculation.
- Strategy B (mean-reversion z-score): unit test the rolling regression/z-score computation against a hand-computed reference series; unit test the 20-bar hard time-stop fires even when price is still favorably diverging.
- **Exit-bar test — cost model verification:** at least five manually hand-calculated sample trades (mix of CNC and MIS) with STT, NSE transaction charge, SEBI fee, stamp duty, brokerage, and GST computed by hand against the published rates in STATEMENT.md, asserted against the model's output to the paisa.
- Risk gate: one test per check (8 total) asserting that check — and only that check — fires for an input crafted to violate exactly one condition; assert the returned reason code matches, not just that the order was rejected.
- Risk gate ordering: a test crafting an input that violates two checks simultaneously (e.g. price-band AND tick-size) and asserting the earlier check in the documented order is the one reported.
- Drawdown circuit breaker: once tripped, further orders are rejected even after equity recovers above the trip threshold, until an explicit reset call is made (latching behavior, not level-triggered).
- **Exit-bar test — backtest replay stability:** run both reference strategies over the same multi-day historical tape three times; assert byte-identical equity curves and trade ledgers across all three runs.

## Phase 3 — Live Market-Data Adapter, Simulated Fills, Backtest/Live Parity

- `SimulatedBroker` fill logic unit tests: a limit order resting through several ticks that never reach its price is never filled; a market order fills at the next available tick's price, not the order-submission-time price.
- Live adapter reconnect test: inject a simulated disconnect mid-stream and assert the event loop sees no duplicated and no dropped ticks across the gap (requires the adapter to expose a way to verify sequence continuity, e.g. an exchange sequence number or timestamp monotonicity check).
- WebSocket binary decode unit tests for each mode (`ltp`/`quote`/`full`) against a byte-for-byte fixture of a known-good packet.
- **Exit-bar test — backtest/live parity:** capture one real recorded session's raw tick log; run it once through `LiveHarness` (replaying the log through the live-shaped adapter path) and once through the Phase 2 `Backtester`; assert the two produced `OrderIntent` sequences are byte-identical with zero tolerance. On first divergence, the test must report the first differing event's index and both intents' full contents — not just "FAILED."
- Negative parity test: deliberately introduce one nondeterminism source (e.g. a `now()` call on the decision path) and assert the parity test *fails* — a parity test that can't detect the bug it exists for is worse than no test.

## Phase 4 — Hot-Path Latency

- Benchmark harness baseline: measured p50/p99/p999 tick-to-decision latency under a stated synthetic load, captured and stored before any Phase 4 optimization work begins.
- Per-optimization before/after benchmark: each individual change (e.g. queue replacement, allocation removal) has its own isolated before/after measurement, not just a single end-of-phase number.
- **Exit-bar test — latency budget:** measured p99 tick-to-decision latency under the stated load meets the stated budget from STATEMENT.md's Phase 4 section.
- Regression guard: the Phase 1 determinism test and Phase 3 parity test are re-run after Phase 4 changes and still pass — latency work must not have traded away correctness.

## Phase 5 — Hardening, Reconciliation, Kill Switch, Real-Broker-Sandbox Proof

- **Exit-bar test — reconciliation catch:** deliberately desynchronize the internal position ledger from a simulated broker position response and assert the reconciliation check flags the mismatch with the specific instrument and quantity delta, not just a generic alarm.
- **Exit-bar test — kill switch, operator-triggered:** operator command halts new order submission within the stated bound; already-open orders are handled per the documented flatten-or-leave policy.
- **Exit-bar test — kill switch, drawdown-triggered:** an injected equity-curve drawdown crossing the configured threshold trips the kill switch automatically, and a reset requires an explicit logged operator action.
- Rate limiter: a burst of orders exceeding the configured token-bucket rate is throttled, not rejected outright, and the throttled orders are eventually submitted once budget replenishes (unless expired by a TTL/validity rule).
- **Exit-bar test — sandbox adapter round-trip:** a real sandbox order (Upstox sandbox or equivalent) is placed, polled to a terminal status, and (if supported) cancelled, through the unmodified strategy/risk/cost/core pipeline; a diff of files touched for this adapter confirms zero changes outside its own directory.

## Cross-cutting

- **CI grep-gate:** no `double`/`float` token appears in any file on the price/quantity/P&L decision path (a maintained allowlist of exempted files — e.g. benchmark reporting, log formatting — is permitted and must be reviewed, not silently grown).
- **CI grep-gate:** no direct call to `std::chrono::system_clock::now()` or `steady_clock::now()` outside the explicitly allowlisted adapter files.
- **Paper-only enforcement test:** attempt to enable live order placement with each of the four layers (compile flag, runtime guard, config token, abort window) individually satisfied but the others left at default, and assert real-order submission is still blocked in every partial combination — only all four together permit it.
- Build and test suite pass on both MSVC and WSL-GCC/Clang toolchains.
