#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <kitdsp/harmonizer.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>

#include <clapeze/features/state/binaryStateFeature.h>
#include <clapeze/processor/baseProcessor.h>
#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class Params : clap_id { Transpose, Finetune, BaseDelay, GrainSize, Feedback, Mix, Count };
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::Transpose> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Transpose", "Transpose", -12, 12, 12, "semis") {}
};

template <>
struct ParamTraits<Params, Params::Finetune> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Finetune", "Fine Tune", -100, 100, 0, "cents") {}
};

template <>
struct ParamTraits<Params, Params::BaseDelay> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("BaseDelay", "Base Delay", 0.0f, 1000.0f, 0.0f, "ms") {
        mCurve = cPowCurve<2.0f>;
    }
};

template <>
struct ParamTraits<Params, Params::GrainSize> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("GrainSize", "Grain Size", 10.0f, 100.0f, 30.0f, "ms") {}
};

template <>
struct ParamTraits<Params, Params::Feedback> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Feedback", "Feedback", 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", "Mix", 1.0f) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace harmonizer {

class Processor : public EffectProcessor<ParamsFeature::ProcessorHandle> {
   public:
    explicit Processor(ParamsFeature::ProcessorHandle& params) : EffectProcessor(params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        float transpose = static_cast<float>(mParams.Get<Params::Transpose>());
        float fine = mParams.Get<Params::Finetune>();
        float delayMs = mParams.Get<Params::BaseDelay>();
        float grainSizeMs = mParams.Get<Params::GrainSize>();
        float feedback = std::exp2f(0.5f * std::log2f(mParams.Get<Params::Feedback>()));
        float mixf = mParams.Get<Params::Mix>();

        float shiftRatio = kitdsp::midiToRatio(transpose + (fine * 0.01f));

        mLeft->SetParams(shiftRatio, grainSizeMs, delayMs, feedback);
        mRight->SetParams(shiftRatio, grainSizeMs, delayMs, feedback);

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            float left = in.left[idx];
            float processedLeft = mLeft->Process(left);
            out.left[idx] = kitdsp::lerp(left, processedLeft, mixf);
        }

        for (size_t idx = 0; idx < in.right.size(); ++idx) {
            float right = in.right[idx];
            float processedRight = mRight->Process(right);
            out.right[idx] = kitdsp::lerp(right, processedRight, mixf);
        }
        return ProcessStatus::Continue;
    }

    void ProcessReset() override {
        mLeft->Reset();
        mRight->Reset();
    }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)sampleRate;
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRatef = static_cast<float>(sampleRate);
        constexpr float maxSizeSeconds = 1.0f;

        mBufLen = static_cast<size_t>(sampleRatef * maxSizeSeconds);
        mBufLeft.reset(new float[mBufLen]);
        mLeft = std::make_unique<kitdsp::Harmonizer>(etl::span(mBufLeft.get(), mBufLen), sampleRatef);
        mBufRight.reset(new float[mBufLen]);
        mRight = std::make_unique<kitdsp::Harmonizer>(etl::span(mBufRight.get(), mBufLen), sampleRatef);
    }

   private:
    // TODO: create resizable span class that doesn't re-alloc in between
    std::unique_ptr<float[]> mBufLeft;
    std::unique_ptr<float[]> mBufRight;
    size_t mBufLen;
    std::unique_ptr<kitdsp::Harmonizer> mLeft;
    std::unique_ptr<kitdsp::Harmonizer> mRight;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        ImGui::TextWrapped(
            "This is a pitch shift effect inspired by the EvenTide H910, the very first realtime pitch shift hardware "
            "module. This, of course, means the way it achieves this effect is kind of nasty, and not very "
            "transparent. it's fun when layered with the original signal, or used as a creative effect, though!");
        kitgui::DebugParam<ParamsFeature, Params::Transpose>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Finetune>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::BaseDelay>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::GrainSize>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Feedback>(mParams);
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
                                    .Parameter<Params::Transpose>()
                                    .Parameter<Params::Finetune>()
                                    .Parameter<Params::BaseDelay>()
                                    .Parameter<Params::GrainSize>()
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

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioEffectDescriptor("kitsblips.harmonizer",
                                              "KitsHarmony",
                                              "Eventide H910-inspired Harmonizer effect"));
}  // namespace harmonizer
