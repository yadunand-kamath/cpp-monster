# P-3.6 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(PrecedenceMerge, CliOverridesAllOtherLayers) {
    LayerValues defaults{{"port", "8080"}};
    LayerValues file{{"port", "9090"}};
    LayerValues env{{"port", "7070"}};
    LayerValues cli{{"port", "6060"}};
    auto merged = merge_layers({defaults, file, env, cli});
    EXPECT_EQ(merged.at("port"), "6060");
}

TEST(PrecedenceMerge, FileOverridesDefaultsWhenEnvAndCliUnset) {
    LayerValues defaults{{"port", "8080"}};
    LayerValues file{{"port", "9090"}};
    auto merged = merge_layers({defaults, file, {}, {}});
    EXPECT_EQ(merged.at("port"), "9090");
}

TEST(PrecedenceMerge, UnsetEverywhereFallsThroughToDefault) {
    LayerValues defaults{{"port", "8080"}};
    auto merged = merge_layers({defaults, {}, {}, {}});
    EXPECT_EQ(merged.at("port"), "8080");
}

TEST(PrecedenceMerge, KeyAbsentFromAllLayersIsNotPresentInMerge) {
    auto merged = merge_layers({{}, {}, {}, {}});
    EXPECT_FALSE(merged.contains("unknown_key"));
}

TEST(SchemaValidation, CollectsMultipleSimultaneousErrors) {
    Schema schema = Schema{}
        .require<int>("port", Range{1, 65535})
        .require<std::string>("host");
    LayerValues broken{{"port", "not_a_number"}}; // host missing entirely
    auto errors = validate(schema, broken);
    EXPECT_EQ(errors.size(), 2);
}

TEST(EdgeCases, MissingConfigFileFallsThroughCleanly) {
    auto loader = ConfigLoader(schema_with_defaults(), "/nonexistent/path.cfg");
    auto result = loader.load();
    ASSERT_TRUE(result.has_value());
}

TEST(EdgeCases, MalformedConfigFileIsDistinctErrorFromValidationFailure) {
    auto path = write_temp_file("{{{ not valid syntax");
    auto loader = ConfigLoader(schema_with_defaults(), path);
    auto result = loader.load();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ConfigErrorKind::ParseError);
}

TEST(HotReload, ChangingFileUpdatesSpecificKeyAndNotifiesSubscriber) {
    auto path = write_temp_file("port=8080\n");
    ConfigLoader loader(schema_with_defaults(), path);
    loader.enable_hot_reload();
    ChangeCollector changes;
    loader.subscribe([&](const ConfigChangeEvent& e) { changes.push(e); });
    write_temp_file(path, "port=9090\n");
    ASSERT_TRUE(changes.wait_for_key_change("port", "9090", 2s));
}

TEST(HotReload, InvalidReloadDoesNotReplaceLastKnownGood) {
    auto path = write_temp_file("port=8080\n");
    ConfigLoader loader(schema_with_defaults(), path);
    loader.enable_hot_reload();
    ChangeCollector changes;
    loader.subscribe([&](const ConfigChangeEvent& e) { changes.push(e); });
    write_temp_file(path, "port=not_a_number\n");
    ASSERT_TRUE(changes.wait_for_validation_failure(2s));
    EXPECT_EQ(loader.current()->get<int>("port"), 8080); // unchanged
}
```

## Hidden Tests

- an empty-string environment variable, checked against the documented set-vs-unset policy
- a concurrent-read-during-reload stress test run specifically under ThreadSanitizer, hammering `loader.current()` from several threads while reload events fire
- the config file being deleted while hot-reload watching is active, confirming no crash and the documented last-known-good fallback
- a schema range/enum-constraint violation (value present, correct type, out of allowed range), confirmed as its own distinguishable validation error category
- a stress test rapidly rewriting the config file many times in succession, confirming every published configuration observed by readers is a fully valid, non-torn snapshot (never a mix of old/new fields)
