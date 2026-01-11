As implied by the name, clapeze is "just" clap under the hood. here are the calls you can use to access the clap internals:

```cpp
// use to get host extensions (things the host provides us)
auto* xxx = GetHost().TryGetExtension<clap_host_XXX_t>(CLAP_EXT_XXX);

// use to register plugin extensions (or, consider implementing a Feature)
class MyPlugin : public BasePlugin {
    void Config() override {
        clap_plugin_XXX_t ext{};
        self.RegisterExtension(CLAP_EXT_XXX, static_cast<const void*>(&ext));
    }
};
```

To access `clap_process_t` and `clap_event_header_t` directly, simply inherit from baseProcessor directly instead of using the processor wrappers implemented in `clapeze::InstrumentPlugin` and `clapeze::EffectPlugin`
