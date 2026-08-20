# P-3.6 — Solution

## Reference Architecture

```cpp
using LayerValues = std::unordered_map<std::string, std::string>; // raw, untyped strings

LayerValues merge_layers(std::initializer_list<LayerValues> layers_low_to_high) {
    LayerValues merged;
    for (auto& layer : layers_low_to_high)
        for (auto& [k, v] : layer)
            merged[k] = v; // later (higher-precedence) layers overwrite earlier ones
    return merged;
}
```

Schema validation, collecting every error rather than stopping at the first:

```cpp
struct ValidationError { std::string key; std::string problem; };

std::vector<ValidationError> validate(const Schema& schema, const LayerValues& merged) {
    std::vector<ValidationError> errors;
    for (auto& field : schema.fields()) {
        auto it = merged.find(field.key);
        if (it == merged.end()) {
            if (field.required) errors.push_back({field.key, "missing required key"});
            continue;
        }
        auto parsed = field.try_parse(it->second); // e.g. string -> int
        if (!parsed) { errors.push_back({field.key, "wrong type"}); continue; }
        if (!field.in_range(*parsed)) errors.push_back({field.key, "value out of allowed range"});
    }
    return errors;
}
```

The atomic hot-reload publish, the project's central correctness mechanism:

```cpp
class ConfigLoader {
public:
    std::shared_ptr<const Config> current() const { return current_.load(); }

    void on_file_changed() { // invoked by the reused P-3.2 FileWatcher
        auto file_layer = read_file_layer(path_);       // fresh read, no shared state touched
        auto merged = merge_layers({defaults_, file_layer, env_layer_, cli_layer_});
        auto errors = validate(schema_, merged);
        if (!errors.empty()) {
            notify_subscribers(ConfigChangeEvent::ValidationFailed(errors));
            return; // current_ is untouched — last known-good remains active
        }
        auto new_config = std::make_shared<const Config>(schema_, merged);
        auto old_config = current_.load();
        current_.store(new_config); // single atomic pointer swap — the entire "commit"
        notify_subscribers(diff_changed_keys(*old_config, *new_config));
    }
private:
    std::atomic<std::shared_ptr<const Config>> current_;
    LayerValues defaults_, env_layer_, cli_layer_;
    std::filesystem::path path_;
    Schema schema_;
};
```

## Design Rationale

**Why represent each layer as a plain map containing only the keys it actually sets, rather than a map with `std::optional<Value>` entries for every schema key?** Key absence already means "unset" unambiguously — wrapping every value in an additional `optional` layer would just be re-expressing the same information twice, once via presence-in-the-map and once via the optional's engaged state, with no benefit and a real risk of the two disagreeing (an `optional` present but empty vs. the key missing entirely) if not handled with perfect consistency everywhere.

**Why validate the entire freshly-built configuration before ever touching `current_`, rather than validating and updating incrementally per key?** Incremental per-key updates reopen exactly the "reader observes a partially-updated configuration" problem the Constraints section forbids — a reader could see key A already updated to its new value while key B still reflects the old one. Building the complete candidate configuration off to the side, validating it as a whole, and only then atomically publishing it (never publishing an object that failed validation) guarantees every reader always sees one fully consistent snapshot, either wholly old or wholly new.

**Why `std::atomic<std::shared_ptr<const Config>>` rather than a mutex-guarded `Config` object?** A reader holding a mutex-protected reference must release the lock before working with the data for any nontrivial duration, but if it copies out individual fields while another thread is mid-reload-write, torn reads are still possible unless the mutex is held for the reader's *entire* usage window (usually impractical). Swapping an immutable, `const`-qualified shared pointer sidesteps this entirely — once a reader has loaded a `shared_ptr<const Config>`, the object it points to can never change under it; the only atomic operation is the swap of the pointer itself, and the old configuration remains valid (and safely destructible once its last reference drops) for exactly as long as any reader is still using it.

## Reference Implementation

The above covers the merge, multi-error validation, and the atomic-publish hot-reload mechanism — the project's hardest correctness requirement. Remaining work for the learner: wiring [P-3.5](../declarative-cli-parser/STATEMENT.md)'s parser output and environment-variable scanning into their respective `LayerValues` maps, `diff_changed_keys` (comparing old vs. new `Config` objects field-by-field to produce the specific-key-changed notification), and wiring [P-3.2](../file-watcher/STATEMENT.md)'s `FileWatcher` to invoke `on_file_changed()`.

## Testing Strategy

Test the merge and validation logic entirely against synthetic in-memory `LayerValues`, with zero real files, environment variables, or CLI invocations involved — this is what makes a "which layer wins" bug reproducible in a fast, deterministic unit test rather than requiring an actual multi-source integration setup to reproduce.

## Performance Analysis

Merge and validation are both O(schema field count), independent of how large any individual source's raw representation is. Hot-reload cost is dominated by re-parsing the config file, which happens only on an actual file-change notification, not on any polling cadence.

## Failure Modes

- Updating the live configuration's fields in place (even under a mutex) instead of publishing a wholly new object atomically — reintroducing the torn-read problem the atomic-pointer-swap design exists to prevent.
- Conflating "source doesn't set this key" with "source sets this key to an invalid/empty value" — collapsing two states the Functional Requirements explicitly require kept distinct.
- Publishing a new configuration before validation completes, allowing a transiently invalid configuration to become momentarily live.

## Extensions

- A structured (JSON/TOML) config-file format instead of a flat key-value format, still funneling into the same `LayerValues` representation.
- A `--print-effective-config` mode showing, for every key, which layer ultimately won — valuable for debugging real-world precedence confusion.
