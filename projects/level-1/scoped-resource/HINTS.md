# P-1.2 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread the relevant Ch01–02 Crash Course sections before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

All three of these types are variations on the same underlying pattern: acquire a resource or commit to an action at construction time, and guarantee it happens exactly once by the time the object's lifetime ends — including lifetimes ended early by an exception unwinding through the stack. Think about what language mechanism guarantees a specific function runs when an object's storage duration ends, regardless of *how* control left that scope, and why that mechanism gives you the exception-safety property for free rather than as something you have to code up separately.

## Hint 2 — Technique

For "move-only, correctly disarms the moved-from object": think about what state a moved-from object needs to be left in so that its own destructor becomes a no-op — and where exactly that disarming needs to happen. It's not enough for the *new* object to know it owns the resource; the *old* object must be left in a state where its destructor recognizes it owns nothing. Consider what a natural "empty" or "null" state looks like for each of your three types (a null `FILE*`, a null/empty invocable, etc.), and make sure your move constructor and move assignment operator both explicitly leave the source in that state — this doesn't happen automatically just because you wrote `= default`.

## Hint 3 — Implementation

For move assignment specifically, consider what must happen to the *target's* currently-held resource before you overwrite it with the source's — forgetting this step is one of the most common ways this kind of wrapper leaks a resource silently. Ask yourself: if `b` already owns a live resource and you do `b = std::move(a)`, does anything ever clean up what `b` used to own? For `scope_exit`'s exception-safety requirement, consider whether your destructor's cleanup-invocation logic needs to look any different from a "normal path" cleanup at all — what actually differs about a destructor call during stack unwinding versus during ordinary scope exit, from the object's own point of view? (Hint: less than you might think.) For self-move-assignment, think about what order of operations in your move-assignment operator would break if `this == &other`.

## Hint 4 — Debugging/Design

If your ASan run reports a double-free, check every code path that could leave two live objects believing they both own the same underlying resource simultaneously — move construction/assignment and self-move-assignment are the two most likely culprits; trace through your move-assignment operator line by line for the case where both sides currently hold distinct live resources. If a destructor that could throw is causing a `std::terminate` you didn't expect during your exception-path test, remember that a destructor invoked during stack unwinding from another exception, if it itself throws, terminates the program immediately — your cleanup logic inside the destructor needs a plan for its own failure that does not involve throwing (swallow-and-log, a non-throwing error channel, or a documented "cleanup failures are unobservable by design" trade-off are all legitimate choices; silently letting an exception propagate out of the destructor is not).
