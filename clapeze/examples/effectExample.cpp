#include <clap/ext/params.h>
#include <clap/plugin-features.h>
#include <clap/plugin.h>
#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/state/binaryStateFeature.h>
#include <clapeze/processor/baseProcessor.h>

/**
 * This is the clapeze interpretation of "AGain", a classic short example plugin.
 * This is a 100% self-contained file. To create a full plugin you'll need this and an entry point.
 * See `examples/entry.cpp` for an example of that.
 */
namespace effectExample {
/* Here are all the parameters we support. */
enum class MyParams : clap_id {
    Gain,
    VuPPM,
    Bypass,
    // count just tracks the number of params there are for later; it's not an actual param.
    Count
};
/*
 * clapeze::ParametersFeature<> implements the nuts and bolts of parameters, including persistence and audio<->main
 * thread communication. Specializing on our Params enum tells it what parameters exist and how they behave.
 */
// let's make some aliases to save some typing
using MyParamsFeature = clapeze::params::EnumParametersFeature<MyParams>;
using MyParamsHandle = MyParamsFeature::ProcessorHandle;
}  // namespace effectExample

/**
 * To define the behavior of each parameter at compile time we use the Traits Pattern.
 * more info: https://www.cs.rpi.edu/~musser/design/blitz/traits1.html
 * Any parameter we try to use but forget to specialize will result in a compile error.
 */
namespace clapeze::params {
using namespace effectExample;
// a ParamTraits implementation should inherit from clapeze::BaseParam. they can do anything, but there are a number of
// convenient ones built-in. see `clapeze/features/parameterConfigs.h` for more.
template <>
struct ParamTraits<MyParams, MyParams::Gain> : public DbParam {
    //                       key    name    min    max    default
    ParamTraits() : DbParam("gain", "Gain", -60.0f, 30.0f, 0.0f) {}
};

template <>
struct ParamTraits<MyParams, MyParams::VuPPM> : public NumericParam {
    //                           key      name    min   max   default
    ParamTraits() : NumericParam("vuppm", "Peak", 0.0f, 1.0f, 0.0f) { mFlags |= CLAP_PARAM_IS_READONLY; }
};

template <>
struct ParamTraits<MyParams, MyParams::Bypass> : public OnOffParam {
    //                         key       name      default
    ParamTraits() : OnOffParam("bypass", "Bypass", OnOff::Off) { mFlags |= CLAP_PARAM_IS_BYPASS; }
};
static_assert(static_cast<clap_id>(MyParams::Count) == 3, "update parameter traits");
}  // namespace clapeze::params

/*
 * Ok, now that our parameters are defined, let's write our plugin! you may prefer to put the parameters in their own
 * header to refer to across modules.
 */
namespace effectExample {
/*
 * Processor holds the code that runs on the audio thread.
 * EffectProcessor provides a shortcut for simple 2-channel stereo effects with parameters
 */
class MyProcessor : public clapeze::EffectProcessor<MyParamsHandle> {
    using BaseType = clapeze::EffectProcessor<MyParamsHandle>;

   public:
    explicit MyProcessor(MyParamsHandle& params) : BaseType(params) {}
    ~MyProcessor() = default;

    clapeze::ProcessStatus ProcessAudio(const clapeze::StereoAudioBuffer& in,
                                        clapeze::StereoAudioBuffer& out) override {
        // each parameter is sample accurate, because ProcessAudio is split into smaller chunks in between each host
        // event.
        float gainDb = mParams.Get<MyParams::Gain>();
        float gainRatio = std::pow(10.0f, gainDb / 20.0f);
        bool shouldBypass = mParams.Get<MyParams::Bypass>() == clapeze::OnOff::On;
        if (shouldBypass) {
            out.CopyFrom(in);
            return clapeze::ProcessStatus::Continue;
        }

        float peakValue = mLastPeakValue;
        if (gainDb <= -60.01f) {
            // we'll treat gain values at the very bottom as if they are totally silent.
            out.Fill(0.0f);
        } else {
            for (size_t idx = 0; idx < in.left.size(); ++idx) {
                out.left[idx] = in.left[idx] * gainRatio;
                out.right[idx] = in.right[idx] * gainRatio;
                peakValue = std::max(std::max(peakValue, out.left[idx]), out.right[idx]);
            }
            // hint that the output is constant, if the input was. in many cases you can just skip setting this flag, it
            // defaults to false.
            out.isLeftConstant = in.isLeftConstant;
            out.isRightConstant = in.isRightConstant;
        }

        if (peakValue != mLastPeakValue) {
            mParams.Send<MyParams::VuPPM>(peakValue);
            mLastPeakValue = peakValue;
        }

        return clapeze::ProcessStatus::Continue;
    }

    // called to reset internal state.
    void ProcessReset() override { mLastPeakValue = 0.0f; }

    // called on main thread before starting up the audio thread. When you need it, this is the main place it's safe to
    // allocate memory
    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {}

   private:
    float mLastPeakValue = 0.0f;
};

/**
 * Plugin represents the overall state of the plugin.
 * EffectPlugin is the paired plugin to the EffectProcessor, so likewise, you can drop down to the BasePlugin whenever.
 */
class MyPlugin : public clapeze::EffectPlugin {
   public:
    explicit MyPlugin(const clap_plugin_descriptor_t& meta) : EffectPlugin(meta) {}
    ~MyPlugin() = default;

   protected:
    /**
     * Config is where we tell the host (and clapeze) what features we'd like to opt into.
     * it runs once per plugin on init.
     */
    void Config() override {
        EffectPlugin::Config();  // pre-configured as a stereo effect

        // Here we configure our parameters. if you have special requirements, you can create your own implementation of
        // ParametersFeature, and config it here.
        MyParamsFeature& params = ConfigFeature<MyParamsFeature>(GetHost(), MyParams::Count)
                                      .Parameter<MyParams::Gain>()
                                      .Parameter<MyParams::VuPPM>()
                                      .Parameter<MyParams::Bypass>();
        static_assert(static_cast<clap_id>(MyParams::Count) == 3, "update parameter order");
        ConfigFeature<clapeze::BinaryStateFeature<MyParamsFeature>>(*this);

        // we are opting into audio processing using the Processor object defined before, and using the params object as
        // our communication channel.
        ConfigProcessor<MyProcessor>(params.GetProcessorHandle<MyParamsFeature::ProcessorHandle>());
    }
};

// descriptors are exactly as in vanilla clap for better or worse
constexpr const char* kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_UTILITY,
                                     CLAP_PLUGIN_FEATURE_STEREO, nullptr};

constexpr clap_plugin_descriptor_t kDescriptor = {
    .clap_version = CLAP_VERSION,
    .id = "clapeze.example.again",
    .name = "AGain",
    .vendor = "Clapeze",
    .url = nullptr,
    .manual_url = nullptr,
    .support_url = nullptr,
    .version = "0.0.0",
    .description = "Simple gain enhancing/reduction plugin",
    .features = kFeatures,
};

// this macro does magic under the hood to tell clapeze that this plugin exists and should be added to the DAW's list of
// plugins.
CLAPEZE_REGISTER_PLUGIN(MyPlugin, kDescriptor);

}  // namespace effectExample
