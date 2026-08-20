# Chapter 09 — Systems Programming: Linux and Windows Side by Side

> Prerequisites: [Chapter 02](../02-lifetime-raii/CONCEPTS.md) (RAII is how you own an fd/HANDLE), [Chapter 06](../06-error-handling/CONCEPTS.md) (errno vs `GetLastError` is an error-channel design problem), [Chapter 07](../07-object-model/CONCEPTS.md) (mmap'd bytes becoming objects needs the lifetime rules).
> This chapter is about what the C++ standard library is hiding underneath `std::thread`, `std::filesystem`, and `std::ifstream` — the actual operating-system primitives, taught once as a concept and then twice as a realization, because the two realizations are genuinely different models, not just different spellings of the same idea.

## Crash Course

### Kernel Object Handles: File Descriptors and `HANDLE`s

Every OS-mediated resource — an open file, a socket, a pipe, a mapped region — is represented to a process by an opaque **kernel object handle**, not by the resource's memory address; the kernel owns the real state, and the process just holds a token referring to it.

**Linux:** a file descriptor is a small non-negative `int`, an index into the process's per-process file-descriptor table. `open()`/`read()`/`write()`/`close()` operate on it; POSIX guarantees the lowest available number is reused, which is precisely why closing the wrong fd or double-closing one causes silent corruption elsewhere. File descriptors are **inherited by child processes by default** across `fork`/`exec` unless explicitly marked `O_CLOEXEC` (or `fcntl(fd, F_SETFD, FD_CLOEXEC)`).

**Windows:** a `HANDLE` is an opaque, process-relative value (not a small sequential integer, and not safe to assume anything about numerically) returned by `CreateFile`/`CreateFileMapping`/etc., closed with `CloseHandle`. Handles are **not inherited by default** — inheritance to a child process requires explicitly setting `bInheritHandle = TRUE` in the `SECURITY_ATTRIBUTES` passed at creation, and the child process must itself be created with `bInheritHandles = TRUE`.

**Divergence:** the default is inverted between platforms — Linux hands fds to children unless told not to; Windows withholds handles from children unless told to. Code that assumes "resources don't leak across `exec`/`CreateProcess` by default" is correct on exactly one of the two platforms.

### Virtual Memory and Memory Mapping

Both OSes let a process map a region of virtual address space backed by a file (or by nothing, for anonymous memory) instead of reading/writing through file-position-based calls.

**Linux:** `mmap()` creates the mapping, `munmap()` removes it, `mprotect()` changes page permissions, `madvise()` hints usage patterns (e.g. `MADV_DONTNEED`) to the kernel's page reclaim policy. Linux's default memory accounting **overcommits**: `mmap`/`malloc` can succeed for more virtual memory than physical RAM + swap can actually back, deferring the failure to the moment a page is *touched* (which can then trigger the OOM killer rather than returning an error from the original call).

**Windows:** `CreateFileMapping` creates a mapping object, `MapViewOfFile` maps a view of it into the process's address space; anonymous memory instead goes through `VirtualAlloc`, which requires an explicit two-step **reserve** (claim address space, no backing yet) then **commit** (actually back it, charged against the pagefile limit) — `VirtualAlloc` with `MEM_COMMIT` can fail immediately, at allocation time, if the pagefile-backed commit charge would be exceeded.

**Divergence:** Linux conflates reserve and commit and defers the failure past the allocation call; Windows separates them and can fail *at* the allocation call if the commit charge doesn't fit — this is the single biggest reason "the same" allocation pattern is closer to correct on Windows than on Linux, and closer to "way too generous" on Linux than on Windows.

### Async Signalling and Interruption

**Linux (POSIX):** `signal()`/`sigaction()` register handlers for asynchronous events (SIGINT, SIGSEGV, SIGTERM, …) that can interrupt a thread at essentially any instruction, including mid-syscall — a blocking syscall interrupted by a signal typically returns early with `errno == EINTR`, and correctly-written blocking-syscall call sites must handle that by retrying. Only a small, standardized set of functions are **async-signal-safe** (safe to call from inside a handler); calling most of the standard library, `malloc`, or iostreams from a handler is undefined behavior.

**Windows:** has no direct equivalent to POSIX signals. Hardware/CPU-level faults (access violation, divide-by-zero) surface through **Structured Exception Handling (SEH)** — `__try`/`__except` — which is a stack-based unwind-and-dispatch mechanism triggered synchronously at the faulting instruction, not an asynchronous interruption of arbitrary code. Console control events (Ctrl+C, window close) are delivered via a registered callback through `SetConsoleCtrlHandler`, which runs on a separate thread rather than interrupting the main one mid-instruction. **Vectored Exception Handlers (VEH)** offer a process-wide, chainable alternative to SEH's per-frame handlers.

**Divergence:** POSIX signals genuinely interrupt in-flight syscalls (hence `EINTR`); SEH is a synchronous fault-handling mechanism with no `EINTR`-like concept, and Windows I/O calls simply don't get randomly interrupted mid-call by "signals" the way POSIX ones can.

### Scalable I/O: Readiness vs. Completion

Both platforms offer a way to monitor many I/O sources without one thread per source, but the two models are structurally different, not just differently named.

**Linux:** `epoll` is a **readiness** model — you register file descriptors of interest, then `epoll_wait` tells you which ones are *ready* to have a (still-blocking-capable) I/O call issued against them; you still perform the actual `read`/`write` yourself afterward.

**Windows:** **IOCP** (I/O Completion Ports) is a **completion** model — you issue the I/O operation (`ReadFile`/`WriteFile` with an `OVERLAPPED` structure) up front, the kernel performs it asynchronously, and `GetQueuedCompletionStatus` tells you when it has *finished*, handing back the result.

**Divergence:** this is a structural, not cosmetic, difference — a readiness-based event loop issues the I/O call itself after being told "you may"; a completion-based event loop issues the I/O call first and is told "it's done." Porting an epoll-shaped event loop to IOCP (or vice versa) requires restructuring the control flow around when the actual I/O call happens, not just swapping API names.

### Dynamic Loading at Runtime

**Linux:** shared objects (`.so`) can be loaded explicitly with `dlopen()`, symbols resolved by name with `dlsym()`, and unloaded with `dlclose()`. `RTLD_GLOBAL` vs `RTLD_LOCAL` control whether a loaded library's symbols become available for *other* libraries loaded afterward to resolve against (global symbol interposition) — a mechanism with no real Windows equivalent. `RPATH`/`RUNPATH` embedded in a binary (or `LD_LIBRARY_PATH`) influence where the dynamic loader searches for a dependency by name.

**Windows:** `LoadLibrary`/`LoadLibraryEx` load a `.dll` explicitly, `GetProcAddress` resolves a symbol by name, `FreeLibrary` unloads. Each DLL has its own **per-module import table** — there is no process-wide global symbol namespace two unrelated DLLs share by default the way `RTLD_GLOBAL` allows on Linux. DLL search order (application directory, system directories, `PATH`, …) determines which physical file satisfies a given name, and a DLL's `DllMain` entry point runs under the **loader lock**, during which calling back into the loader (e.g. loading another DLL) or several other operations is unsafe — a subtle reentrancy hazard with no Linux equivalent, since `.so` initializers don't run under an analogous global lock.

**Divergence:** Linux's global-symbol-interposition option (`RTLD_GLOBAL`) has no Windows counterpart; Windows's `DllMain`-under-loader-lock reentrancy hazard has no direct Linux counterpart — each platform has a runtime-loading footgun the other structurally doesn't.

### Inspection and Observability Tooling

**Linux:** `ptrace` is the underlying syscall that lets one process attach to and control another (single-stepping, reading memory, intercepting syscalls) — `gdb` and `strace` are both built on it. `strace` traces a process's syscalls; `perf` samples CPU counters and call stacks for profiling.

**Windows:** WinDbg (and Visual Studio's debugger) attach to and control a process similarly to gdb, but the systemwide, always-on tracing story is different — **ETW (Event Tracing for Windows)** is a low-overhead, systemwide, always-available tracing infrastructure that any process can emit structured events into and any consumer can subscribe to, without per-process attach; Process Monitor is a GUI front-end over a subset of ETW-like filesystem/registry/process events. `!analyze` is a WinDbg extension command that automates common crash-dump triage.

**Divergence:** `ptrace`-based tools are fundamentally single-process-attach; ETW is fundamentally systemwide and always-recording (subject to what providers are enabled) — the tooling philosophy differs, not just the command names.

## Common Misconceptions

1. **"A file descriptor and a `HANDLE` are just two names for the same idea, so code can treat them interchangeably behind a thin typedef."** They differ in more than spelling: fds are small reused integers inherited by children by default; `HANDLE`s are opaque values never inherited by default. A portability shim that only renames the type without renumbering the inheritance defaults will leak (or fail to leak, in the opposite direction) resources across process creation.

2. **"If `mmap`/`VirtualAlloc` succeeded, the memory is actually backed by RAM or swap and safe to fully touch."** On Linux, a successful `mmap`/`malloc` under overcommit is not a guarantee of backing — touching every page can still trigger the OOM killer later. On Windows, a successful `VirtualAlloc` with `MEM_COMMIT` *is* a real guarantee (the pagefile charge was accepted at that moment) — the two platforms' "success" means different things.

3. **"A blocking syscall interrupted by `EINTR` is a real error and should be treated like one."** `EINTR` typically means "a signal was delivered while you were blocked, nothing is actually wrong with the resource" — correct code retries the call (or uses `SA_RESTART` where applicable), rather than propagating it as a hard failure.

4. **"Since Windows has no signals, Windows programs can't be interrupted by asynchronous events at all."** Windows has its own asynchronous surfaces — console control events via `SetConsoleCtrlHandler`, and vectored exception handlers for hardware faults — they're just structurally different (callback-on-a-separate-thread, and synchronous-fault-dispatch, respectively) from POSIX signals' any-instant interruption of the same thread.

5. **"epoll and IOCP are the same idea with different function names, so a straight find-and-replace port between them is reasonable."** They're opposite models — readiness (you still issue the I/O) vs. completion (the I/O is already done when you're told). A correct port restructures *when* the actual read/write call happens, not just which API is called.

6. **"`dlopen`/`LoadLibrary` failing to find a dependency will produce the same kind of diagnostic on both platforms."** The two platforms' loaders search different paths by different rules (`RPATH`/`RUNPATH`/`LD_LIBRARY_PATH` vs. DLL search order) and report different, platform-specific error information (`dlerror()`'s string vs. a Win32 error code from `GetLastError()` after `LoadLibrary` returns `NULL`) — code that parses or compares error text across platforms is relying on a coincidence, not a contract.

## Quick Checks

**09-QC1.** What is the default inheritance-across-`fork`/`exec` behavior for a Linux file descriptor versus a Windows `HANDLE`, and what must be done explicitly to get the *other* behavior on each platform?

**09-QC2.** Why can a `mmap`/`malloc` call succeed on Linux for more memory than the machine can physically back, while an analogous `VirtualAlloc(..., MEM_COMMIT)` call on Windows is expected to fail immediately in the equivalent situation?

**09-QC3.** What does `EINTR` actually signal, and why is the generally correct response to retry rather than to treat it as a resource failure?

**09-QC4.** Name the Windows mechanism that plays a role most structurally similar to a POSIX signal handler for a hardware fault (e.g. access violation), and explain the one respect in which it is not truly analogous to a signal.

**09-QC5.** What is the fundamental structural difference between epoll's readiness model and IOCP's completion model, in terms of when the actual I/O call happens?

**09-QC6.** What Linux dynamic-loading feature (`RTLD_GLOBAL`) has no direct Windows equivalent, and what Windows dynamic-loading hazard (`DllMain` under the loader lock) has no direct Linux equivalent?

**09-QC7.** Why is `ptrace`-based tooling (gdb, strace) described as fundamentally single-process-attach, while ETW is described as fundamentally systemwide?

**09-QC8.** Give one concrete example (from this chapter) of "the same" error condition surfacing genuinely different diagnostic information on Linux versus Windows, rather than merely a differently-spelled error code for the same information.

## Problems

### Level 1 — Recognition

**09-P01.** Is a Linux file descriptor's small integer value something a program should ever rely on as meaningful (e.g., assuming fd 3 is always "the first file I opened")? Explain what POSIX actually guarantees about fd numbering and why relying on specific values is fragile.

**09-P02.** Given `HANDLE h = CreateFile(...);` followed later by `CloseHandle(h);`, is it safe to call `CloseHandle(h)` a second time on the same value? Explain what category of bug a double-close is, independent of platform.

**09-P03.** A Linux program calls `mmap` requesting 100 GB of anonymous memory on a machine with 16 GB of RAM and no swap, and the call succeeds. Is this necessarily a bug, or is it expected overcommit behavior? State what would actually happen if the program touched all 100 GB.

**09-P04.** Is `read()` on Linux guaranteed to either fully succeed, fully fail, or block until a fixed amount of data is available — or can it legitimately return successfully having transferred fewer bytes than requested? State which, and name the general term for a syscall's return needing to be re-checked and possibly re-issued for this reason.

**09-P05.** Does `epoll_wait` returning that a file descriptor is "ready for reading" guarantee that a subsequent `read()` on it will return the full amount of data requested, some data, or could it still return `EWOULDBLOCK`/`EAGAIN` under certain conditions (e.g., level-triggered vs. edge-triggered semantics, or a race with another thread)? Explain briefly.

**09-P06.** A Windows program calls `LoadLibrary("plugin.dll")` and it returns `NULL`. Is the failure reason available anywhere, and if so, through what specific call?

### Level 2 — Prediction

**09-P07.** A parent process opens a log file (getting fd 4), then calls `fork()` followed by `exec()` to run a child program, without ever calling `fcntl(4, F_SETFD, FD_CLOEXEC)`. Predict whether the child process inherits fd 4 in a usable state, and state the one flag that would have prevented this if set at `open()` time.

**09-P08.** A Windows program creates a file handle via `CreateFile` without setting `bInheritHandle = TRUE` in its `SECURITY_ATTRIBUTES`, then calls `CreateProcess` to launch a child, passing `bInheritHandles = TRUE` to `CreateProcess` itself. Predict whether the child process can use the parent's handle value directly, and explain which of the two settings (the handle's own creation-time flag, or the `CreateProcess` call's flag) is the one that actually gated this outcome.

**09-P09.** A Linux service `mmap`s a 500 MB anonymous region "just in case," touches only 50 MB of it, and this pattern is repeated by 40 separate service instances on a machine with 8 GB RAM. Predict whether the kernel initially rejects any of the `mmap` calls, and what the practical risk is once several instances start touching more of their reserved region simultaneously.

**09-P10.** The equivalent service is ported to Windows, reserving 500 MB via `VirtualAlloc(..., MEM_RESERVE)` per instance but committing only 50 MB via a later `VirtualAlloc(..., MEM_COMMIT)` call, across the same 40 instances on 8 GB RAM (with a correspondingly sized pagefile). Predict what happens differently here compared to the Linux case in 09-P09 if all 40 instances later try to commit their full 500 MB simultaneously.

**09-P11.** A Linux program is blocked in a `read()` call on a slow pipe when the process receives `SIGINT` (Ctrl+C) and the handler simply sets a flag and returns. Predict what `read()`'s return value and `errno` will be immediately after the handler returns, assuming the handler was installed without `SA_RESTART`.

**09-P12.** A Windows console application is running a long computation with no signal-like handler installed for Ctrl+C, but has registered a callback via `SetConsoleCtrlHandler`. Predict, structurally, whether the callback runs on the same thread that was doing the long computation, interrupting it mid-instruction the way a POSIX signal handler would, or runs differently — and explain the practical implication for a computation loop that wants to detect the request.

**09-P13.** A logging library calls `malloc` and writes to a `FILE*` from inside a POSIX signal handler installed for `SIGSEGV`, intending to log diagnostic information before the process dies. Predict whether this is reliable, and name the property such a handler is required to have that this violates.

**09-P14.** A network server uses `epoll` in level-triggered mode and, on a "ready to read" event, reads only half of the available bytes on that fd before moving on to service other ready fds, intending to come back to it later. Predict whether the next `epoll_wait` call will report that same fd ready again (assuming no new data arrived), and explain why, contrasting briefly with what edge-triggered mode would have required instead.

**09-P15.** The same server is ported to Windows using IOCP: it issues an overlapped `ReadFile` for a fixed 4 KB buffer, and the underlying TCP data available is only 1 KB at the moment the read completes. Predict whether the completion notification reports success with fewer bytes transferred, or whether the operation instead stays pending until the full 4 KB arrives.

**09-P16.** A Linux program calls `dlopen("libplugin.so", RTLD_LOCAL)`, and `libplugin.so` in turn depends on a symbol expected to be resolved from a separately, previously `dlopen`'d library that was loaded with `RTLD_GLOBAL`. Predict whether `libplugin.so`'s dependency resolves successfully, and state what would change if the earlier library had instead been loaded with `RTLD_LOCAL`.

### Level 3 — Implementation

**09-P17.** Write a small program that opens a file descriptor (`open()`) without `O_CLOEXEC`, then `fork()`s and `exec()`s a trivial child program (e.g., one that lists its own open file descriptors from `/proc/self/fd`), and confirm empirically that the child sees the inherited fd. Repeat with `O_CLOEXEC` set at `open()` time and confirm the child no longer sees it. Document the exact syscalls/flags used and the observed `/proc/self/fd` listings in both cases.

**09-P18.** Write the Windows equivalent: create a file handle via `CreateFile` with `bInheritHandle` first `FALSE` then `TRUE` in its `SECURITY_ATTRIBUTES`, launch a trivial child process via `CreateProcess` with `bInheritHandles = TRUE` in both cases, and have the child attempt to use the handle value passed to it via a command-line argument (converted appropriately) or `STARTUPINFO` mechanism. Demonstrate the difference in whether the child's use of the handle succeeds, and document the exact API calls and flags used.

**09-P19.** Implement a small program that `mmap`s an anonymous region, writes a recognizable pattern to the first page, then calls `madvise(MADV_DONTNEED)` on that page, then reads it back. Document the actual observed contents after the `madvise` call (on your toolchain) and explain, referencing what `MADV_DONTNEED` actually promises the kernel it may do, why the observed result is consistent with the man page's guarantee (or lack of one) about the region's contents afterward.

**09-P20.** Implement the Windows equivalent of a reserve/commit split: call `VirtualAlloc` with `MEM_RESERVE` for a large region, attempt to write to it before committing (documenting the resulting access violation), then call `VirtualAlloc` again with `MEM_COMMIT` on a subrange, and demonstrate that writes now succeed only within the committed subrange. Document the exact calls, sizes, and observed behavior at each step.

**09-P21.** Write a program that installs a `sigaction`-based handler for `SIGALRM`, sets an interval timer (`setitimer` or `alarm`) that fires during a blocking `read()` on a pipe with no data available, and demonstrates that the `read()` call returns early with `errno == EINTR` rather than continuing to block. Then repeat with the handler's `sa_flags` including `SA_RESTART` and demonstrate the `read()` instead transparently continues blocking (or, if you supply data before it would complete, succeeds without the caller having to detect and retry `EINTR` itself).

**09-P22.** Write a Windows console program that registers a `SetConsoleCtrlHandler` callback, starts a long-running loop on the main thread, and sends itself (or is sent) a `CTRL_C_EVENT`. Demonstrate, with actual observed output/timing, that the callback executes without directly halting the main thread's current instruction, and that the main loop must itself poll a flag (or be otherwise cooperative) to actually stop — contrasting this with the more disruptive nature of a POSIX signal delivered to a thread blocked in a syscall.

**09-P23.** Implement a minimal `epoll`-based single-threaded server that accepts TCP connections and echoes received data back to each client, handling at least two simultaneous connections correctly (i.e., not blocking one connection's I/O behind another's). Document your event loop's structure (registration, `epoll_wait`, dispatch) and demonstrate it working via two concurrent client connections.

**09-P24.** Implement the same echo server's core loop using IOCP instead: issue overlapped `ReadFile`/`WriteFile` operations, dispatch on `GetQueuedCompletionStatus`, and handle at least two simultaneous connections correctly. Document the structural differences between where the actual `recv`/`WSARecv`-equivalent call happens in this implementation versus in 09-P23's `epoll` version — specifically, at what point in the control flow the I/O call is issued relative to when you're notified of readiness/completion.

**09-P25.** Write a small plugin-loading program that uses `dlopen`/`dlsym`/`dlclose` to load a shared object built separately, call one exported function from it, and unload it — and demonstrate, by calling `dlopen` a second time after `dlclose`, whether the library's static/global state was actually reinitialized (i.e., whether unload+reload behaves like a fresh load). Document the actual observed behavior and explain it in terms of what `dlclose` does and does not guarantee.

**09-P26.** Write the Windows equivalent plugin loader using `LoadLibrary`/`GetProcAddress`/`FreeLibrary`, calling one exported function and then unloading, and demonstrate the analogous unload+reload behavior for the DLL's own global/static state. Compare your observation to 09-P25's Linux result and note whether the two platforms' behaviors actually agree in this specific case.

**09-P27.** Using `strace -c` (or equivalent syscall-counting invocation) on a small file-copying program, identify and report the actual syscalls issued (e.g. `open`, `read`, `write`, `close`, and how many times each) for copying a specific file size with a specific buffer size, and explain how the reported `read`/`write` call count relates to the file size divided by the buffer size.

**09-P28.** Using Process Monitor (or an ETW-based equivalent capture) on the Windows-ported version of the same file-copying program, capture and report the analogous file-system operations, and explicitly note at least one piece of information Process Monitor's capture surfaces (e.g., full process/thread context, or a systemwide view including unrelated processes touching the same file) that `strace`'s single-process syscall trace does not surface by default.

**09-P29.** [DEBUG]
```cpp
// parent.cpp (Linux)
int fd = open("secret.log", O_RDWR | O_CREAT, 0600);
pid_t pid = fork();
if (pid == 0) {
    execl("/usr/bin/untrusted_plugin", "untrusted_plugin", nullptr);
}
```
The team is surprised that `untrusted_plugin` — a third-party binary they don't control — appears to have read/write access to `secret.log`'s contents via fd 3, despite never being told that fd exists. Identify precisely why this happens given the code shown, and state the minimal change to `open()`'s flags that would prevent it.

**09-P30.** [DEBUG] A Windows service creates a named pipe handle for inter-process communication with `bInheritHandle = TRUE`, intending only its own trusted child helper process (spawned later via `CreateProcess` with `bInheritHandles = TRUE`) to have access to it — but a security review flags that *any* child process the service spawns afterward, including ones running lower-trust code, also receives this handle if `bInheritHandles = TRUE` is set for that spawn. Identify precisely why marking a handle inheritable at creation time is not, by itself, a mechanism for restricting *which* child receives it, and propose the correct Windows mechanism (hint: explicit handle lists via `STARTUPINFOEX`/`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`) for scoping inheritance to a specific child.

### Level 4 — Debugging

**09-P31.** [DEBUG] A Linux service allocates a series of large anonymous `mmap` regions over its lifetime, sized generously "for headroom," and runs fine in testing (light load, plenty of free RAM) but gets killed unpredictably in production under heavier concurrent load, with no error ever returned from any `mmap` call, `malloc`, or the service's own code. Identify the most likely mechanism (referencing Linux's overcommit behavior and what happens when overcommitted pages are actually touched under memory pressure) and name the specific system component responsible for the kill.

**09-P32.** [DEBUG] The same service, ported "as-is" to Windows using `VirtualAlloc(..., MEM_RESERVE | MEM_COMMIT)` for the same generous sizes, instead fails immediately at startup on a machine with a modestly sized pagefile, with `VirtualAlloc` itself returning `NULL`. Identify why the failure mode moved from "eventual, unpredictable kill under load" (Linux) to "immediate, deterministic allocation failure" (Windows), and state the one-line fix that would make the Windows port behave more like the original design intent (reserve generously, commit only what's actually used).

**09-P33.** [DEBUG] A Linux program's `SIGCHLD` handler calls `wait()` (not `waitpid()` with `WNOHANG` in a loop) to reap a terminated child, and the program intermittently "misses" reaping some children when several exit in a very short window, leaving zombie processes behind. Identify why a single `wait()` call per handler invocation is insufficient here, referencing the fact that multiple pending instances of the same signal number are not queued (signals of the same number are not stacked — the handler is not guaranteed to run once per child exit).

**09-P34.** [DEBUG] A cross-platform library's error-reporting layer takes whatever `errno` value a Linux syscall produced and directly forwards its *numeric value* across a portability shim, expecting Windows code on the other side to interpret the same number via `GetLastError()`'s semantics. Identify why this is fundamentally broken — not merely inconvenient — referencing the fact that `errno` and Win32 error codes are separate numbering schemes with no guaranteed correspondence, and state the correct approach (translate to a small, deliberately platform-neutral error enum/category at the boundary, not pass the raw number through).

**09-P35.** [DEBUG] An `epoll`-based server registered a socket in level-triggered mode, and a developer, believing they were optimizing by "reducing wakeups," switched it to edge-triggered mode without also changing the socket to non-blocking mode and without changing the read loop to read until `EAGAIN`. The server now periodically stops processing data on some connections entirely, appearing to hang. Identify precisely why edge-triggered mode requires draining a fd to `EAGAIN`/`EWOULDBLOCK` on every readiness notification (rather than reading "some" data, as was safe under level-triggered), and what happens to the remaining unread data's "readiness" signal once an edge-triggered fd is not fully drained.

**09-P36.** [DEBUG] An IOCP-based server posts an overlapped `ReadFile` and, in its completion handler, immediately posts the *next* overlapped read using the *same* `OVERLAPPED` structure and buffer that the just-completed read used, believing this is a harmless reuse pattern; under load, the server exhibits corrupted read data and occasional crashes. Identify why reusing an `OVERLAPPED` structure (or its associated buffer) for a new operation before the previous operation has been fully retired (i.e., before its completion has actually been dequeued and processed) is unsafe, and what per-operation-context pattern (e.g., a pool of per-request "extended OVERLAPPED" structures) avoids it.

**09-P37.** [DEBUG] A plugin system on Linux uses `dlopen(path, RTLD_NOW | RTLD_GLOBAL)` for every plugin, and after several plugins are loaded, one plugin's internal helper function silently starts calling a same-named but semantically different helper function defined in a different, unrelated plugin, causing corrupted behavior with no crash. Identify how `RTLD_GLOBAL`'s global-symbol-interposition semantics allow a later-loaded plugin's non-static, externally-linked symbol to shadow or collide with an earlier-loaded plugin's same-named symbol, and propose the fix (`RTLD_LOCAL` plus giving each plugin's genuinely-internal helpers internal/static linkage or a unique namespace).

**09-P38.** [DEBUG] A Windows DLL's `DllMain` spawns a new thread during `DLL_PROCESS_ATTACH` to perform "background initialization," and the process intermittently deadlocks at startup. Identify why creating a thread (or performing several other operations, such as loading another DLL or calling many Win32 APIs) from within `DllMain` while the loader lock is held is a documented hazard, and state the general guidance (defer non-trivial initialization out of `DllMain`, e.g. to a lazily-called explicit init function) that avoids it.

### Level 5 — Integration

**09-P39.** Design and implement a small, single interface (e.g. `class FileWatcher`) that wraps directory change notification behind one API, backed by `inotify` on Linux and `ReadDirectoryChangesW` on Windows, exposing at least "file created," "file modified," and "file deleted" events uniformly. Document the specific translation your implementation performs between each platform's native event vocabulary and your unified event enum, and identify at least one native event or edge case (e.g., a rename, which each platform reports differently) that required a deliberate design decision to normalize.

**09-P40.** Extend 09-P39's `FileWatcher` with a portable automated test suite (e.g. via GoogleTest) that creates, modifies, and deletes files in a temporary directory and asserts the expected sequence of unified events, and run it unmodified on both a Linux and a Windows build. Report the actual result of running it on both platforms, and, if any event ordering or coalescing behavior differed between platforms even after your normalization layer, document that residual difference explicitly rather than papering over it.

**09-P41.** Design and implement a minimal cross-platform "owning kernel handle" RAII wrapper (a single class template or a pair of thin platform-specific classes behind one interface) that closes an fd (`close()`) on Linux and a `HANDLE` (`CloseHandle()`) on Windows in its destructor, correctly handles move-only ownership transfer, and correctly represents "no handle" (`-1` on Linux vs. `INVALID_HANDLE_VALUE` on Windows are *not* the same sentinel value or even the same type category) without leaking either platform's specific "invalid" representation into client code. Demonstrate it wrapping a real opened file on each platform and verify (e.g. via `/proc/self/fd` or Process Explorer) that the handle is actually released when the wrapper goes out of scope.

**09-P42.** Build a small single-threaded event-loop abstraction with one portable interface (`register_read(fd_or_handle, callback)`, `run()`) implemented once against `epoll` and once against IOCP, sufficient to run the echo server from 09-P23/09-P24 through it unmodified at the call-site level. Document specifically how your abstraction reconciles epoll's "tell me when ready, I call read" model with IOCP's "I called read, tell me when done" model behind the single interface — i.e., where the actual platform-specific I/O call ends up happening in your abstraction's internals on each platform.

**09-P43.** Implement a small process-supervisor prototype that launches a child process, monitors it for unexpected termination, and restarts it with exponential backoff — using `fork`/`exec`/`waitpid`/signals on Linux and `CreateProcess`/`WaitForSingleObject`/(`GenerateConsoleCtrlEvent` or `TerminateProcess` for a graceful-then-forced shutdown sequence) on Windows — behind one interface. Document how your implementation's "ask the child to shut down gracefully, then force-kill after a timeout" logic differs in actual mechanism between the two platforms (Linux: `SIGTERM` then `SIGKILL`; Windows: no direct signal equivalent for a console-less process, so document what your implementation actually does and why).

**09-P44.** Take any two of this chapter's six mandated concept pairings and, for each, write one paragraph identifying a plausible real production incident that would occur *specifically* on one platform and not manifest at all (not just manifest differently) on the other, given the divergence described in this chapter's Crash Course — and explain precisely which platform-specific behavior is the root cause in each case.

### Level 6 — Production

**09-P45.** Your team ships a service that must run identically on Linux and Windows, and a production incident review reveals it silently leaked file handles/descriptors on Windows only, under a specific child-process-spawning code path that worked correctly on Linux (BC-3's exact scenario). Propose a concrete engineering process — code review checklist items, a portable RAII-handle-wrapper mandate (building on 09-P41), and/or an automated test that specifically exercises handle-inheritance-across-process-creation on both platforms in CI — that would have caught this class of bug before it shipped, and explain specifically why the Linux side's default behavior (fd inheritance) masked the bug that Windows's opposite default (no inheritance unless requested) would have caught immediately in the other direction, had the platforms been tested identically from day one.

**09-P46.** Your organization is deciding whether a new latency-sensitive network service should be built on a readiness-based (epoll) or completion-based (IOCP) I/O model on its primary deployment platform, given that the service may eventually need to run on both Linux and Windows. Propose a concrete architecture (e.g., a completion-oriented internal design that can be adapted to epoll via a thin "issue read on readiness" shim, versus a readiness-oriented internal design that maps awkwardly onto IOCP) that minimizes the actual restructuring cost of eventually supporting both models, and justify, given this chapter's readiness-vs-completion divergence, which of the two general design directions ports more cheaply to the other model and why.

**09-P47.** A production incident postmortem concludes that a Linux service's memory-overcommit-driven OOM kill (per 09-P31's mechanism) took down a colocated, unrelated service on the same host, because the kernel's OOM killer selected a different process than the one that actually over-allocated. Propose concrete production safeguards (e.g., `vm.overcommit_memory` policy changes, per-service memory cgroup limits, `madvise`/monitoring-based self-throttling before the kernel is forced to intervene) that would make the actual offending service fail predictably and in isolation instead of triggering a kernel-level decision with collateral damage, and state what monitoring signal would need to exist to catch the over-allocation trend before it reaches this point.

**09-P48.** Your organization ships a Windows DLL whose `DllMain` was found, during an incident postmortem, to have been performing nontrivial initialization (spawning a thread, loading a second DLL with its own `DllMain`) directly inside `DLL_PROCESS_ATTACH` — the exact hazard from 09-P38 — and this had shipped to production undetected for months because it only deadlocked under a specific, rare load-order condition. Propose a concrete engineering policy (e.g., a static-analysis or code-review rule flagging any non-trivial call inside `DllMain`, a mandated "lazy explicit init" pattern for all DLLs in the codebase, and/or a stress test that deliberately varies DLL load order in CI) that would make this class of bug structurally difficult to ship, and state why "just tell developers to be careful in `DllMain`" is not sufficient as the only safeguard.

### Level 7 — Principal Reasoning

**09-P49.** A principal engineer is comparing two philosophies of observability tooling for a fleet of services running on both Linux and Windows: Linux's story is built around `ptrace`-based, single-process, on-demand attach (gdb, strace) plus separate sampling (`perf`); Windows's story additionally offers ETW, a systemwide, always-available, low-overhead tracing infrastructure any process can emit into and any consumer can subscribe to without per-process attach. Reason through what a Linux-side systemwide equivalent would need to provide to close this specific gap (consider that some Linux tracing infrastructure exists — e.g. `perf` system-wide sampling, or kernel tracepoints/eBPF — and reason about whether it actually closes the gap or only partially approximates it), and propose an organization-wide production-observability standard that accounts for the fact that "trace this one process" and "continuously observe the whole fleet with negligible overhead" are not equally well-supported by both platforms' native tooling — addressing specifically what an incident responder should reach for first on each platform and why.

**09-P50.** A principal engineer is asked to define an organization-wide policy for how internal cross-platform libraries should handle the fact that "the same" error condition (e.g., a failed file open, a failed dynamic-library load, a failed memory commit) surfaces fundamentally different diagnostic information on Linux versus Windows — not just a different numeric code for the same fact, but sometimes genuinely different information content (recall 09-QC8). Reason through the tradeoffs between (a) exposing each platform's native error information through a `variant`-like escape hatch alongside a normalized cross-platform category, (b) normalizing aggressively to a small neutral error taxonomy and discarding platform-specific detail, and (c) requiring every call site to handle both platforms' native error types explicitly with no shared abstraction at all — and propose which default your organization's libraries should adopt, under what circumstances a call site should be allowed to deviate from that default, and what a code-review checklist item enforcing the policy would actually check for.

## Integration Challenge — 09-IC1

Pick exactly one of this chapter's six mandated concept pairings (kernel handles, memory mapping, async signalling, scalable I/O, dynamic loading, or observability tooling) and design a single portable interface that wraps it — your interface may reuse or extend 09-P41's handle wrapper, 09-P42's event-loop abstraction, or a new abstraction of your choosing for a different pairing.

1. State the interface's public contract precisely (types, functions, and what guarantees it makes to a caller who never looks at the platform-specific implementation underneath).
2. Implement it against both the Linux and Windows realizations described in this chapter's Crash Course for your chosen pairing.
3. Write a single test suite (e.g. GoogleTest) that exercises the interface's contract — not implementation details — and run it unmodified on both platforms, reporting the actual results.
4. Identify, in writing, the one hardest divergence between the two platforms' native models for your chosen pairing that your abstraction had to paper over, and justify the specific design decision you made to reconcile it (rather than merely asserting that it works) — referencing 09-P44's or 09-P46's reasoning about which underlying model ports more cheaply onto the other where relevant.

## Chapter Projects

This chapter feeds directly into:
- **[P-3.2](../PROJECT_ROADMAP.md) Cross-Platform File Watcher** — draws directly on 09-P39/09-P40's `inotify`/`ReadDirectoryChangesW` unification work and this chapter's dynamic-loading and handle material generally.
- **[P-3.4](../PROJECT_ROADMAP.md) Content-Addressed Duplicate File Detector** — draws on this chapter's file-descriptor/handle, streaming I/O, and observability-tooling material (09-P27/09-P28) for verifying its own I/O behavior.
- **[P-4.1](../PROJECT_ROADMAP.md) Single-Threaded Event Loop (epoll + IOCP, one interface)** — directly extends 09-P23/09-P24/09-P42's readiness-vs-completion event-loop work.
- **[P-4.6](../PROJECT_ROADMAP.md) Process Supervisor (restart w/ backoff)** — directly extends 09-P43's process-supervision prototype.
