# P-5.6 — Tests

## Visible Tests (GoogleTest)

Assumes example plugins built as separate shared libraries within the test suite: `example_plugin_valid.{so,dll}`, `example_plugin_version_mismatch.{so,dll}`, `example_plugin_null_interface.{so,dll}`, `example_plugin_throws_internally.{so,dll}`.

```cpp
TEST(PluginHost, FullLifecycleLoadCallUnload) {
    auto plugin = PluginHost::load(path_to("example_plugin_valid"));
    ASSERT_TRUE(plugin.has_value());
    int result = plugin->interface()->add(2, 3);
    EXPECT_EQ(result, 5);
    // plugin goes out of scope here — RAII unload per Hint 3
}

TEST(PluginHost, VersionMismatchIsRejectedWithClearDiagnostic) {
    auto plugin = PluginHost::load(path_to("example_plugin_version_mismatch"));
    ASSERT_FALSE(plugin.has_value());
    EXPECT_NE(plugin.error().message.find("version"), std::string::npos);
    EXPECT_NE(plugin.error().message.find(std::to_string(kHostSupportedVersion)), std::string::npos);
}

TEST(PluginHost, NullInterfacePointerIsCleanRejectionNotCrash) {
    auto plugin = PluginHost::load(path_to("example_plugin_null_interface"));
    EXPECT_FALSE(plugin.has_value());
    EXPECT_EQ(plugin.error().kind, PluginErrorKind::kNullInterface);
}

TEST(PluginHost, InvalidSharedLibraryFileIsClearLoadError) {
    write_file("not_a_real_library.so", "definitely not ELF or PE bytes");
    auto plugin = PluginHost::load("not_a_real_library.so");
    EXPECT_FALSE(plugin.has_value());
    EXPECT_EQ(plugin.error().kind, PluginErrorKind::kLoadFailed);
    EXPECT_FALSE(plugin.error().platform_message.empty()); // dlerror()/GetLastError() surfaced
}

TEST(PluginHost, MissingFileIsClearLoadError) {
    auto plugin = PluginHost::load("this_file_does_not_exist.so");
    EXPECT_FALSE(plugin.has_value());
    EXPECT_EQ(plugin.error().kind, PluginErrorKind::kLoadFailed);
}

TEST(PluginHost, ExceptionInsidePluginDoesNotCrossBoundary) {
    auto plugin = PluginHost::load(path_to("example_plugin_throws_internally"));
    ASSERT_TRUE(plugin.has_value());
    int error_code = plugin->interface()->risky_operation(/*trigger_internal_exception=*/true);
    EXPECT_NE(error_code, 0); // translated to a C-compatible error code, not an uncaught exception
}

TEST(PluginHost, TwoPluginsWithSameSymbolNameDoNotInterfere) {
    auto plugin_a = PluginHost::load(path_to("example_plugin_valid"));
    auto plugin_b = PluginHost::load(path_to("example_plugin_valid_variant")); // exports a same-named internal symbol
    ASSERT_TRUE(plugin_a.has_value());
    ASSERT_TRUE(plugin_b.has_value());
    EXPECT_EQ(plugin_a->interface()->add(2, 3), 5);
    EXPECT_EQ(plugin_b->interface()->add(2, 3), 5); // both correct despite shared internal symbol name
}

TEST(PluginHost, UnloadedPluginInterfaceIsNotAccessibleThroughApi) {
    PluginHandle plugin = *PluginHost::load(path_to("example_plugin_valid"));
    plugin.unload_explicitly();
    EXPECT_FALSE(plugin.is_loaded());
    // interface() on an unloaded handle is a compile-time or documented-precondition error, not silently unsafe
}
```

## Hidden Tests

- an ASan/valgrind-clean run of the full load/call/unload lifecycle test, confirming no leak of the loaded library handle or interface-related host bookkeeping
- a repeated load-unload-reload cycle test (the same plugin loaded and unloaded many times in sequence) confirming no resource exhaustion or state corruption across cycles
- an older-supported-version plugin (if the host's documented policy allows backward compatibility) loading successfully and exposing only the subset of interface functionality that version defines
- a newer-than-supported-version plugin correctly rejected per the documented forward-compatibility policy
- a platform-specific symbol-visibility verification test confirming the Windows build actually requires explicit export markings (i.e., an unmarked function is confirmed NOT resolvable via `GetProcAddress`), directly validating Hint 4's stated platform divergence rather than assuming it
