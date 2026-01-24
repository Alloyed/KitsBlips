#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/params/enumParametersFeature.h>
#include <clapeze/params/parameterTypes.h>
#include <kitdsp/filters/dcBlocker.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>

#include "clapeze/baseProcessor.h"
#include "clapeze/params/parameterOnlyStateFeature.h"
#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
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
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::Algorithm> : public clapeze::EnumParam<Algorithm> {
    ParamTraits()
        : clapeze::EnumParam<Algorithm>("Algorithm", {"Clip", "Saturate", "Fold", "Rectify"}, Algorithm::HardClip) {}
};

template <>
struct ParamTraits<Params, Params::Gain> : public clapeze::DbParam {
    ParamTraits() : clapeze::DbParam("Gain", 0.0f, 32.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::Tone> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Tone", 0.5f) {}
};

template <>
struct ParamTraits<Params, Params::Makeup> : public clapeze::DbParam {
    ParamTraits() : clapeze::DbParam("Makeup", -9.0f, 9.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", 1.0f) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace distort {

namespace {

float distort(Algorithm algorithm, float in) {
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
// TODO : this was too lazy, time to find a nice shelf filter
class ToneFilter {
   public:
    explicit ToneFilter(float sampleRate) { mPole1.SetFrequency(1200.0f, sampleRate); }
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

class Processor : public EffectProcessor<ParamsFeature::ProcessorHandle> {
   public:
    explicit Processor(ParamsFeature::ProcessorHandle& params) : EffectProcessor(params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
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
            float distortedLeft = distort(algorithm, leftPre * gain);
            float distortedRight = distort(algorithm, rightPre * gain);

            // weak post tone
            float postTone = kitdsp::lerpf(0.4f, 0.6f, tonef);
            float processedLeft = dcLeft.Process(tonePostLeft.Process(distortedLeft, postTone)) * makeup;
            float processedRight = dcRight.Process(tonePostRight.Process(distortedRight, postTone)) * makeup;

            // outputs
            out.left[idx] = kitdsp::lerpf(left, processedLeft, mixf);
            out.right[idx] = kitdsp::lerpf(right, processedRight, mixf);
        }

        return ProcessStatus::Continue;
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
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRatef = static_cast<float>(sampleRate);
        tonePreLeft = ToneFilter(sampleRatef);
        tonePreRight = ToneFilter(sampleRatef);
        tonePostLeft = ToneFilter(sampleRatef);
        tonePostRight = ToneFilter(sampleRatef);
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
    void OnUpdate() override {
        kitgui::DebugParam<ParamsFeature, Params::Algorithm>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Gain>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Tone>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Makeup>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Mix>(mParams);
    }

   private:
    ParamsFeature& mParams;
};
#endif

class Plugin : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(PluginHost& host) : EffectPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Algorithm>()
                                    .Parameter<Params::Gain>()
                                    .Parameter<Params::Tone>()
                                    .Parameter<Params::Makeup>()
                                    .Parameter<Params::Mix>();
        ConfigFeature<ParameterOnlyStateFeature<ParamsFeature>>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetProcessorHandle());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.distort", "KitsDist", "Waveshaper-based distortion"));

}  // namespace distort
