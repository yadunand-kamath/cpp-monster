# P-3.5 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(CliParser, ParsesBooleanFlag) {
    auto spec = Parser{}.flag("verbose", 'v', "enable verbose output");
    auto result = spec.parse({"--verbose"});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->get<bool>("verbose"));
}

TEST(CliParser, ParsesTypedOptionWithDefault) {
    auto spec = Parser{}.option<int>("count", 'c', "how many", /*default=*/1);
    auto result = spec.parse({});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get<int>("count"), 1);
}

TEST(CliParser, ParsesNegativeNumberOptionValueCorrectly) {
    auto spec = Parser{}.option<int>("offset", 'o', "offset");
    auto result = spec.parse({"--offset", "-5"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get<int>("offset"), -5);
}

TEST(CliParser, ParsesValueThatLooksLikeAFlag) {
    auto spec = Parser{}.option<std::string>("file", 'f', "file path");
    auto result = spec.parse({"--file", "--verbose"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->get<std::string>("file"), "--verbose");
}

TEST(CliParser, DispatchesToCorrectSubcommand) {
    auto spec = Parser{}
        .subcommand("build", Parser{}.flag("release", 'r', "release build"))
        .subcommand("test", Parser{}.option<std::string>("filter", 'f', "test filter"));
    auto result = spec.parse({"build", "--release"});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->subcommand_name(), "build");
    EXPECT_TRUE(result->get<bool>("release"));
}

TEST(CliParser, DoubleDashSeparatesPositionalsFromOptions) {
    auto spec = Parser{}.flag("verbose", 'v', "verbose").positional_list("files");
    auto result = spec.parse({"--", "--verbose", "file.txt"});
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->get<bool>("verbose"));
    EXPECT_THAT(result->get_list("files"), ::testing::ElementsAre("--verbose", "file.txt"));
}

TEST(CliParser, UnknownFlagIsDistinguishableError) {
    auto spec = Parser{}.flag("verbose", 'v', "verbose");
    auto result = spec.parse({"--nonexistent"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ParseErrorKind::UnknownOption);
}

TEST(CliParser, MissingRequiredOptionIsDistinguishableError) {
    auto spec = Parser{}.option<std::string>("file", 'f', "file", Required{});
    auto result = spec.parse({});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ParseErrorKind::MissingRequired);
}

TEST(CliParser, WrongTypeValueIsDistinguishableError) {
    auto spec = Parser{}.option<int>("count", 'c', "count");
    auto result = spec.parse({"--count", "notanumber"});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, ParseErrorKind::TypeMismatch);
}

TEST(CliParser, GeneratedHelpTextMatchesCurrentSpec) {
    auto spec = Parser{}.flag("verbose", 'v', "enable verbose output")
                         .option<int>("count", 'c', "how many", 1);
    EXPECT_EQ(spec.help_text(), R"(Usage: tool [options]

Options:
  -v, --verbose       enable verbose output
  -c, --count=<int>   how many (default: 1)
)");
}
```

## Hidden Tests

- adding a new option to the spec and re-checking the snapshot help-text test fails until updated — proving the coupling between spec and generated help is real, not coincidental
- a repeated-option case, checked against whichever policy (error / last-wins / accumulate) is documented
- a subcommand-local option colliding by name with a global option, checked against the documented scoping decision
- a malformed specification (two options sharing the same name) triggering whichever rejection mechanism (compile-time or startup-time) was chosen, with a clear message identifying the collision
- an enum-restricted-string-set option given a value outside the allowed set, producing a `TypeMismatch`-category error naming the allowed values
