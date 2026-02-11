# Parameters

Parameters and plugin parameterization is where a bulk of clapeze's weirdness comes in. In Clapeze:

- Parameters are state. For most plugins, you can just define parameters, and you will automatically get a way to serialize that plugin's state and presets.
- Parameters are declarative. You specify the bulk of how a parameter works when you declare it, using the Traits[^traits] pattern.
- Parameters are static. You cannot add or remove parameters at runtime, only hide them.

This adds up to a process where you'll need to do a lot of work up front to create a parameter, but once it exists, it's very easy and efficient to use in DSP code.

## Example

There are full examples in context in the source tree, so let's pull out the code from one of them, `effectExample.cpp`[^src].

Step 1 is to enumerate our parameters. Here, they are "Gain", "Makeup", and "Mix". (count is used to track how many parameters there actually are, and is specific to the example. You can write your enums your own way). Then, for convenience, we'll make a short name for `ParametersFeature` with that enum applied.

```cpp
enum class Params : clap_id { Gain, Makeup, Mix, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
```

Next, let's declare what each parameter does, using traits. Each trait, under the hood, declares a number of methods that apply to that parameter (to string, from string, get default, etc.). This is neatly bundled up in a few base classes that you can inherit from, or you can make or derive your own.

```cpp
template <>
struct clapeze::ParamTraits<Params, Params::Gain> : public clapeze::DbParam {
    /*parameters are key, name, min, max, default*/
    ParamTraits() : clapeze::DbParam("gain", "Gain", 0.0f, 32.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Makeup> : public clapeze::DbParam {
    /*parameters are key, name, min, max, default*/
    ParamTraits() : clapeze::DbParam("makeup", "Makeup", -9.0f, 9.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    /*parameters are key, name, default (min and max are always 0)*/
    ParamTraits() : clapeze::PercentParam("mix", "Mix", 1.0f) {}
};
```

Finally, we just need to declare an order for them in the DAW, and pass our final parameters object to the audio processor.

```cpp
    void Config() override {
        EffectPlugin::Config();

        // This is where ::Count is needed, by the way
        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Gain>()
                                    .Parameter<Params::Makeup>()
                                    .Parameter<Params::Mix>();

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
```

Then, from the audio thread:

```cpp
    void ProcessAudio(const clapeze::StereoAudioBuffer& in, clapeze::StereoAudioBuffer& out) override {
        // the DB params will return the number in dBFS
        float gain = dbToRatio(mParams.Get<Params::Gain>());
        float makeup = dbToRatio(mParams.Get<Params::Makeup>());
        // mix is a percent, so it's from 0-1
        float mixf = mParams.Get<Params::Mix>();

        // do what you want with them
    }
```

Tricky, but done! The most common parameter type is probably percent, but to get you intrigued here's a non exhaustive list[^paramConfigs]:

- NumericParam (the base for all other floating point types)
- IntegerParam (stepped)
- EnumParam (will display as a dropdown in supported DAWs)
- OnOffParam (boolean)

[^src]: [source](/clapeze/examples/effectExample.cpp)

[^traits]: https://www.cs.rpi.edu/~musser/design/blitz/traits1.html

[^paramConfigs]: [source](/clapeze/include/clapeze/ext/parameterConfigs.h)
