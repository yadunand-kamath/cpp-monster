# P-3.5 — Solution

## Reference Architecture

```cpp
enum class ArgKind { Flag, Option, Positional, PositionalList };

struct ArgSpec {
    ArgKind kind;
    std::string name;
    char short_form = 0;
    std::string description;
    std::type_index value_type = typeid(void);
    std::function<Result<std::any, ParseError>(std::string_view)> parse_value;
    std::any default_value;
    bool required = false;
};

class Parser {
public:
    Parser& flag(std::string name, char short_form, std::string desc) {
        specs_.push_back(ArgSpec{ArgKind::Flag, std::move(name), short_form, std::move(desc)});
        return *this;
    }

    template <typename T>
    Parser& option(std::string name, char short_form, std::string desc,
                    T default_value = T{}, Required req = Required{false}) {
        specs_.push_back(ArgSpec{
            ArgKind::Option, std::move(name), short_form, std::move(desc),
            typeid(T),
            [](std::string_view s) -> Result<std::any, ParseError> {
                return parse_value<T>(s).map([](T v) { return std::any(std::move(v)); });
            },
            std::any(default_value), req.value
        });
        return *this;
    }

    Parser& subcommand(std::string name, Parser sub) {
        subcommands_.emplace(std::move(name), std::move(sub));
        return *this;
    }

    Result<ParsedArgs, ParseError> parse(std::span<const std::string_view> argv) const;
    std::string help_text() const; // iterates specs_ only — the single source of truth
private:
    std::vector<ArgSpec> specs_;
    std::unordered_map<std::string, Parser> subcommands_;
};
```

The core token-walking loop, showing the "next token after an option is unconditionally its value" rule from Hint 4:

```cpp
Result<ParsedArgs, ParseError> Parser::parse(std::span<const std::string_view> argv) const {
    ParsedArgs result(*this);
    bool positional_only = false; // set once "--" is seen

    for (std::size_t i = 0; i < argv.size(); ++i) {
        std::string_view tok = argv[i];
        if (!positional_only && tok == "--") { positional_only = true; continue; }

        if (!positional_only && tok.starts_with("--")) {
            auto name = tok.substr(2);
            auto* spec = find_spec_by_name(name);
            if (!spec) return ParseError{ParseErrorKind::UnknownOption, std::string(name)};
            if (spec->kind == ArgKind::Flag) { result.set_flag(spec->name, true); continue; }
            if (i + 1 >= argv.size())
                return ParseError{ParseErrorKind::MissingValue, spec->name};
            auto value = spec->parse_value(argv[++i]); // unconditionally consumes next token
            if (!value) return ParseError{ParseErrorKind::TypeMismatch, spec->name};
            result.set_option(spec->name, std::move(*value));
            continue;
        }
        result.add_positional(tok);
    }

    for (auto& spec : specs_)
        if (spec.required && !result.has(spec.name))
            return ParseError{ParseErrorKind::MissingRequired, spec.name};

    return result;
}
```

## Design Rationale

**Why `std::any` for the option's default/parsed value rather than a `std::variant` of every supported type?** `std::any` lets `option<T>()` remain generic over any type the caller provides a `parse_value<T>` overload for, without the `Parser`/`ArgSpec` types needing to enumerate every possible option type in a closed `variant`. The type-safety cost (a `std::any_cast` mismatch would be a bug, not a user-facing error, since `value_type`/`parse_value` are always set together for the same `T`) is acceptable here because the cast only ever happens against a type this same specification declared.

**Why validate the specification (duplicate names, etc.) at program-startup rather than attempting full compile-time rejection?** Fully `constexpr`-evaluating a builder chain into a `static_assert`-checkable structure is achievable in C++20 but adds substantial complexity (constexpr-friendly string/vector storage, careful avoidance of dynamic allocation in constant-evaluated contexts) for a benefit — catching a spec typo slightly earlier — that a clear, loud startup-time check already delivers with far less machinery. This project documents startup-time validation as the chosen, honest tradeoff, consistent with the Constraints section's explicit invitation to make and justify that choice.

**Why does the next token after a recognized value-taking option get consumed unconditionally, without checking whether it also looks flag-shaped?** Because "looks flag-shaped" is a property of the token's *text*, not of what role it plays in the grammar — an option that is known (from the specification) to require a value has already told the parser, structurally, what the next token's role is. Re-inspecting that token's shape to guess its role a second time is exactly the bug described in Hint 4 and the negative-number/`--file=--verbose` edge cases.

## Reference Implementation

The above covers the builder, the token loop's core value-consumption rule, and the design rationale for the `std::any`/startup-validation choices. Remaining work for the learner: `help_text()`'s formatting pass over `specs_`, subcommand dispatch (peeling off the first positional token and delegating to the matching sub-`Parser`, when subcommands are declared), the `--` separator's interaction with subcommand dispatch, and the repeated-option policy's concrete implementation.

## Testing Strategy

The generated-help-matches-spec snapshot test is only meaningful if it's re-verified whenever the specification changes — treat a spec change that doesn't require a corresponding help-text-test update as a sign the coupling has silently broken (e.g. help text hand-written outside the iteration-over-`specs_` path).

## Performance Analysis

Parsing is O(argv length) with O(1) average-case specification lookups (a hash map from option name/short-form to its `ArgSpec`) — negligible for any realistic CLI invocation; this project's engineering effort is about correctness and API ergonomics, not throughput.

## Failure Modes

- Re-inspecting a value token's shape after already knowing (from the specification) that it must be consumed as a value — misparsing negative numbers and flag-shaped values.
- Hand-writing any part of the help text outside of iterating the specification's declaration list, silently decoupling it from future spec changes.
- Using exceptions as the sole error-reporting path when the project's own stated philosophy (Ch06, [P-2.4](../../level-2/result-error-propagation/STATEMENT.md)) favors `Result`-style propagation — an inconsistency the Constraints section explicitly calls out.

## Extensions

- Shell completion script generation (bash/zsh/PowerShell) derived from the same specification.
- Environment-variable fallback for options (declared per-option, checked when the CLI value is absent), a natural bridge toward [P-3.6](../layered-configuration-loader/STATEMENT.md)'s layered configuration model.
