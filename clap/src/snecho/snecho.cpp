#include "snecho/snecho.h"

#include "clapeze/effectPlugin.h"
#include "clapeze/ext/parameterConfigs.h"
#include "clapeze/ext/parameters.h"
#include "descriptor.h"

#include <etl/memory.h>
#include <kitdsp/apps/snesEcho.h>
#include <kitdsp/apps/snesEchoFilterPresets.h>
#include <kitdsp/dbMeter.h>
#include <kitdsp/samplerate/resampler.h>

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include <kitgui/context.h>
#include "gui/debugui.h"
#include "gui/feature.h"
#endif

namespace {
enum class Params : clap_id {
    Mix,
    Size,
    Feedback,
    FilterPreset,
    FreezeEcho,
    ResetHead,
    SizeRange,
    EchoDelayMod,
    FilterMix,
    ClearBuffer,
    Bypass,
    StereoMode,
    Count
};
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::Bypass> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("Bypass", OnOff::Off) {}
};

template <>
struct clapeze::ParamTraits<Params::Size> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Size", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::Feedback> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Feedback", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::FilterPreset> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Filter Preset", 0, kitdsp::SNES::kNumFilterPresets, 0) {}
};

template <>
struct clapeze::ParamTraits<Params::FreezeEcho> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("Freeze Echo", OnOff::Off) {}
};

template <>
struct clapeze::ParamTraits<Params::ResetHead> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("Reset Playhead", OnOff::Off) {}
};

template <>
struct clapeze::ParamTraits<Params::SizeRange> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Size Range", 0, 2, 0) {}
};

template <>
struct clapeze::ParamTraits<Params::EchoDelayMod> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Echo Mod", 1.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::FilterMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Filter Mix", 1.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::ClearBuffer> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("Clear Buffer", OnOff::Off) {}
};

template <>
struct clapeze::ParamTraits<Params::StereoMode> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("Stereo Mode", OnOff::Off) {}
};

using namespace kitdsp;
using namespace clapeze;

namespace snecho {
class Processor : public EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        bool bypass = mParams.Get<Params::Bypass>() == OnOff::On;
        bool stereoMode = mParams.Get<Params::StereoMode>() == OnOff::On;
        if (bypass) {
            etl::mem_copy(in.left.begin(), in.left.end(), out.left.data());
            etl::mem_copy(in.right.begin(), in.right.end(), out.right.data());
        } else if (stereoMode) {
            mLeft.ProcessAudio(mParams, in.left, out.left);
            mRight.ProcessAudio(mParams, in.right, out.right);
        } else {
            mLeft.ProcessAudio(mParams, in.left, out.left);
            etl::mem_copy(out.left.begin(), out.left.end(), out.right.data());
        }
    }

    void ProcessReset() override {
        mLeft.ProcessReset();
        mRight.ProcessReset();
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        mLeft.Activate(sampleRate);
        mRight.Activate(sampleRate);
    }

   private:
    struct Channel {
        void ProcessAudio(ParamsFeature::ProcessParameters& mParams,
                          const etl::span<float>& in,
                          etl::span<float>& out) {
            // inputs
            // core
            snes1.cfg.echoBufferSize = mParams.Get<Params::Size>();

            snes1.cfg.echoFeedback = mParams.Get<Params::Feedback>();

            size_t filterPreset = mParams.Get<Params::FilterPreset>();
            if (filterPreset != mLastFilterPreset) {
                mLastFilterPreset = filterPreset;
                memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
                // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
            }

            int32_t range = mParams.Get<Params::SizeRange>();
            if (range == 0) {
                snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
            } else if (range == 1) {
                snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
            } else {
                snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
            }

            float wetDryMix = mParams.Get<Params::Mix>();

            snes1.cfg.freezeEcho = mParams.Get<Params::FreezeEcho>() == OnOff::On;

            // extension
            snes1.cfg.echoDelayMod = mParams.Get<Params::EchoDelayMod>();

            snes1.cfg.filterMix = mParams.Get<Params::FilterMix>();

            snes1.mod.clearBuffer = mParams.Get<Params::ClearBuffer>() == OnOff::On;
            snes1.mod.resetHead = mParams.Get<Params::ResetHead>() == OnOff::On;
            snes1.cfg.echoBufferIncrementSamples = SNES::kOriginalEchoIncrementSamples;

            // processing
            for (size_t idx = 0; idx < in.size(); ++idx) {
                float drySignal = in[idx];
                float wetSignal = snesSampler.Process<kitdsp::interpolate::InterpolationStrategy::None>(
                    drySignal, [this](float in, float& out) { out = snes1.Process(in * 0.5f) * 2.0f; });

                // outputs
                out[idx] = lerpf(drySignal, wetSignal, wetDryMix);
            }
        }

        void ProcessReset() { snes1.Reset(); }

        void Activate(double sampleRate) { snesSampler = {SNES::kOriginalSampleRate, static_cast<float>(sampleRate)}; }

        static constexpr size_t snesBufferSize = 7680UL * 10000;
        int16_t snesBuffer1[snesBufferSize];
        kitdsp::SNES::Echo snes1{snesBuffer1, snesBufferSize};
        kitdsp::Resampler<float> snesSampler{kitdsp::SNES::kOriginalSampleRate, 41000};
        size_t mLastFilterPreset;
    };

    Channel mLeft;
    Channel mRight;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        ImGui::TextWrapped(
            "With Snecho, I have created an emulation of the echo effect found in the SPC700 chip, used in the "
            "SNES/Super Famicom consoles."
            "It's a pretty normal, if crunchy, delay, but weird effects can be achieved by automating the 'Size' "
            "Parameter.");

        kitgui::DebugParam<ParamsFeature, Params::Mix>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Size>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Feedback>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::FilterPreset>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::FreezeEcho>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::ResetHead>(mParams);

        // extensions
        ImGui::TextWrapped("The following parameters would not be available on a real SNES. Consider them bonuses!");
        kitgui::DebugParam<ParamsFeature, Params::Bypass>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::SizeRange>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EchoDelayMod>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::FilterMix>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::ClearBuffer>(mParams);
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
                                    .Module("Original")
                                    .Parameter<Params::Mix>()
                                    .Parameter<Params::Size>()
                                    .Parameter<Params::Feedback>()
                                    .Parameter<Params::FilterPreset>()
                                    .Parameter<Params::FreezeEcho>()
                                    .Parameter<Params::ResetHead>()
                                    .Module("Extensions")
                                    .Parameter<Params::Bypass>()
                                    .Parameter<Params::StereoMode>()
                                    .Parameter<Params::SizeRange>()
                                    .Parameter<Params::EchoDelayMod>()
                                    .Parameter<Params::FilterMix>()
                                    .Parameter<Params::ClearBuffer>();

#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>([&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif
        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioEffectDescriptor("kitsblips.snecho", "Snecho", "A SNES-inspired mono delay effect"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace snecho
