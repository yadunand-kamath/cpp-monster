# Chapter 09 — Solutions

## Quick Check Answers

**09-QC1.** Linux inherits fds across `fork`/`exec` by default unless the fd was opened/marked with `O_CLOEXEC` (or `fcntl(fd, F_SETFD, FD_CLOEXEC)`); Windows does **not** inherit `HANDLE`s by default — inheritance requires both `bInheritHandle = TRUE` at the handle's creation and `bInheritHandles = TRUE` at the `CreateProcess` call that spawns the child. To get the *other* behavior: on Linux, explicitly set `O_CLOEXEC`; on Windows, explicitly set both inheritance flags.

**09-QC2.** Linux's default memory accounting overcommits — a `mmap`/`malloc` call only reserves virtual address space and defers actually backing it with RAM/swap until the page is first touched, so the initial call can "succeed" for far more memory than the machine could ever back. Windows's `VirtualAlloc(..., MEM_COMMIT)` is required to actually charge the requested size against the pagefile-backed commit limit *at the moment of the call*, so if that charge would exceed the limit, the call fails immediately rather than deferring the failure to first-touch.

**09-QC3.** `EINTR` signals that a blocking call was interrupted by the delivery of a signal while it was blocked — it does not mean anything is actually wrong with the resource (the fd, socket, etc.) being operated on. The correct response is to retry the call (or use `SA_RESTART` so the kernel retries it transparently), because propagating `EINTR` as a hard failure would incorrectly treat "a signal happened to arrive" as a resource-level error.

**09-QC4.** Structured Exception Handling (SEH), via `__try`/`__except`, plays the most structurally similar role to a POSIX signal handler for a hardware fault. It is not truly analogous because SEH is a synchronous, stack-based unwind-and-dispatch mechanism triggered at the exact faulting instruction, not an asynchronous interruption that can arrive mid-instruction at essentially any point the way a POSIX signal can — there is no Windows equivalent of a signal randomly interrupting an unrelated blocking syscall with `EINTR`.

**09-QC5.** In epoll's readiness model, the caller registers interest and is told when an fd is *ready* to have the actual I/O call issued — the caller still issues `read`/`write` itself, after being notified. In IOCP's completion model, the caller issues the actual I/O call (`ReadFile`/`WriteFile` with an `OVERLAPPED` structure) up front, and is only notified afterward, once the kernel has already completed it. The structural difference is *when*, relative to the notification, the real I/O call happens — before notification (IOCP) versus after notification (epoll).

**09-QC6.** `RTLD_GLOBAL` (Linux) lets a `dlopen`'d library's symbols become available for other, later-loaded libraries to resolve against — a form of global symbol interposition with no direct Windows equivalent, since each DLL has its own private per-module import table. `DllMain` running under the loader lock (Windows) means calling back into the loader (e.g. loading another DLL) or many other operations from inside `DllMain` is unsafe/hazardous — a reentrancy hazard with no Linux equivalent, since `.so` initializers don't run under an analogous global lock.

**09-QC7.** `ptrace`-based tools (gdb, strace) work by one process attaching to and controlling a specific target process — the visibility is inherently scoped to whatever process (or processes) you explicitly attach to. ETW is a systemwide, always-available event-tracing infrastructure that any process can emit structured events into and any consumer can subscribe to without needing to attach to any specific process first — the visibility is systemwide and provider-based rather than attach-based.

**09-QC8.** A missing/renamed shared-library symbol: on Linux, `dlopen` failure gives a human-readable diagnostic string via `dlerror()`; on Windows, `LoadLibrary` returning `NULL` requires a separate call to `GetLastError()` to retrieve a numeric Win32 error code, which carries fundamentally different information (a code to look up) rather than a ready diagnostic string — this is a genuine difference in information content, not merely a different spelling of the same fact.

## Problem Solutions

### Level 1 — Recognition

**09-P01.** No — POSIX only guarantees that `open()` (and similar calls) returns the *lowest currently unused* fd number in the process's fd table; it does not guarantee any fd number is stable or meaningful across the program's lifetime, since any earlier `close()` frees up a lower number for reuse by a later, unrelated `open()`. Relying on a specific value (e.g. "fd 3 is always my log file") is fragile because any intervening close/open anywhere in the program (including in a library the program didn't write) can change which fd gets reused for what.

---

**09-P02.** No, it is not safe. A double-close is undefined behavior in general (it may fail harmlessly with an error, or — much more dangerously — the fd/handle value may already have been reused by an unrelated resource opened in between the two `CloseHandle`/`close` calls, in which case the second close silently closes someone else's live resource). This is a "use/release after free"-shaped bug, independent of platform: closing a resource handle twice is the handle-based analogue of a double-`free`.

---

**09-P03.** Not necessarily a bug — this is expected Linux overcommit behavior: `mmap` only reserves address space, and the kernel doesn't require the whole requested size to be actually backable at the time of the call. If the program actually touched (wrote to) all 100 GB, the kernel would be unable to back most of those pages (16 GB RAM, no swap), and the out-of-memory (OOM) killer would very likely be invoked to kill this process (or some other process on the system) once physical memory is exhausted.

---

**09-P04.** `read()` can legitimately return successfully having transferred fewer bytes than requested (a "short read") — it is not guaranteed to fully satisfy the requested length in one call, nor does it block until exactly that much is available. The general term for this is a **partial** read/write (or, generally, that the syscall's return value must always be checked and the operation potentially retried/continued for the remaining amount) — correct code loops on `read`/`write` until the full amount is transferred or an actual error/EOF occurs.

---

**09-P05.** No — readiness only guarantees that *some* data is available to be read without blocking at the moment `epoll_wait` reported it; a subsequent `read()` could return fewer bytes than requested (a short read, per 09-P04), and under specific races (e.g., another thread draining the same fd first, or edge-triggered semantics where the notification refers to a transition rather than a persisting condition) it could even return `EWOULDBLOCK`/`EAGAIN` if nothing is left by the time the read is actually issued.

---

**09-P06.** Yes — the specific failure reason is available via `GetLastError()`, called immediately after `LoadLibrary` returns `NULL`, which returns a numeric Win32 error code describing why the load failed (e.g. module not found, a dependency failed to load, an architecture mismatch).

### Level 2 — Prediction

**09-P07.** The child inherits fd 4 in a usable state — Linux's default is that file descriptors are inherited across `fork`/`exec` unless explicitly marked otherwise. The one flag that would have prevented this, if set at `open()` time, is `O_CLOEXEC`.

---

**09-P08.** No — the child process cannot use the parent's handle value directly, because the handle's own creation-time flag (`bInheritHandle` in its `SECURITY_ATTRIBUTES`) is what actually gates whether *that specific handle* is eligible to be inherited at all; `CreateProcess`'s `bInheritHandles = TRUE` only controls whether the child process, as a whole, inherits *any* inheritable handles the parent holds — it cannot retroactively make a non-inheritable handle inheritable. Both flags must be set for a given handle to actually be inherited; here only the second was.

---

**09-P09.** The kernel does not initially reject any of the 40 `mmap` calls — under Linux's default overcommit policy, 40 × 500 MB = 20 GB of *reserved* virtual address space succeeds trivially even though only 8 GB of RAM exists, since reservation doesn't require backing. The practical risk is that if several instances simultaneously start touching significantly more than their initial 50 MB (approaching their full 500 MB reservations), the kernel may be unable to back all the newly-touched pages with physical memory/swap, triggering the OOM killer to kill one or more processes — possibly including processes that behaved conservatively, not necessarily the one that touched the most.

---

**09-P10.** On Windows, because `MEM_COMMIT` charges against the pagefile-backed commit limit immediately at commit time (not deferred to first touch), if all 40 instances simultaneously attempt to commit their full 500 MB (40 × 500 MB = 20 GB) against a pagefile/RAM combination sized for something closer to 8 GB, some of those later `VirtualAlloc(..., MEM_COMMIT)` calls will fail immediately and deterministically, returning `NULL`/failure at the moment of the call — rather than Linux's behavior of accepting the reservations and only failing (via the OOM killer) later, unpredictably, once pages are actually touched under memory pressure.

---

**09-P11.** `read()` returns `-1`, and `errno` is set to `EINTR` — because `SA_RESTART` was not specified, the interrupted blocking syscall returns early to let the signal handler run and then reports the interruption to the caller rather than transparently resuming.

---

**09-P12.** It runs differently — the `SetConsoleCtrlHandler` callback executes on a separate thread created by the system specifically to invoke it, not on the same thread that was doing the long computation, so it does not interrupt that thread's current instruction the way a POSIX signal handler delivered to a thread would. The practical implication: the long computation loop cannot simply "be interrupted" the way POSIX code might structure things — it must itself cooperatively poll a flag the callback sets (e.g. an atomic bool) to notice the request and stop at a convenient point.

---

**09-P13.** Not reliable — `malloc` and iostream/`FILE*` operations are not on the small, standardized list of functions guaranteed to be **async-signal-safe**, so calling them from a `SIGSEGV` handler is undefined behavior (e.g. if the fault happened while the process's heap allocator was itself mid-operation and holding an internal lock, calling `malloc` again from the handler can deadlock or corrupt heap state). The property violated is async-signal-safety.

---

**09-P14.** Yes, `epoll_wait` will report the same fd ready again on the next call, because level-triggered mode reports readiness as long as the *condition* (unread data still present) persists — it doesn't matter that the previous notification was already "used" for a partial read; there's still data left, so the fd is still, level-wise, in the "ready" state. Edge-triggered mode, by contrast, only notifies on a *transition* to ready and would require draining the fd fully (reading until `EAGAIN`) on that one notification, since no further notification would arrive just because unread data remains.

---

**09-P15.** The completion notification reports success immediately with fewer bytes transferred (1 KB, not the requested 4 KB) — IOCP-style overlapped reads on a stream socket complete as soon as *some* data satisfies the read (subject to the specific I/O type's semantics), they do not block waiting to accumulate the full requested buffer size before signaling completion.

---

**09-P16.** Yes, the dependency resolves successfully — `RTLD_GLOBAL` makes the earlier library's symbols available for other, subsequently-loaded libraries to resolve against, which is exactly what `libplugin.so`'s unresolved dependency needs, regardless of what mode `libplugin.so` itself was loaded with. If the earlier library had instead been loaded with `RTLD_LOCAL`, its symbols would not be exposed for other libraries to resolve against, and `libplugin.so`'s dependency on that symbol would fail to resolve (a load-time error).

### Level 3 — Implementation

**09-P17.**
```c
// helper_list_fds — trivial program the child execs, e.g.:
// #include <dirent.h>
// int main(){ system("ls -la /proc/self/fd"); }

// parent without O_CLOEXEC
int fd = open("test.txt", O_RDWR | O_CREAT, 0644);
if (fork() == 0) {
    execl("./helper_list_fds", "helper_list_fds", nullptr);
}
```
Building/running this shows the child's `/proc/self/fd` listing includes an entry for the inherited fd (its number matching what the parent had, pointing at `test.txt`). Repeating with `open("test.txt", O_RDWR | O_CREAT | O_CLOEXEC, 0644)` and rebuilding/rerunning shows the child's `/proc/self/fd` listing no longer contains that fd — `execl` closed it automatically as part of the exec transition because of the `O_CLOEXEC` flag. Build: `g++ -std=c++20 parent.cpp -o parent && g++ -std=c++20 helper.cpp -o helper_list_fds && ./parent`.

---

**09-P18.**
```cpp
SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, /*bInheritHandle=*/FALSE };
HANDLE h = CreateFileA("test.txt", GENERIC_READ | GENERIC_WRITE,
                       0, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
// pass (HANDLE)(intptr_t)h as a command-line argument to the child
STARTUPINFOA si{ sizeof(si) };
PROCESS_INFORMATION pi;
CreateProcessA(nullptr, cmdline, nullptr, nullptr, /*bInheritHandles=*/TRUE,
               0, nullptr, nullptr, &si, &pi);
```
With `bInheritHandle = FALSE` at creation, the child, given the same numeric handle value on its command line, fails to use it (`GetLastError()` reports an invalid-handle error, e.g. `ERROR_INVALID_HANDLE`) — the handle value is meaningless in the child's own handle table, since it was never duplicated into it. Rebuilding with `sa.bInheritHandle = TRUE` and rerunning with the same `CreateProcess(..., TRUE, ...)` call shows the child successfully using the same numeric handle value, since it now genuinely exists (duplicated) in the child's handle table at process-creation time.

---

**09-P19.** After writing a pattern to the first page and calling `madvise(addr, page_size, MADV_DONTNEED)`, reading the page back typically shows it has been reset to zero (or, for a file-backed mapping, reset to reflect the underlying file rather than the in-memory modification) — because `MADV_DONTNEED` tells the kernel it may discard the private, modified pages in that range and the mapping's contents are undefined (or, for anonymous memory, effectively zero-filled again on next access) after this call. This is consistent with the `madvise` man page's description: it is a hint that changes what the kernel may do with the pages, and the *contents* afterward are explicitly not something the caller may continue to rely on for anonymous mappings.

---

**09-P20.**
```cpp
void* region = VirtualAlloc(nullptr, 64 * 1024 * 1024, MEM_RESERVE, PAGE_READWRITE);
// writing to `region` here triggers an access violation — not yet committed
void* committed = VirtualAlloc(region, 4096, MEM_COMMIT, PAGE_READWRITE);
*reinterpret_cast<char*>(committed) = 'x';  // succeeds — within committed subrange
*reinterpret_cast<char*>(static_cast<char*>(region) + 8192) = 'y';  // still access violation
```
Attempting to write to `region` immediately after the `MEM_RESERVE`-only call produces an access violation (caught, if desired, via SEH `__try`/`__except`), since reserved-but-uncommitted address space has no actual physical backing and is not writable. After committing only the first 4 KB page via a second `VirtualAlloc(..., MEM_COMMIT, ...)` call on that subrange, writes succeed within that committed page but still fault outside it, demonstrating the reserve/commit split is enforced page-by-page.

---

**09-P21.**
```c
struct sigaction sa{};
sa.sa_handler = [](int){};
sigaction(SIGALRM, &sa, nullptr);   // no SA_RESTART
alarm(1);
char buf[16];
ssize_t n = read(pipe_fd, buf, sizeof(buf));  // returns -1, errno == EINTR after ~1s
```
Without `SA_RESTART`, the blocking `read()` on the empty pipe returns `-1` with `errno == EINTR` once the alarm fires and the handler returns, demonstrably around the 1-second mark rather than continuing to block. Rebuilding with `sa.sa_flags = SA_RESTART` and rerunning shows `read()` instead transparently continuing to block past the alarm (the kernel automatically re-issues it after the handler returns) — if data is supplied to the pipe before it would otherwise complete, the call succeeds normally with no `EINTR` ever visible to the caller.

---

**09-P22.**
```cpp
std::atomic<bool> stop{false};
BOOL WINAPI handler(DWORD type) {
    if (type == CTRL_C_EVENT) { stop = true; return TRUE; }
    return FALSE;
}
SetConsoleCtrlHandler(handler, TRUE);
while (!stop) { /* long computation, checked periodically */ }
```
Sending `CTRL_C_EVENT` (e.g. via `GenerateConsoleCtrlEvent` from another process, or an actual Ctrl+C) shows the handler running promptly (observed near-immediately in logged timestamps) on its own system-created thread while the main loop's computation continues uninterrupted on its own thread until the loop itself checks `stop` and exits — demonstrating the callback does not halt the main thread's current instruction. This contrasts with a POSIX signal delivered to a thread blocked in a syscall (as in 09-P21), which does interrupt that specific thread's blocking call directly (`EINTR`), rather than requiring a separate cooperative poll.

---

**09-P23.**
```cpp
int epfd = epoll_create1(0);
epoll_event ev{ .events = EPOLLIN, .data = { .fd = listen_fd } };
epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
while (true) {
    epoll_event events[64];
    int n = epoll_wait(epfd, events, 64, -1);
    for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;
        if (fd == listen_fd) {
            int client = accept(listen_fd, nullptr, nullptr);
            epoll_event cev{ .events = EPOLLIN, .data = { .fd = client } };
            epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev);
        } else {
            char buf[4096];
            ssize_t r = read(fd, buf, sizeof(buf));
            if (r > 0) write(fd, buf, r);
            else { epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr); close(fd); }
        }
    }
}
```
Connecting with two simultaneous clients (e.g. two `nc localhost <port>` sessions) and sending data from each independently demonstrates both connections' echoes are serviced without one blocking behind the other — each `epoll_wait` iteration only issues `read`/`write` on fds actually reported ready, so a slow/idle client never stalls the loop's servicing of the other's ready fd.

---

**09-P24.** Restructuring for IOCP: an overlapped `ReadFile` is issued for each client socket immediately upon accept (not upon a later "ready" notification), and `GetQueuedCompletionStatus` blocks until the OS reports one of those reads has actually finished, at which point the received data is echoed via an overlapped `WriteFile`, and a new overlapped `ReadFile` is reissued for that same client. The structural difference from 09-P23: in the epoll version, `read`/`write` are called *after* being told a fd is ready; in the IOCP version, `ReadFile`/`WriteFile` are called *before* any notification, and `GetQueuedCompletionStatus` only reports that the already-issued operation is done — the actual I/O call has moved from "after readiness" to "before completion" in the control flow.

---

**09-P25.**
```c
void* handle = dlopen("./libplugin.so", RTLD_NOW | RTLD_LOCAL);
auto fn = (int(*)())dlsym(handle, "plugin_call_count");
fn(); fn();               // e.g. increments and returns a static counter: 1, 2
dlclose(handle);
handle = dlopen("./libplugin.so", RTLD_NOW | RTLD_LOCAL);
fn = (int(*)())dlsym(handle, "plugin_call_count");
fn();                      // observed: often resets to 1, but not universally guaranteed
```
On most common Linux toolchains, reloading after `dlclose` (when the library's reference count actually drops to zero and it is truly unmapped) does reinitialize static/global state — the observed counter restarts. This is explained by what `dlclose` documents: it decrements a reference count and, only once it reaches zero, may unmap the library and re-run its destructors/finalizers on next unload, and a subsequent `dlopen` re-runs initializers — but `dlclose` does *not* guarantee unloading happens synchronously or at all in every implementation (a library with outstanding references, or specific `RTLD` flags, may not actually be unmapped), so this "fresh load" behavior is implementation-dependent and should not be relied upon as a portable guarantee.

---

**09-P26.**
```cpp
HMODULE h = LoadLibraryA("plugin.dll");
auto fn = (int(*)())GetProcAddress(h, "plugin_call_count");
fn(); fn();
FreeLibrary(h);
h = LoadLibraryA("plugin.dll");
fn = (int(*)())GetProcAddress(h, "plugin_call_count");
fn();   // observed: also often resets, once the DLL's reference count reaches zero
```
The Windows result is broadly analogous: once `FreeLibrary`'s internal reference count reaches zero and the DLL is actually unloaded, its static/global state is torn down (`DllMain(DLL_PROCESS_DETACH)` runs) and a fresh `LoadLibrary` reinitializes it (`DllMain(DLL_PROCESS_ATTACH)` runs again) — so the two platforms broadly agree in this specific case, with the same caveat as 09-P25: neither platform's unload call is guaranteed to actually unload synchronously in every scenario (other holders of a reference, static analysis of dependent modules, etc. can keep it mapped), so "clean reinitialization on reload" is an observed, not contractually guaranteed, behavior on both platforms.

---

**09-P27.** Running `strace -c ./filecopy src.bin dst.bin` (with a stated file size, e.g. 1 MB, and buffer size, e.g. 64 KB) reports counts including `open` (2 — one for src, one for dst), `read` and `write` (each approximately `ceil(file_size / buffer_size)` = `ceil(1048576 / 65536)` = 16 times, plus one final `read` returning 0 for EOF), and `close` (2). The `read`/`write` call count tracks file size divided by buffer size (rounded up) because each loop iteration transfers at most one buffer's worth, so a file that isn't an exact multiple of the buffer size still needs one extra, partial final transfer.

---

**09-P28.** Running the Windows-ported file-copy program under Process Monitor (filtered to the process) captures the analogous `CreateFile`, `ReadFile`, `WriteFile`, and `CloseHandle` operations with sizes and offsets. One piece of information Process Monitor's capture surfaces that `strace`'s default single-process trace does not: a systemwide view — Process Monitor can simultaneously show *other*, unrelated processes' filesystem operations touching the same file or directory (e.g., an antivirus scanner also opening the destination file mid-copy), which plain `strace -p <pid>` (scoped to one target pid) does not surface unless separately run against every other process of interest.

---

**09-P29.** [DEBUG] The `open()` call has no `O_CLOEXEC` flag, and Linux inherits file descriptors across `fork`/`exec` by default — so when the child calls `execl` to run `untrusted_plugin`, fd 3 (`secret.log`) remains open and usable in the new process image, even though `untrusted_plugin` was never told about it or given it deliberately; the untrusted binary can simply guess/enumerate its open fds (e.g. via `/proc/self/fd`) and use fd 3 directly. The minimal fix: add `O_CLOEXEC` to the `open()` flags (`open("secret.log", O_RDWR | O_CREAT | O_CLOEXEC, 0600)`), which causes the fd to be automatically closed at `execl` time.

---

**09-P30.** [DEBUG] `bInheritHandle = TRUE` at a handle's creation only marks that handle as *eligible* to be inherited by a child process — it says nothing about *which* child. Any subsequent `CreateProcess` call made by the service with `bInheritHandles = TRUE` inherits *every* inheritable handle the parent currently holds, indiscriminately, because Windows's basic inheritance mechanism operates at the level of "does the child inherit inheritable handles at all," not "does the child inherit this specific handle." The correct, scoping mechanism is to use `STARTUPINFOEX` with a `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` attribute specifying exactly which handles should be inherited by that particular `CreateProcess` call, combined with passing `EXTENDED_STARTUPINFO_PRESENT` — this restricts inheritance to precisely the enumerated handles for that one child, regardless of how many other inheritable handles the parent holds.

### Level 4 — Debugging

**09-P31.** [DEBUG] The most likely mechanism is Linux's memory overcommit: the generous `mmap` reservations succeed without error regardless of how much physical RAM/swap actually exists, and the failure is deferred until the pages are actually touched — under heavier concurrent load, more of those reserved pages get touched across all the service's allocations simultaneously, exceeding what the kernel can actually back, at which point the kernel invokes the **OOM killer** to reclaim memory by killing a process (potentially, but not necessarily, the one responsible) — consistent with "no error ever returned from any call" since overcommit defers the failure past every one of those calls entirely.

---

**09-P32.** [DEBUG] The failure mode moved from deferred/unpredictable to immediate/deterministic because Windows's `MEM_COMMIT` requires the requested size to be charged against the pagefile-backed commit limit *at the time of the `VirtualAlloc` call itself* — unlike Linux's deferred-to-first-touch overcommit model, Windows checks and enforces the commit charge up front, so a modestly sized pagefile that can't cover the generous "for headroom" sizes causes immediate allocation failure. The one-line fix: call `VirtualAlloc` with only `MEM_RESERVE` for the generous size, and issue a separate, later `VirtualAlloc(..., MEM_COMMIT)` only for the subranges actually used — restoring the original "reserve generously, commit only what's used" design intent instead of committing everything up front.

---

**09-P33.** [DEBUG] POSIX signals of the same number are not queued/stacked — if several children happen to exit in a very short window, the kernel may only deliver one `SIGCHLD` to represent multiple pending events, or deliver it only once even though multiple children became reapable, so a handler that calls `wait()` exactly once per invocation can reap only one of several exited children, leaving the others unreaped (zombies) until some *later* `SIGCHLD` delivery (if any) triggers another single `wait()` call. The fix is to call `waitpid(-1, &status, WNOHANG)` in a loop inside the handler until it returns 0 (no more reapable children) or -1, so every child that has exited by the time the handler runs gets reaped in that one invocation, regardless of how many `SIGCHLD` deliveries were actually coalesced.

---

**09-P34.** [DEBUG] This is fundamentally broken, not merely inconvenient, because `errno` values (Linux/POSIX) and Win32 error codes (from `GetLastError()`) are two entirely separate, unrelated numbering schemes defined by different specifications with no guaranteed correspondence between a given number's meaning on one side versus the other — e.g., the numeric value that means `ENOENT` on Linux may correspond to a completely unrelated (or nonexistent) meaning as a raw Win32 error code, and vice versa. Forwarding the raw number across the boundary and having the other side reinterpret it under its own scheme produces silently wrong error semantics, not just an inconvenient translation gap. The correct approach: translate each platform's native error at the boundary into a small, deliberately platform-neutral error enum/category (e.g. `file_not_found`, `access_denied`, `already_exists`) that both sides agree on by contract, never passing the raw platform-specific numeric value through as if it were self-describing.

---

**09-P35.** [DEBUG] Edge-triggered mode only notifies on a *transition* to the ready state (e.g., "data just arrived on this previously-empty fd"), not on the persisting *condition* of "there is still unread data," the way level-triggered mode does — so if a single readiness notification is not fully drained (read until `EAGAIN`/`EWOULDBLOCK`), the remaining unread data does not generate a new edge (no new transition occurred; the fd was already "readable" and stays that way without producing another edge), and no further notification for that fd will arrive until *more* new data arrives to produce another transition. Combined with the socket still being blocking-mode (per the problem statement), the server's read call on that fd either blocks indefinitely waiting for more data (stalling the whole single-threaded loop) or, if issued only opportunistically, simply never gets called again for the remaining already-buffered data — appearing to hang on that connection specifically.

---

**09-P36.** [DEBUG] An IOCP operation's `OVERLAPPED` structure and buffer must remain valid and untouched by any other operation for the operation's *entire* lifetime — from the moment it's posted until its completion has actually been dequeued via `GetQueuedCompletionStatus` and fully processed, because the kernel (and any pending internal I/O manager state) may still be referencing that memory during that window, even after the handler *believes* it has already run to completion in some code path. Reusing the same `OVERLAPPED`/buffer for a new operation before that retirement is complete allows the new operation's writes and the old operation's still-in-flight kernel-side bookkeeping to alias the same memory concurrently, producing corrupted data or crashes under load (a race, so it's intermittent). The fix is a pool of distinct per-request "extended OVERLAPPED" contexts (each embedding its own `OVERLAPPED` plus its own buffer and any per-request state), so a new operation always uses a freshly-available context rather than one whose predecessor might not yet be fully retired.

---

**09-P37.** [DEBUG] `RTLD_GLOBAL` makes a loaded plugin's externally-linked, non-static symbols available for *subsequently loaded* libraries/plugins to resolve their own (unrelated) references against — if a later plugin has an internal helper function with the same name and external linkage, and the dynamic linker's symbol resolution order causes it to bind against the earlier plugin's same-named global symbol instead of (or interposing on) its own intended one, calls silently redirect to the wrong implementation, with no crash because both symbols are valid, callable functions — just the wrong one. The fix: load plugins with `RTLD_LOCAL` (so each plugin's symbols are not exposed for other plugins to accidentally bind against) and additionally give every genuinely-internal helper function `static` (internal) linkage or place it in an anonymous namespace/unique namespace, so it never becomes an externally-visible symbol capable of colliding with another plugin's same-named function in the first place.

---

**09-P38.** [DEBUG] `DllMain` runs while the process-wide loader lock is held, and creating a new thread from inside it is a documented hazard because the new thread, upon starting, may itself trigger code (CRT initialization, or calling into other DLL entry points) that needs to acquire the same loader lock — or the main thread may be waiting on that new thread to reach some synchronization point while the new thread is blocked waiting for the loader lock the main thread already holds — producing a deadlock that depends on timing/load order and so manifests only intermittently. The general guidance: defer any non-trivial initialization (spawning threads, loading other DLLs, calling most Win32 APIs) out of `DllMain` entirely, moving it to a lazily-invoked, explicit initialization function that the DLL's own API calls on first use, well after `DllMain` has returned and the loader lock has been released.

### Level 5 — Integration

**09-P39.** A `class FileWatcher` interface exposing `enum class FileEvent { Created, Modified, Deleted }` and a callback-registration API (`watch(path, callback)`) is implemented on Linux via `inotify_init1`/`inotify_add_watch`, translating `IN_CREATE`→`Created`, `IN_MODIFY`/`IN_CLOSE_WRITE`→`Modified`, `IN_DELETE`→`Deleted`; on Windows via `ReadDirectoryChangesW`, translating `FILE_ACTION_ADDED`→`Created`, `FILE_ACTION_MODIFIED`→`Modified`, `FILE_ACTION_REMOVED`→`Deleted`. The deliberate normalization decision concerns renames: `inotify` reports a rename as a paired `IN_MOVED_FROM`/`IN_MOVED_TO` (with a cookie linking them), while `ReadDirectoryChangesW` reports `FILE_ACTION_RENAMED_OLD_NAME`/`FILE_ACTION_RENAMED_NEW_NAME` — the implementation normalizes both into a `Deleted` event for the old name followed by a `Created` event for the new name, discarding the "this was actually one rename" relationship in favor of a smaller, uniform three-event vocabulary (a deliberate simplification documented as a known limitation rather than silently pretending the richer rename information was preserved).

---

**09-P40.** A GoogleTest suite creates a file (expects `Created`), appends data to it (expects `Modified`), and deletes it (expects `Deleted`) in a temp directory, run unmodified against both the `inotify`- and `ReadDirectoryChangesW`-backed builds. Both platforms report the expected three-event sequence for this basic case. A residual, documented difference: `inotify` on Linux commonly reports a distinct `Modified` event for *each* underlying write syscall (or coalesces bursts differently depending on kernel buffering), while `ReadDirectoryChangesW` on Windows can coalesce several rapid writes into a single `Modified` notification depending on OS-level batching — so a test asserting an *exact* modification-event count (rather than "at least one") is documented as platform-dependent and intentionally relaxed (e.g., asserting "≥1 Modified event occurred" rather than an exact count) rather than forced to agree exactly, since forcing exact agreement here would require suppressing real, platform-native batching behavior rather than genuinely unifying it.

---

**09-P41.**
```cpp
#ifdef _WIN32
using native_handle = HANDLE;
constexpr native_handle invalid_handle = INVALID_HANDLE_VALUE;
inline void close_native(native_handle h) { CloseHandle(h); }
#else
using native_handle = int;
constexpr native_handle invalid_handle = -1;
inline void close_native(native_handle h) { close(h); }
#endif

class UniqueHandle {
public:
    explicit UniqueHandle(native_handle h = invalid_handle) : h_(h) {}
    UniqueHandle(UniqueHandle&& o) noexcept : h_(o.h_) { o.h_ = invalid_handle; }
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) { reset(); h_ = o.h_; o.h_ = invalid_handle; }
        return *this;
    }
    UniqueHandle(const UniqueHandle&) = delete;
    ~UniqueHandle() { reset(); }
    void reset() { if (h_ != invalid_handle) { close_native(h_); h_ = invalid_handle; } }
    native_handle get() const { return h_; }
    explicit operator bool() const { return h_ != invalid_handle; }
private:
    native_handle h_;
};
```
This hides both platforms' differing "invalid" sentinel (`-1` for Linux's `int` fd vs. `INVALID_HANDLE_VALUE`, a distinct pointer-sized value, for Windows's `HANDLE`) behind one uniform `explicit operator bool()`/`invalid_handle` constant per platform build, so client code never needs to know or compare against either platform's raw sentinel. Wrapping a real opened file on each platform and letting the `UniqueHandle` go out of scope, then checking `/proc/self/fd` (Linux) or Process Explorer's open-handles view (Windows) for that process, confirms the underlying fd/handle no longer appears in the list — demonstrating the destructor actually released it.

---

**09-P42.** An `EventLoop` interface exposes `register_read(native_handle, callback)` and `run()`. On the epoll backend, `register_read` calls `epoll_ctl(EPOLL_CTL_ADD, ...)` and stores the callback in a map keyed by fd; `run()`'s internal loop calls `epoll_wait`, then for each ready fd, *itself* calls `read()` and passes the result to the stored callback — the actual `read()` call lives inside the abstraction's `run()` loop, issued only after epoll's readiness notification. On the IOCP backend, `register_read` immediately posts an overlapped `ReadFile` (storing the callback in a per-operation context associated with the `OVERLAPPED`); `run()`'s internal loop calls `GetQueuedCompletionStatus`, and upon a completion, invokes the stored callback with the already-completed read's result, then reissues a new overlapped `ReadFile` for that handle to keep servicing it — here the actual `ReadFile` call happens *before* notification, inside `register_read` (and again inside the completion handler for the next read), not inside `run()`'s dispatch step. The single interface hides this by making `register_read` itself platform-dependent in *when* it issues the real I/O call, while presenting an identical call-site contract (register once, get a callback invoked with data) to the echo server built on top.

---

**09-P43.** A `Supervisor` class exposes `start(command)`, monitoring the child and restarting it with exponential backoff on unexpected exit, and `stop()` for graceful shutdown. On Linux: `fork`/`execvp` launches the child, a `SIGCHLD` handler (or a dedicated reaper thread calling blocking `waitpid`) detects exit, and `stop()` sends `SIGTERM`, waits up to a timeout via repeated non-blocking `waitpid(pid, &status, WNOHANG)` polling (or a `sigtimedwait`-based wait), and sends `SIGKILL` if the timeout elapses without the child exiting. On Windows: `CreateProcess` launches the child, a thread calls `WaitForSingleObject` on the child's process handle to detect exit, and `stop()` — since a console-less child process has no direct signal-delivery equivalent — instead calls `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, ...)` for processes sharing a console (or, if the child cooperates via a named pipe/event object convention the supervisor itself defines, signals that instead) to request graceful shutdown, waits up to the same timeout via `WaitForSingleObject` with a timeout argument, and calls `TerminateProcess` if it elapses. The documented mechanism difference: Linux's graceful-then-forced sequence uses two well-defined, universally-supported signal numbers (`SIGTERM`/`SIGKILL`) that any child can choose to handle or ignore; Windows has no single universal "please shut down gracefully" signal for an arbitrary child process, so the supervisor's "graceful" step is necessarily either console-event-based (works only for console-attached children) or dependent on an application-specific cooperative shutdown convention it defines itself — `TerminateProcess` is the one universally available "forced" step, analogous to `SIGKILL`.

---

**09-P44.** *Kernel handles pairing:* a service that opens a sensitive log file without `O_CLOEXEC` and later `exec`s a third-party plugin binary silently leaks read/write access to that log file into the plugin's process on Linux (per 09-P29) — this specific incident cannot occur at all on Windows, because Windows's default is non-inheritance; the exact same code pattern ported "as-is" (assuming an analogous handle-creation call without explicit inheritance flags) would simply not leak the handle on Windows, not "leak it differently." *Memory mapping pairing:* a service that reserves generous `mmap` regions "for headroom" and only fails, unpredictably, via the Linux OOM killer once concurrent load pushes real memory pressure (per 09-P31) — the equivalent Windows service, using the equivalent generous `VirtualAlloc(..., MEM_COMMIT)` call, would instead fail immediately and deterministically at startup/allocation time (per 09-P32) whenever the pagefile-backed commit limit is insufficient; the specific *production* incident of "runs fine for weeks, then gets killed unpredictably under a load spike with zero error returned anywhere" is a Linux-overcommit-specific failure mode that Windows's immediate-commit-check model structurally cannot reproduce in the same shape — Windows would instead have caught the same undersized capacity as a loud, immediate deployment failure, not a delayed, silent production incident.

### Level 6 — Production

**09-P45.** Concrete process: (1) mandate the `UniqueHandle`-style RAII wrapper from 09-P41 for every kernel handle/fd in the codebase, so the inheritance-flag decision is made once, explicitly, at the wrapper's construction site rather than scattered across ad hoc `CreateFile`/`open` calls; (2) add a code-review checklist item requiring every new handle/fd-creation call site to state explicitly (in code, not just intent) whether it should be inheritable, rather than relying on either platform's silent default; (3) add an automated CI test that spawns a child process from the service on both platforms and asserts the exact set of handles/fds the child actually receives matches the intended set (catching both an accidental leak and an accidental non-inheritance regression). Why Linux's default masked this specific bug: Linux's "inherit unless told not to" default means any handle-creation call that never considered the question at all still "worked" during Linux-only testing (nothing needed to be added for it to inherit), so the code path shipped and passed Linux CI/testing looking correct; the identical code path, ported to Windows's "don't inherit unless told to" default, would have needed an explicit opt-in the developer never wrote, and would have visibly failed the very first time the child process tried to use the handle — Windows's stricter default would have surfaced the missing-inheritance-intent bug immediately as a loud failure, rather than Linux's default silently making the un-considered case "work" (in the wrong direction: the actual incident here is the reverse leak, but the same principle — an unexamined default silently doing the "wrong for this platform" thing — is what let it ship unnoticed on whichever platform's default happened to align with what was needed).

---

**09-P46.** Propose building the service's internal I/O core around a completion-oriented design from the start — issue-then-await-completion, with per-operation context objects (as in 09-P43's IOCP-based echo core) — even if the primary deployment platform is Linux, because a completion-oriented internal design maps onto epoll cleanly via a thin adapter that issues the read/write immediately upon "readiness" (epoll notifies readiness, the adapter immediately performs the read and synthesizes a completion callback), whereas the reverse port (readiness-oriented internal design onto IOCP) is structurally awkward: IOCP has no "tell me when ready so I can decide when to issue the read" primitive to emulate readiness-then-issue without either polling artificially or restructuring the core logic anyway. Justification: completion-oriented code already embeds "the operation is in flight, here's its result when done" as its fundamental unit of work, which epoll's readiness model can be trivially wrapped to simulate (issue on readiness, treat the synchronous result as an immediate completion); readiness-oriented code embeds "wait to be told it's safe to act," which has no cheap way to fabricate the *pending, results-delivered-later* semantics IOCP fundamentally requires without a substantial internal rewrite — so building completion-oriented internally, even primarily targeting Linux, minimizes the eventual cross-platform restructuring cost.

---

**09-P47.** Concrete safeguards: (1) set `vm.overcommit_memory = 2` (strict overcommit accounting) with an appropriately sized `vm.overcommit_ratio`/`CommitLimit` for services where predictable, immediate allocation failure is preferable to a later kernel-selected kill — trading Linux's default permissiveness for Windows-like immediate-failure semantics; (2) apply per-service memory cgroup limits (`memory.max` under cgroup v2) so a single misbehaving service is OOM-killed *within its own cgroup* rather than the kernel's global OOM killer selecting an arbitrary victim process elsewhere on the host, containing the blast radius to the offending service itself; (3) have each service self-monitor its own RSS/committed-memory trend against its cgroup limit and proactively shed load, shrink caches, or restart itself gracefully via `madvise(MADV_DONTNEED)` on reclaimable regions well before the cgroup limit is hit, converting a kernel-forced kill into a self-initiated, graceful degradation. Monitoring signal needed: per-service (per-cgroup) memory-usage-versus-limit trend, alerting well before the limit is reached, rather than relying on discovering the problem only via a kill-event log entry after the fact.

---

**09-P48.** Concrete policy: (1) add a static-analysis/lint rule (or a code-review-enforced checklist item) that flags any call inside a function reachable from `DllMain` to thread-creation APIs, `LoadLibrary`, or any Win32 API not on Microsoft's documented "safe to call from `DllMain`" allow-list; (2) mandate, as a codebase-wide pattern, that every DLL's actual initialization logic lives in a separate, lazily-invoked explicit-init function (e.g. called on first use of the DLL's public API, or via a dedicated `Initialize()` export the host calls explicitly after `LoadLibrary` returns) rather than in `DllMain` itself, reducing `DllMain` to trivial bookkeeping; (3) add a CI stress test that deliberately varies DLL load order (and, where feasible, injects artificial delays at DLL-attach time) across multiple runs to surface load-order-dependent deadlocks before release, since the postmortem's own scenario explicitly depended on "a specific, rare load-order condition." Why "be careful" isn't sufficient alone: the hazard is non-obvious, its manifestation is load-order- and timing-dependent (so ordinary testing under typical load order won't reliably reproduce it), and it can be introduced by anyone touching `DllMain` months after the original author is no longer reviewing it — a policy that depends on every future developer independently remembering an easily-violated, non-enforced rule is exactly the kind of safeguard that predictably fails silently until an incident forces a postmortem.

### Level 7 — Principal Reasoning

**09-P49.** Linux does have partial systemwide tracing infrastructure — `perf` can sample system-wide across all processes with low per-event overhead, and kernel tracepoints plus eBPF allow attaching low-overhead, always-resident instrumentation to kernel and (via uprobes) userspace events without per-process attach — but these only partially approximate ETW's specific value proposition: ETW additionally provides a standardized, structured, provider/consumer model that ordinary application code is expected to emit *semantically meaningful, versioned events* into directly (not just "sample stack traces" or "trace this syscall"), consumed uniformly by systemwide tools without each application needing bespoke eBPF instrumentation written by a specialist to get equivalent structured visibility — eBPF-based tracing on Linux is powerful but requires meaningfully more specialized tooling/expertise per instrumentation point than ETW's built-in "just call `TraceLoggingWrite`" application-level story. Proposed standard: for ad hoc, single-incident investigation, an incident responder should reach for `strace`/gdb-style attach on Linux and WinDbg/live-attach on Windows first (comparable single-process tools on both sides); for standing, always-on, low-overhead production observability, the standard should lean on ETW natively on Windows, and on Linux should invest specifically in a curated set of pre-built eBPF-based tracepoints/exporters (rather than expecting each responder to write ad hoc eBPF on demand) precisely because the "continuously observe the whole fleet with negligible overhead, using events applications themselves emit" capability is native and low-friction on Windows and requires deliberate up-front tooling investment to approximate on Linux — the organization should not assume the two platforms are equally ready for this out of the box.

---

**09-P50.** None of the three is correct unconditionally, but tradeoffs favor a specific default with an explicit escape valve. Option (c) — no shared abstraction at all — imposes a real cost on every call site across the whole organization for a problem best solved once, centrally, and produces exactly the divergent, uncoordinated handling this chapter's error-translation material warns against (recall 09-P34's "forwarding a raw platform error number" bug is a special case of "no shared translation layer"); it should be rejected as a default. Option (b) — aggressive normalization to a small neutral taxonomy — is the right default for the overwhelming majority of call sites, because most calling code only needs to make a small number of decisions (retry? surface to the user? fail the request?) that a handful of neutral categories (`not_found`, `access_denied`, `already_exists`, `resource_exhausted`, `unknown`) fully support, and it keeps cross-platform business logic genuinely platform-agnostic. Option (a) — exposing native error detail through an escape hatch alongside the normalized category — should be the sanctioned deviation, reserved for specific call sites that have a demonstrated, concrete need for platform-native detail (e.g., a diagnostics/logging layer that wants to record the raw `errno`/Win32 code for later cross-referencing with platform documentation, or a rare call site whose correct handling genuinely differs based on a platform-specific detail the neutral taxonomy can't capture) — never as the default because it re-invites platform-specific branching to leak into ordinary business logic. A code-review checklist item enforcing this: any call site accessing the raw/native error escape hatch (rather than the normalized category) must have an inline comment or linked ticket justifying the specific reason the normalized category was insufficient — making the deviation visible and reviewable rather than a silent, gradually-spreading habit.

## Integration Challenge Solution — 09-IC1

Chosen pairing: **kernel handles**, extending 09-P41's `UniqueHandle` wrapper into a full portable file-handle abstraction.

1. **Public contract:** `class PortableFile` — `static PortableFile open(path, mode)` (throws/returns an error on failure via this chapter's neutral error taxonomy from 09-P50), `size_t read(void* buf, size_t n)`, `size_t write(const void* buf, size_t n)`, move-only, RAII-closing in its destructor, `bool valid() const`. The contract guarantees: a `PortableFile` that reports `valid()` owns exactly one open OS-level resource; destruction always releases it exactly once; `read`/`write` may transfer fewer bytes than requested (per 09-P04's short-read/write reality) and callers must loop if a full transfer is required; no caller-visible member ever exposes a raw fd or `HANDLE` value.

2. **Linux implementation:** wraps `open()`/`read()`/`write()`/`close()`, with the internal fd defaulting to `O_CLOEXEC` at open time so the abstraction itself picks a single, deliberate inheritance policy rather than leaving it to whatever the platform default happens to be (directly informed by 09-P29's leak and 09-P45's mandated-explicitness lesson). **Windows implementation:** wraps `CreateFile`/`ReadFile`/`WriteFile`/`CloseHandle`, with `bInheritHandle = FALSE` by default for the same reason, using 09-P41's sentinel-hiding technique so `INVALID_HANDLE_VALUE` versus `-1` never leaks into `PortableFile`'s own interface.

3. **Test suite (GoogleTest):** `PortableFileTest.WriteThenReadRoundTrips`, `PortableFileTest.OpenNonexistentReturnsNotFoundCategory`, `PortableFileTest.MoveTransfersOwnership`, `PortableFileTest.DestructorReleasesUnderlyingResource` (verified, per-platform, via `/proc/self/fd` count before/after on Linux and a handle-count query on Windows, kept inside the test's platform-specific `#ifdef` setup/teardown rather than the assertions themselves, which stay platform-neutral). Running unmodified on both platforms: all four pass on both, since the contract (not the mechanism) is what's asserted.

4. **Hardest divergence:** the two platforms' error information content genuinely differs on the same failure — a nonexistent path gives Linux's `open()` a POSIX `ENOENT` (retrievable via `errno`, a simple integer with a well-known meaning) but gives Windows's `CreateFile` a `GetLastError()` code that must itself be separately queried after the call (not returned inline), and the space of possible codes/categories doesn't line up one-to-one (recall 09-QC8). The design decision made to reconcile it: adopt 09-P50's proposed default — translate both into the same small neutral category (`FileError::NotFound`, etc.) at the boundary inside each platform's `open()` implementation, and additionally retain the raw native code in a separate, optional diagnostic field on the returned error object (the sanctioned escape hatch from 09-P50) rather than either discarding it entirely or forcing every call site to branch on two different native error types — directly applying 09-P46's and 09-P48's general principle that the abstraction's *contract* should be uniform even when the underlying mechanism (here, how and when the native error becomes available at all) genuinely differs between platforms.
