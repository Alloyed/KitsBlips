#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/params/enumParametersFeature.h>
#include <clapeze/params/parameterTypes.h>
#include <kitdsp/apps/chorus.h>

#include "clapeze/baseProcessor.h"
#include "clapeze/state/binaryStateFeature.h"
#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class Params : clap_id { Rate, Depth, Delay, Feedback, Mix, Count };
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::Rate> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Rate", "Rate", 0.0f, 5.0f, 1.0f, "hz") {}
};

template <>
struct ParamTraits<Params, Params::Depth> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Depth", "Depth", 0.2f) {}
};

template <>
struct ParamTraits<Params, Params::Delay> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Delay", "Delay", 2.0f, 20.0f, 8.0f, "ms") { mCurve = cPowCurve<2>; }
};

template <>
struct ParamTraits<Params, Params::Feedback> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Feedback", "Feedback", -0.95f, 0.95f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", "Mix", 0.5f) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace chorus {

class Processor : public EffectProcessor<ParamsFeature::ProcessorHandle> {
   public:
    explicit Processor(ParamsFeature::ProcessorHandle& params) : EffectProcessor(params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        mChorus->cfg.lfoRateHz = mParams.Get<Params::Rate>();
        mChorus->cfg.delayBaseMs = mParams.Get<Params::Delay>();
        mChorus->cfg.delayModMs = mChorus->cfg.delayBaseMs * mParams.Get<Params::Depth>();
        mChorus->cfg.feedback = mParams.Get<Params::Feedback>();
        mChorus->cfg.mix = mParams.Get<Params::Mix>();

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            kitdsp::float_2 processed = mChorus->Process(kitdsp::float_2(in.left[idx], in.right[idx]));

            // outputs
            out.left[idx] = processed.left;
            out.right[idx] = processed.right;
        }
        return ProcessStatus::Continue;
    }

    void ProcessReset() override { mChorus->Reset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRatef = static_cast<float>(sampleRate);
        constexpr float maxSizeSeconds = 1.0f;

        mBufLen = static_cast<size_t>(sampleRatef * maxSizeSeconds);
        mBuf.reset(new float[mBufLen]);
        mChorus = std::make_unique<kitdsp::Chorus>(etl::span(mBuf.get(), mBufLen), sampleRatef);
    }

   private:
    std::unique_ptr<float[]> mBuf;
    size_t mBufLen;
    std::unique_ptr<kitdsp::Chorus> mChorus;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        ImGui::TextWrapped(
            "KitsChorus is currently based on the Juno-60 chorus algorithm. More algorithms Soon(tm), maybe?");
        kitgui::DebugParam<ParamsFeature, Params::Rate>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Depth>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Delay>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Feedback>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Mix>(mParams);
    }

   private:
    ParamsFeature& mParams;
};
#endif

class Plugin : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(const clap_plugin_descriptor_t& meta) : EffectPlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Rate>()
                                    .Parameter<Params::Depth>()
                                    .Parameter<Params::Delay>()
                                    .Parameter<Params::Feedback>()
                                    .Parameter<Params::Mix>();
        ConfigFeature<BinaryStateFeature<ParamsFeature>>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetProcessorHandle());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.chorus", "KitsChorus", "2-voice Chorus effect."));
}  // namespace chorus
