#include "crunch/crunch.h"

#include <clapeze/effectPlugin.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include "descriptor.h"

namespace {
enum class Params : clap_id { Gain, Makeup, Mix, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::Gain> {
    using _paramtype = clapeze::DbParam;
};

template <>
struct clapeze::ParamTraits<Params, Params::Makeup> {
    using _paramtype = clapeze::DbParam;
};

template <>
struct clapeze::ParamTraits<Params, Params::Mix> {
    using _paramtype = clapeze::PercentParam;
};

namespace crunch {
inline float dbToRatio(float db) {
    return std::pow(10, db / 20.0f);
}

class Processor : public clapeze::EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const clapeze::StereoAudioBuffer& in, clapeze::StereoAudioBuffer& out) override {
        float gain = dbToRatio(mParams.Get<Params::Gain>());
        float makeup = dbToRatio(mParams.Get<Params::Makeup>());
        float mixf = mParams.Get<Params::Mix>();

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            // in
            float left = in.left[idx];
            float right = in.right[idx];

            float crunchedLeft = std::tanh(left * gain) * makeup;
            float crunchedRight = std::tanh(right * gain) * makeup;

            // outputs
            out.left[idx] = std::lerp(left, crunchedLeft, mixf);
            out.right[idx] = std::lerp(right, crunchedRight, mixf);
        }
    }

    void ProcessReset() override {}

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {}

   private:
};

class Plugin : public clapeze::EffectPlugin {
   public:
    static const clapeze::PluginEntry Entry;
    Plugin(clapeze::PluginHost& host) : EffectPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .ConfigParam<Params::Gain>("Gain", 0.0f, 32.0f, 0.0f)
                                    .ConfigParam<Params::Makeup>("Makeup", -9.0f, 9.0f, 0.0f)
                                    .ConfigParam<Params::Mix>("Mix", 1.0f);

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const clapeze::PluginEntry Entry{
    AudioEffectDescriptor("kitsblips.crunch", "Crunch", "A SNES-inspired mono delay effect"),
    [](clapeze::PluginHost& host) -> clapeze::BasePlugin* { return new Plugin(host); }};

}  // namespace crunch
