# P-2.5 — Tests

## Visible Tests (GoogleTest)

```cpp
struct Point { int32_t x, y; };
// user-written registration, once, drives both directions:
auto describe_fields(Point*) { return std::make_tuple(&Point::x, &Point::y); }

struct Line { Point start, end; };
auto describe_fields(Line*) { return std::make_tuple(&Line::start, &Line::end); }

TEST(Serializer, RoundTripsInt32) {
    Point p{100, -200};
    auto bytes = serialize(p);
    auto result = deserialize<Point>(bytes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().x, 100);
    EXPECT_EQ(result.value().y, -200);
}

TEST(Serializer, RoundTripsAllPrimitiveTypes) {
    struct AllTypes { int32_t i; double d; bool b; std::string s; };
    // ... register, round-trip, compare each field
}

TEST(Serializer, RoundTripsEmptyString) {
    struct HasString { std::string s; };
    HasString h{""};
    auto bytes = serialize(h);
    auto result = deserialize<HasString>(bytes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().s, "");
}

TEST(Serializer, RoundTripsStringWithEmbeddedNullByte) {
    struct HasString { std::string s; };
    HasString h{std::string("ab\0cd", 5)};
    auto bytes = serialize(h);
    auto result = deserialize<HasString>(bytes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().s.size(), 5u);
    EXPECT_EQ(result.value().s, std::string("ab\0cd", 5));
}

TEST(Serializer, RoundTripsThreeLevelsOfNesting) {
    struct Inner { int32_t v; };
    struct Middle { Inner inner; int32_t w; };
    struct Outer { Middle middle; int32_t z; };
    // register all three, round-trip Outer, verify all fields at every level
}

TEST(Serializer, TruncatedBufferIsDetectedNotUB) {
    Point p{1, 2};
    auto bytes = serialize(p);
    bytes.resize(bytes.size() - 2); // truncate mid-field
    auto result = deserialize<Point>(bytes);
    EXPECT_FALSE(result.has_value());
}

TEST(Serializer, TruncatedStringLengthPrefixIsDetected) {
    struct HasString { std::string s; };
    HasString h{"hello world"};
    auto bytes = serialize(h);
    bytes.resize(4); // cut off right after (or within) the length prefix
    auto result = deserialize<HasString>(bytes);
    EXPECT_FALSE(result.has_value());
}

TEST(Serializer, SchemaEvolutionSuppliesDefaultForNewField) {
    struct PersonV1 { std::string name; };
    struct PersonV2 { std::string name; int32_t age = -1; }; // -1 documented default

    PersonV1 old_data{"Ada"};
    auto bytes = serialize(old_data); // serialized under schema version 1

    auto result = deserialize<PersonV2>(bytes);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().name, "Ada");
    EXPECT_EQ(result.value().age, -1); // documented default applied
}

TEST(Serializer, EndianCorrectnessViaManualByteSwapCheck) {
    int32_t value = 0x01020304;
    auto bytes = serialize_raw_int32_le(value); // helper exposing wire encoding
    EXPECT_EQ(static_cast<uint8_t>(bytes[0]), 0x04);
    EXPECT_EQ(static_cast<uint8_t>(bytes[3]), 0x01);
}
```

## Hidden Tests

- a struct with a field type that has no registered serialization support (e.g. a raw pointer) — checked to be a compile-time error (`static_assert`/concept failure), not a runtime surprise
- a schema-version-mismatch case where the version is *newer* than the reader understands — checked against whatever documented policy (hard failure vs. best-effort) was chosen
- round-trip correctness for boundary numeric values (`INT32_MIN`, `INT32_MAX`, `NaN`/`Infinity` for floating-point fields, if supported)
- a deeply nested (4+ levels) struct beyond the minimum 3-level requirement, checking the recursion genuinely has no hardcoded depth limit
