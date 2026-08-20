# P-3.2 — Cross-Platform File Watcher

**Level:** 3 (Realistic utility) · **Category:** Systems · **Requires:** Ch01–09 · **Est. effort:** L (16-24h)

## Objective

Build a directory-watching utility that reports file creation, modification, deletion, and rename events, presenting one unified event interface backed by `inotify` on Linux and `ReadDirectoryChangesW` on Windows, with event coalescing (rapid duplicate events for the same file collapsed) and correct rename tracking (a rename reported as one logical event, not a delete-then-create pair, where the underlying OS API allows this distinction).

## Functional Requirements

1. Watch a directory (optionally recursively) and report, through one common event type, at minimum: file created, file modified, file deleted, file renamed (old name → new name, where derivable), and directory created/deleted (if watching recursively).
2. On Linux, implement this via `inotify` (`inotify_init1`, `inotify_add_watch`, reading structured events from the returned fd).
3. On Windows, implement this via `ReadDirectoryChangesW`, correctly handling its buffer-based, callback-or-overlapped-I/O-driven event delivery model.
4. Coalesce rapid-fire duplicate events for the same file within a short, documented time window (e.g. many editors save a file via multiple underlying filesystem operations that would otherwise appear as several "modified" events for one logical save) — document your coalescing window and policy.
5. Correctly reconstruct rename events as a single logical event where the OS API provides enough information to do so (Linux's `inotify` delivers paired `IN_MOVED_FROM`/`IN_MOVED_TO` events sharing a cookie; Windows' API delivers a similar old-name/new-name pair for renames within a watched directory) — falling back to separate delete+create events, documented as such, when the OS doesn't give you enough to correlate them (e.g. a move across watched-directory boundaries).
6. Provide a clean shutdown mechanism (the watch can be stopped, and any blocking wait for events is interrupted rather than left blocked forever).

## Input

A directory path to watch, an optional recursive flag, and a callback (or event queue) the caller consumes events from.

## Output

A stream of unified `FileEvent` values (kind, path, and for renames, both old and new paths).

## Constraints

- C++20, correct behavior required on both Linux (via `inotify`) and Windows (via `ReadDirectoryChangesW`) behind one shared interface — this is a direct, required application of Ch09's paired-platform systems material, not an optional nice-to-have.
- Must not busy-poll for events — both platform backends must use their respective blocking/asynchronous waiting mechanisms (`inotify`'s readable fd via `select`/`epoll`/blocking `read`; Windows' overlapped I/O or a dedicated thread with `ReadDirectoryChangesW`'s synchronous mode) rather than a `sleep`-and-recheck loop.
- The clean-shutdown mechanism must actually interrupt a currently-blocked wait, not merely set a flag that's checked only after the next event arrives (which could be never, if no further filesystem activity occurs).

## Edge Cases

- A file that is created and then immediately deleted within the coalescing window — decide and document whether this collapses to "nothing happened" (arguably correct, since the net effect is no change) or is still reported as two events (arguably more useful for auditing purposes) — pick one and document why.
- A rename that moves a file *out of* the watched directory entirely (to an unwatched location) — the OS may only give you the "moved from" half of this; document the resulting reported event.
- Watching a directory that is itself deleted while being watched — the watch must terminate cleanly, not crash or hang, when its target directory disappears out from under it.
- Very high event volume (e.g. a build system writing thousands of files in a tight loop) — the watcher must not drop the connection to the OS's event source due to an internal buffer overflowing silently (Windows' `ReadDirectoryChangesW` in particular has a documented buffer-overflow failure mode that must be handled, not ignored).

## Error Handling

- The target directory not existing at watch-start time — a clear, immediate error.
- The underlying OS watch mechanism failing to initialize (resource limits, permissions) — a clear, documented error distinct from "directory doesn't exist."
- An event-buffer-overflow condition (Windows) or an `inotify` queue overflow (`IN_Q_OVERFLOW`, Linux) — both must be detected and surfaced to the caller as a distinct "some events may have been missed, consider a full re-scan" signal, not silently swallowed.

## Acceptance Criteria

- Correctly reports create/modify/delete/rename events for real filesystem operations performed against a temporary watched directory, on both Linux (WSL) and Windows.
- The coalescing behavior is demonstrated with a documented before/after event count for a rapid-fire multi-write scenario (e.g. an editor-style save-via-temp-file-then-rename pattern).
- Rename reconstruction is demonstrated for an in-directory rename (single logical event) and documented for an out-of-directory move (whatever the OS boundary allows).
- Clean shutdown is demonstrated: stopping the watch reliably unblocks a thread currently waiting for events, verified via a test with a timeout that would fail if shutdown hung.
- Builds and runs correctly on both platforms.

## Testing Requirements

- Cross-platform correctness tests for each event kind against a real temporary directory.
- The coalescing-window test with a documented before/after count.
- The rename-reconstruction test (in-directory case) and the documented out-of-directory case.
- A clean-shutdown test with an enforced timeout, run specifically to catch a shutdown implementation that only "usually" works.
- A high-event-volume stress test checking the overflow-detection path is actually reachable and reported, not just theoretically handled.

## Hints

### Hint 1 — Direction
Both `inotify` and `ReadDirectoryChangesW` solve the same underlying problem (the OS notifying you about filesystem changes without you having to poll) but expose that solution through very differently-shaped APIs — one delivers a stream of structured records you read from a file descriptor, the other fills a buffer via an asynchronous I/O operation you have to explicitly re-issue. Think about what single, common shape (a queue of typed events, filled by some background mechanism specific to each platform) both of these very different native mechanisms could be adapted to feed into.

### Hint 2 — Technique
For the "not busy-polling" and "interruptible shutdown" requirements together, consider what it means to wait on *multiple* things at once — the OS's event source, and a separate shutdown signal — rather than waiting on the event source alone and hoping shutdown happens to coincide with an event arriving. On Linux, `epoll` (or even `select`) watching both the `inotify` fd and a self-pipe or `eventfd` you control for shutdown signaling is the standard pattern. On Windows, `ReadDirectoryChangesW`'s overlapped mode combined with `WaitForMultipleObjects` waiting on both the I/O completion and a manually-signaled event handle achieves the same shape.

### Hint 3 — Implementation
For coalescing, think about what data structure lets you efficiently answer "have I seen an event for this path within the last N milliseconds" and update that answer as new events arrive — a hash map from path to last-seen-timestamp, checked and updated on every incoming raw event before deciding whether to emit a coalesced event to your caller, is a reasonable starting point. For rename reconstruction on Linux, `inotify`'s `IN_MOVED_FROM` and `IN_MOVED_TO` events share a `cookie` field specifically so you can correlate them — buffer an unmatched `IN_MOVED_FROM` briefly and pair it with a subsequent `IN_MOVED_TO` carrying the same cookie; if no matching `IN_MOVED_TO` arrives within a short window, the file was likely moved outside the watched tree, and you should emit whatever your documented fallback behavior specifies.

### Hint 4 — Debugging/Design
If your Windows implementation intermittently misses events under high write volume (working fine in light testing, dropping events under a stress test), check your `ReadDirectoryChangesW` buffer size and how promptly you re-issue the read after processing a batch of events — this API delivers events into a fixed-size buffer you provide, and if you're slow to re-issue the next read (or your buffer is too small for the burst rate), the OS can overflow that buffer and signal a documented overflow condition rather than silently queuing unlimited events — if your stress test never observes this overflow signal despite dropping events, you're likely not checking for it at all, which is itself the bug the Error Handling requirement exists to catch.
