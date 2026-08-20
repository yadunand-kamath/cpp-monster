# P-3.2 — Solution

## Reference Architecture

A common event type and public class, backed by two independent platform-specific backend implementations selected at compile time.

```cpp
enum class FileEventKind { Created, Modified, Deleted, Renamed, DirCreated, DirDeleted };

struct FileEvent {
    FileEventKind kind;
    std::filesystem::path path;
    std::optional<std::filesystem::path> old_path; // set only for Renamed
};

enum class WatchTermination { DirectoryRemoved, Error };

class FileWatcher {
public:
    explicit FileWatcher(std::filesystem::path dir, bool recursive = false,
                          CoalesceWindow window = CoalesceWindow{50ms});
    void start(std::function<void(const FileEvent&)> on_event,
               std::function<void(WatchTermination)> on_terminate = {});
    void stop(); // must unblock any thread currently waiting on the OS backend
    ~FileWatcher() { stop(); }
private:
    std::unique_ptr<PlatformBackend> backend_; // LinuxInotifyBackend or WindowsRdcBackend
    Coalescer coalescer_;
    std::thread worker_;
};
```

The Linux backend's blocking loop, showing the dual-wait shape that both busy-poll avoidance and clean shutdown depend on:

```cpp
class LinuxInotifyBackend {
public:
    LinuxInotifyBackend(const std::filesystem::path& dir, bool recursive) {
        inotify_fd_ = inotify_init1(IN_NONBLOCK);
        // add_watch(dir) and, if recursive, each subdirectory found via a walk
        int pipefds[2]; pipe(pipefds);
        shutdown_read_ = pipefds[0]; shutdown_write_ = pipefds[1];
        epoll_fd_ = epoll_create1(0);
        epoll_add(epoll_fd_, inotify_fd_);
        epoll_add(epoll_fd_, shutdown_read_);
    }

    void run(RawEventSink& sink) {
        std::array<epoll_event, 2> events;
        while (true) {
            int n = epoll_wait(epoll_fd_, events.data(), events.size(), -1);
            for (int i = 0; i < n; ++i) {
                if (events[i].data.fd == shutdown_read_) return; // woken by stop()
                if (events[i].data.fd == inotify_fd_) drain_inotify_events(sink);
            }
        }
    }

    void request_stop() { char b = 1; write(shutdown_write_, &b, 1); }
private:
    int inotify_fd_, epoll_fd_, shutdown_read_, shutdown_write_;
};
```

`drain_inotify_events` reads raw `inotify_event` records, correlating `IN_MOVED_FROM`/`IN_MOVED_TO` pairs by their shared `cookie` (held in a small pending map with a short timeout), and forwards resolved `FileEvent`s to a shared `Coalescer` before they reach the caller's callback.

## Design Rationale

**Why route the Windows shutdown through a second, manually-signaled event handle rather than just closing the directory handle from another thread?** Closing a handle that has an outstanding overlapped `ReadDirectoryChangesW` operation on it is a documented source of undefined/unreliable behavior across Windows versions. `WaitForMultipleObjects` waiting on both the I/O completion event and an explicit "please stop" event is the supported pattern, and it exactly mirrors the Linux epoll-on-two-fds shape — the two backends end up structurally parallel even though the underlying primitives (fds vs handles, readiness vs completion) are unrelated, which is the point of Ch09's paired-platform framing.

**Why correlate renames via a short-timeout pending map instead of assuming `IN_MOVED_FROM`/`IN_MOVED_TO` always arrive back-to-back?** They usually do, but nothing in the specification guarantees it, especially under concurrent filesystem activity from other processes. A timeout-bounded pending map (resolve to the documented fallback if no match arrives in time) is honest about this uncertainty rather than silently assuming an ordering guarantee that doesn't actually exist.

**Why is `Coalescer` a separate component the raw per-platform events feed into, rather than logic embedded in each backend?** The coalescing policy (time window, per-path last-seen tracking) is identical regardless of which OS produced the raw event — implementing it once, downstream of both backends, avoids duplicating (and potentially inconsistently re-implementing) the same logic twice.

## Reference Implementation

The above covers the dual-wait shutdown shape (the project's trickiest correctness requirement) and the Linux rename-correlation sketch. Remaining substantial work, left to the learner:
1. The Windows backend's full `ReadDirectoryChangesW` overlapped-I/O loop, including detecting and surfacing its buffer-overflow condition (`ERROR_NOTIFY_ENUM_DIR` or a truncated result buffer, depending on API surface used).
2. `Coalescer`'s concrete time-window bookkeeping and its policy decision for the create-then-immediate-delete edge case.
3. Recursive watching — adding subdirectory watches dynamically on Linux as new subdirectories are created (inotify watches are not inherently recursive), versus Windows' native `bWatchSubtree` flag which handles this for you.
4. The "watched directory itself deleted" termination path on both platforms, and wiring `on_terminate`.

## Testing Strategy

Prefer real, running temporary directories with real filesystem operations over mocking the OS notification APIs — the entire point under test is the actual OS behavior (event ordering, buffer/queue overflow conditions, rename-pair delivery), which a mock would have to assume correctly in the first place, defeating the test's purpose.

## Performance Analysis

Both backends are O(1) per notification received — no polling, no scanning. Coalescing adds an O(1) hash-map lookup/update per raw event. The dominant cost under high volume is however many raw OS-level notifications the underlying filesystem activity actually generates, which the tool cannot reduce, only aggregate downstream.

## Failure Modes

- Assuming inotify watches recurse automatically (they don't) and silently missing all events in subdirectories created after the initial watch setup.
- Closing a Windows directory handle with an outstanding overlapped operation still pending, causing undefined behavior instead of a clean shutdown.
- A shutdown mechanism that sets a flag checked only after the next event — passes casual testing (since some activity usually follows shortly), fails the specifically-designed "stop with zero activity" test.
- Silently swallowing the overflow/missed-events condition instead of surfacing it, giving false confidence that no events were missed under load.

## Extensions

- A higher-level "one-shot debounced rebuild" wrapper (common in build-watch tools) built directly on top of the coalescing mechanism.
- Persisting a directory-tree snapshot so a missed-events condition can be recovered from via a targeted re-scan rather than a full one.
