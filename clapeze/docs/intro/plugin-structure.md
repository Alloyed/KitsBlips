# BasePlugin

`clapeze::BasePlugin` is the main class you need to implement in order to define a plugin's behavior. It has a few extension points, but by far the most important is `BasePlugin::Config()`. Here is where we define what functionality a plugin actually has, and opt-in to various bits of behavior.

## Features

"features" are the main unit of code organization in clapeze. clap itself is built on extensions, which goes into daw plugins, which (may eventually) use C++ modules, so... "feature" is the next best word :)

To use a feature in your plugin, simply add it your `Config()` method like so:

```cpp
class MyPlugin : public BasePlugin {
    void Config() override {
        ConfigFeature<StateFeature>();
        ConfigFeature<StereoAudioPortsFeature<1, 1>>();
        ConfigFeature<LatencyFeature>(5);
    }
};
```

From this call we know our plugin will store state, use 1 input stereo audio port and 1 out, and will have a latency of 5 samples.

Features are full classes, so they can be used to achieve an "object composition" style of OOP. They're stateful, and they might have useful methods on them, so to pull them out in your application code do something like this:

```cpp
    auto& myFeature = BaseFeature::GetFromPlugin<MyFeature>(pluginInstance);
    myFeature.DoSomething();
```

## Which features do I need?

Depending on where and how you plan to use your clap plugin, some features are almost mandatory, wheras others are very situational. Here are a few different tiers to start you off:

### Tier 1. MVP/Prototype

At this tier we require a way to send parameters to the DAW and serialize them. Some DAWs will fail to load your plugin if you don't provide this.

- `BaseParametersFeature` (try `EnumParametersFeature` to start)
- `BaseStateFeature` (try `TomlStateFeature` to start)

Depending on your plugin, you should also investigate:

- `NotePortsFeature` for instruments/note processors
- `StereoAudioPortsFeature` for sound makers
- `LatencyFeature` if your DSP has unavoidable latency

### Tier 3. Custom UI

UI gets its own tier, as it's the most complex feature to fulfill.

- `BaseGuiFeature` (bring your own!)
- `AssetsFeature` (use to load visual/sound assets)

### Tier 4. Niceties

These are expected of production plugins.

- `PresetFeature` to save and load presets

## Host

The `clapeze::PluginHost` object holds methods that call back into the host application. You can access it by calling `BasePlugin::GetHost()`. Because clap is extension-based, the features the host is required to provide to you is not well specified, but just as an example, you can use the PluginHost object to:

- Write to the DAW's log file (`host.Log()`)
- Request that the audio thread be restarted, in case you need to allocate more resources (`host.RequestRestart()`)

TBD: explain when this is useful
