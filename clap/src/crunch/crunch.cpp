#include "crunch/crunch.h"

#include <clapeze/effectPlugin.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include <kitdsp/dbMeter.h>
#include <kitdsp/filters/dcBlocker.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/math/approx.h>
#include <kitdsp/math/util.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/ext/kitgui.h>
#include <kitgui/app.h>
#include <kitgui/context.h>
#endif

namespace {
enum class Params : clap_id { Algorithm, Gain, Tone, Makeup, Mix, Count };
enum class Algorithm {
    // base algorithms (similar to renoise's built-in distortion)
    HardClip,
    Tanh,
    Fold,
    Rectify,
    // not an algorithm :)
    Count
};
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::Algorithm> {
    using _paramtype = clapeze::EnumParam<Algorithm>;
};

template <>
struct clapeze::ParamTraits<Params, Params::Gain> {
    using _paramtype = clapeze::DbParam;
};

template <>
struct clapeze::ParamTraits<Params, Params::Tone> {
    using _paramtype = clapeze::PercentParam;
};

template <>
struct clapeze::ParamTraits<Params, Params::Makeup> {
    using _paramtype = clapeze::DbParam;
};

template <>
struct clapeze::ParamTraits<Params, Params::Mix> {
    using _paramtype = clapeze::PercentParam;
};

using namespace clapeze;

namespace crunch {

namespace {

float crunch(Algorithm algorithm, float in) {
    switch (algorithm) {
        case Algorithm::HardClip: {
            return kitdsp::clamp(in, -1.0f, 1.0f);
        }
        case Algorithm::Tanh: {
            return tanhf(in);
        }
        case Algorithm::Fold: {
            return sinf(in * kitdsp::kPi);
        }
        case Algorithm::Rectify: {
            return fabsf(in) * 2.0f - 1.0f;
        }
        case Algorithm::Count: {
            return in;
        }
    }
    return in;
}

// lazy tone control
// TODO: this was too lazy, time to find a nice shelf filter
class ToneFilter {
   public:
    ToneFilter(float sampleRate) { mPole1.SetFrequency(1200.0f, sampleRate); }
    float Process(float in, float tone) {
        float lowpass = mPole1.Process(in);
        float highpass = in - lowpass;

        // needs to be 2 at 0.5 and 1 at 0, 1, in between is a matter of taste
        float gain = sinf(tone * kitdsp::kPi) + 1.0f;

        return kitdsp::lerpf(lowpass, highpass, tone) * gain;
    }
    void Reset() { mPole1.Reset(); }
    kitdsp::OnePole mPole1;
};
}  // namespace

class Processor : public EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        Algorithm algorithm = mParams.Get<Params::Algorithm>();
        float gain = kitdsp::dbToRatio(mParams.Get<Params::Gain>());
        float tonef = mParams.Get<Params::Tone>();
        float makeup = kitdsp::dbToRatio(mParams.Get<Params::Makeup>());
        float mixf = mParams.Get<Params::Mix>();

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            // in
            float left = in.left[idx];
            float right = in.right[idx];

            // strong pre tone
            float preTone = kitdsp::lerpf(0.1f, 0.9f, tonef);
            float leftPre = tonePreLeft.Process(left, preTone);
            float rightPre = tonePreRight.Process(right, preTone);

            // TODO: different gain curves for different algorithms?
            float crunchedLeft = crunch(algorithm, leftPre * gain);
            float crunchedRight = crunch(algorithm, rightPre * gain);

            // weak post tone
            float postTone = kitdsp::lerpf(0.4f, 0.6f, tonef);
            float processedLeft = dcLeft.Process(tonePostLeft.Process(crunchedLeft, postTone)) * makeup;
            float processedRight = dcRight.Process(tonePostRight.Process(crunchedRight, postTone)) * makeup;

            // outputs
            out.left[idx] = kitdsp::lerpf(left, processedLeft, mixf);
            out.right[idx] = kitdsp::lerpf(right, processedRight, mixf);
        }
    }

    void ProcessReset() override {
        dcLeft.Reset();
        dcRight.Reset();
        tonePreLeft.Reset();
        tonePreRight.Reset();
        tonePostLeft.Reset();
        tonePostRight.Reset();
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        float sampleRatef = static_cast<float>(sampleRate);
        tonePreLeft = {sampleRatef};
        tonePreRight = {sampleRatef};
        tonePostLeft = {sampleRatef};
        tonePostRight = {sampleRatef};
    }

   private:
    static constexpr float cSampleRate = 96000.0f;
    kitdsp::DcBlocker dcLeft;
    kitdsp::DcBlocker dcRight;
    ToneFilter tonePreLeft{cSampleRate};
    ToneFilter tonePreRight{cSampleRate};
    ToneFilter tonePostLeft{cSampleRate};
    ToneFilter tonePostRight{cSampleRate};
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override { mParams.DebugImGui(); }

   private:
    ParamsFeature& mParams;
};
#endif

class Plugin : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    Plugin(PluginHost& host) : EffectPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::Config();

        ParamsFeature& params =
            ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                .ConfigParam<Params::Algorithm>("Algorithm",
                                                std::vector<std::string_view>{"Clip", "Saturate", "Fold", "Rectify"},
                                                Algorithm::HardClip)
                .ConfigParam<Params::Gain>("Gain", 0.0f, 32.0f, 0.0f)
                .ConfigParam<Params::Tone>("Tone", 0.5f)
                .ConfigParam<Params::Makeup>("Makeup", -9.0f, 9.0f, 0.0f)
                .ConfigParam<Params::Mix>("Mix", 1.0f);
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>([&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioEffectDescriptor("kitsblips.crunch", "Crunch", "A SNES-inspired mono delay effect"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace crunch
