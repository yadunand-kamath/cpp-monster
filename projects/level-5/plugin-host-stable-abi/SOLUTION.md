# P-5.6 — Solution

## Reference Architecture

The shared interface header — the one artifact both host and plugins compile against (Hint 1):

```c
// plugin_interface.h — valid C, usable from C++, C, or any language with C-ABI FFI
#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_INTERFACE_VERSION 1

typedef struct PluginInterface {
    uint32_t version; // first member, checked before anything else is touched — Hint 1
    int32_t (*add)(int32_t a, int32_t b);
    int32_t (*risky_operation)(int32_t trigger_internal_exception); // 0 = success, nonzero = translated error
} PluginInterface;

typedef const PluginInterface* (*PluginEntryPointFn)(void);
#define PLUGIN_ENTRY_POINT_NAME "get_plugin_interface"

#ifdef __cplusplus
}
#endif
```

The plugin side, showing Hint 2's exception-translation pattern:

```cpp
extern "C" int32_t risky_operation(int32_t trigger_internal_exception) {
    try {
        if (trigger_internal_exception) throw std::runtime_error("internal plugin failure");
        do_real_work();
        return 0;
    } catch (...) {
        return -1; // translated to a C-compatible sentinel — never lets the exception reach the boundary
    }
}

extern "C" PLUGIN_EXPORT const PluginInterface* get_plugin_interface() {
    static const PluginInterface iface{PLUGIN_INTERFACE_VERSION, &add, &risky_operation};
    return &iface;
}
```

The host's RAII handle (Hint 3):

```cpp
class PluginHandle {
public:
    static std::expected<PluginHandle, PluginError> load(const std::string& path) {
        PlatformLibraryHandle lib = platform_load_library(path); // dlopen/LoadLibrary
        if (!lib) return std::unexpected(PluginError::load_failed(platform_last_error()));

        auto entry = reinterpret_cast<PluginEntryPointFn>(platform_get_symbol(lib, PLUGIN_ENTRY_POINT_NAME));
        if (!entry) { platform_unload_library(lib); return std::unexpected(PluginError::symbol_not_found()); }

        const PluginInterface* iface = entry();
        if (!iface) { platform_unload_library(lib); return std::unexpected(PluginError::null_interface()); }

        if (iface->version != PLUGIN_INTERFACE_VERSION) {
            platform_unload_library(lib);
            return std::unexpected(PluginError::version_mismatch(iface->version, PLUGIN_INTERFACE_VERSION));
        }
        return PluginHandle(lib, iface);
    }

    const PluginInterface* interface() const { return iface_; } // only path to the function pointers
    ~PluginHandle() { if (lib_) platform_unload_library(lib_); } // RAII unload

private:
    PluginHandle(PlatformLibraryHandle lib, const PluginInterface* iface) : lib_(lib), iface_(iface) {}
    PlatformLibraryHandle lib_;
    const PluginInterface* iface_;
};
```

## Design Rationale

**Why must the version field be the very first member of `PluginInterface`, and why check it before touching any other field?** If the host and a plugin disagree about the struct's layout (a stale rebuild, an interface change one side didn't pick up), the *only* safe assumption the host can make about a pointer of a supposedly-`PluginInterface`-shaped struct is where its very first field sits — any field after that is only safely readable once the version has confirmed both sides agree on the layout that follows. Checking the version first is what makes the check itself trustworthy rather than being a check that might already be reading garbage from a misaligned field.

**Why does the plugin's own code, not the host, catch exceptions with `catch (...)`?** The host is calling into a C function pointer that, from the host's perspective, could be implemented in any language with C-ABI FFI compatibility — C, Rust, or C++ compiled by an entirely different (possibly incompatible) compiler/version than the host. There is no safe, portable way for the host to "catch" a C++ exception thrown by code it doesn't control the compilation of; the only sound design puts the responsibility for exception safety at the boundary on whichever side has full knowledge of what kind of code sits behind that specific function pointer — which is the plugin itself.

**Why does `PluginHandle::interface()` return a raw pointer rather than, say, a reference or a smart pointer wrapping the interface struct?** The interface struct is owned by the plugin (it returned a pointer to static, plugin-owned storage) — the host never owns or is responsible for freeing it. A raw pointer accurately represents this non-owning relationship; wrapping it in a smart pointer on the host side would incorrectly imply host-side ownership/lifetime responsibility over memory the host has no actual control over.

## Reference Implementation

The above covers the interface header's shape, the plugin-side exception-translation pattern, and the host's RAII load/validate sequence. Remaining work for the learner: the `platform_load_library`/`platform_get_symbol`/`platform_unload_library`/`platform_last_error` wrappers (thin, platform-specific shims over `dlopen`/`dlsym`/`dlclose`/`dlerror` and `LoadLibrary`/`GetProcAddress`/`FreeLibrary`/`GetLastError`+`FormatMessage`), the example plugins (valid, version-mismatched, null-interface, throws-internally, and a same-symbol-name variant for the non-interference test), and the `PLUGIN_EXPORT` macro (expanding to nothing meaningful on Linux, where non-static symbols export by default, and to `__declspec(dllexport)` on Windows, per Hint 4's stated platform divergence).

## Testing Strategy

Build and verify the Windows export-visibility divergence explicitly (per the Hidden Tests' symbol-visibility test) rather than assuming it — confirm directly that an unmarked function genuinely fails to resolve via `GetProcAddress` on Windows, since trusting a description of the platform difference without observing it firsthand is exactly the kind of gap that later produces a confusing failure when a real interface change accidentally omits the export marking.

## Performance Analysis

Not a performance-sensitive project — load/unload and symbol resolution are one-time, infrequent operations relative to any real workload a plugin might perform; the interesting cost here is correctness risk (ABI mismatch, exception-crossing-boundary UB), not throughput or latency.

## Failure Modes

- A C++ type slipping into the interface struct because it "happened to work" during testing (e.g. a `std::string` that worked because host and plugin were built with the exact same compiler and standard library version) — fragile and not actually meeting the "stable C ABI" requirement, since it would break the moment either side's toolchain changed.
- An interface change made without bumping the version number, silently reintroducing the exact struct-layout-mismatch risk the version check exists to catch — the version check is only as reliable as the discipline of incrementing it.
- Calling `dlclose`/`FreeLibrary` while any host code still holds a copy of a function pointer obtained from that plugin's interface, leaving a dangling pointer that will crash (or worse, silently execute unrelated code) on next call — this is why `PluginHandle`'s design in Hint 3 makes the interface pointer's accessible lifetime tied directly to the handle's own lifetime.

## Extensions

- A plugin discovery mechanism (scanning a directory for plugin files matching a naming convention) built on top of this single-plugin-load primitive.
- Multiple interface versions supported simultaneously by the host, with version-specific dispatch, extending the single-version-only policy documented here into genuine backward compatibility.
