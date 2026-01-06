#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include <kitdsp/harmonizer.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <gui/feature.h>
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#endif

namespace {
enum class Params : clap_id { Transpose, Finetune, BaseDelay, GrainSize, Feedback, Mix, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::Transpose> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Transpose", -12, 12, 12, "semis") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Finetune> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Fine Tune", -100, 100, 0, "cents") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::BaseDelay> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Base Delay", 0.0f, 1000.0f, 0.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::GrainSize> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Grain Size", 10, 100, 30, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Feedback> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Feedback", 0.0f, 0.95f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", 1.0f) {}
};

using namespace clapeze;

namespace harmonizer {

class Processor : public EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        float transpose = static_cast<float>(mParams.Get<Params::Transpose>());
        float fine = mParams.Get<Params::Finetune>();
        float delayMs = mParams.Get<Params::BaseDelay>();
        float grainSizeMs = mParams.Get<Params::GrainSize>();
        float feedback = mParams.Get<Params::Feedback>();
        float mixf = mParams.Get<Params::Mix>();

        float shiftRatio = kitdsp::midiToRatio(transpose + (fine * 0.01f));

        mLeft->SetParams(shiftRatio, grainSizeMs, delayMs, feedback);
        mRight->SetParams(shiftRatio, grainSizeMs, delayMs, feedback);

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            float left = in.left[idx];
            float processedLeft = mLeft->Process(left);
            out.left[idx] = kitdsp::lerpf(left, processedLeft, mixf);
        }

        for (size_t idx = 0; idx < in.right.size(); ++idx) {
            float right = in.right[idx];
            float processedRight = mRight->Process(right);
            out.right[idx] = kitdsp::lerpf(right, processedRight, mixf);
        }
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
    explicit Plugin(PluginHost& host) : EffectPlugin(host) {}
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
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioEffectDescriptor("kitsblips.harmonizer",
                                              "KitsHarmony",
                                              "Eventide H910-inspired Harmonizer effect"));
}  // namespace harmonizer
