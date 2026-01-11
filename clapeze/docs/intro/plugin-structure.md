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

## Host

The `clapeze::PluginHost` object holds methods that call back into the host application. You can access it by calling `BasePlugin::GetHost()`.

TBD: explain when this is useful
