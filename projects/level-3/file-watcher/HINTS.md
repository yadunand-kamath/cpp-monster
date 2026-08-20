# P-3.2 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch09's paired-platform I/O material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

`inotify` and `ReadDirectoryChangesW` both notify you about filesystem changes without polling, but they hand you the information in very differently shaped packages — one is a stream of small structured records read from a file descriptor, the other is a buffer of variable-length records filled by an asynchronous I/O operation you must explicitly keep re-issuing. Rather than trying to make one platform's API "look like" the other's at the point of use, think about designing one common, small `FileEvent` type and a background mechanism *per platform* whose only job is to translate that platform's native notifications into a stream of these common events — the rest of your code (coalescing, rename reconstruction, the public API) then operates on one shape regardless of platform.

## Hint 2 — Technique

Not busy-polling and supporting a clean, prompt shutdown are really the same underlying design problem: you need to wait on more than one thing at a time — the OS's notification source, and a separate signal that means "stop now." On Linux, register both the `inotify` file descriptor and a second fd you fully control (a pipe you can write a byte to, or an `eventfd`) with `epoll`, and wake on whichever becomes ready first. On Windows, `ReadDirectoryChangesW`'s overlapped mode gives you an event handle that signals on I/O completion; pair it with a second, manually-created event handle for shutdown, and wait on both via `WaitForMultipleObjects`.

## Hint 3 — Implementation

For coalescing, a hash map from path to "last emitted at" timestamp, checked and updated for every raw incoming event before deciding whether to emit anything to the caller, is enough — if a path's last-emitted timestamp is within your coalescing window, swallow the new raw event (or extend the window; document which). For rename reconstruction on Linux: `inotify`'s `IN_MOVED_FROM` and `IN_MOVED_TO` records share a `cookie` value specifically designed for this correlation — hold an unmatched `IN_MOVED_FROM` in a small pending map keyed by cookie, and pair it with the next `IN_MOVED_TO` bearing the same cookie; if none arrives within a short timeout, resolve it as your documented fallback (the file left the watched tree). Windows' `FILE_ACTION_RENAMED_OLD_NAME`/`FILE_ACTION_RENAMED_NEW_NAME` pair works the same way but without an explicit cookie — they arrive as consecutive records in the same buffer read, which you can rely on for correlation instead.

## Hint 4 — Debugging/Design

If your Windows implementation drops events under a high-volume stress test despite passing light manual testing, look at your `ReadDirectoryChangesW` buffer size and, more importantly, how quickly you re-issue the next read after draining a buffer of results — this is a fixed-size buffer the OS fills once per call, and if your processing-then-reissue loop is too slow relative to the write rate, or your buffer is undersized for a realistic burst, the OS will report a buffer-overflow condition (rather than silently accumulating unlimited events) — check whether your stress test ever actually observes and reports this condition; if it silently drops events without that signal firing, you likely never wired up detection for it in the first place, which is precisely the gap the Error Handling requirement is there to catch.
