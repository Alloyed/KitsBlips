#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include "clapeze/baseProcessor.h"
#include "descriptor.h"

/*
 * Each parameter needs an instatiation of clapeze::ParamTraits to define its behavior.
 * They also need a unique identifier. here we are using a C++ enum for that.
 */
namespace {
// count just tracks the number of params there are for later
enum class Params : clap_id { Gain, Makeup, Mix, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::Gain> : public clapeze::DbParam {
    /*
     * Each constructor is unique to the base param type, for clapeze::DbParam:
     *                               name    min   max    default
     */
    ParamTraits() : clapeze::DbParam("Gain", 0.0f, 32.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Makeup> : public clapeze::DbParam {
    ParamTraits() : clapeze::DbParam("Makeup", -9.0f, 9.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    /*                                    name   default */
    ParamTraits() : clapeze::PercentParam("Mix", 1.0f) {}
};
static_assert(static_cast<clap_id>(Params::Count) == 3, "update parameter traits");

namespace crunch {
inline float dbToRatio(float db) {
    return std::pow(10.0f, db / 20.0f);
}

/*
 * Processor holds the code that runs on the audio thread. EffectProcessor provides a shortcut for simple stereo
 * effects.
 */
class Processor : public clapeze::EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(const clapeze::StereoAudioBuffer& in,
                                        clapeze::StereoAudioBuffer& out) override {
        // each parameter is sample accurate, because ProcessAudio is split into smaller chunks in between each host
        // event.
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
        return clapeze::ProcessStatus::Continue;
    }

    // called to reset internal state. we don't have any, so it's unnecessary here
    void ProcessReset() override {}

    // called on main thread before starting up the audio thread. this is the main place it's safe to allocate memory
    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {}

   private:
    // no state here!
};

// Plugin represents the overall state of the plugin
class Plugin : public clapeze::EffectPlugin {
   public:
    explicit Plugin(clapeze::PluginHost& host) : EffectPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        // Config is where we tell the host (and clapeze) what features we'd like to opt into.
        // it runs once per plugin on init.
        EffectPlugin::Config();

        // we are opting into parameters
        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Module("Main")
                                    .Parameter<Params::Gain>()
                                    .Parameter<Params::Makeup>()
                                    .Parameter<Params::Mix>();
        static_assert(static_cast<clap_id>(Params::Count) == 3, "update parameter order");

        // we are opting into audio processing using the Processor object defined before
        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

// this macro does magic under the hood to tell clapeze that this plugin exists and should be added to the DAW's list of
// plugins.
CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioEffectDescriptor("clapeze.effectexample.distort",
                                              "Example Distortion",
                                              "Simple tanh distortion."));

}  // namespace crunch
