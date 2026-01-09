#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include <kitdsp/frequencyShifter.h>
#include <kitdsp/math/util.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class Params : clap_id { Shift, Mix, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::Shift> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Shift", cPowBipolarCurve<2>, -1000, 1000, 0, "hz") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", 1.0f) {}
};

using namespace clapeze;

namespace freqshift {

class Processor : public EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        float shift = mParams.Get<Params::Shift>();
        float mixf = mParams.Get<Params::Mix>();

        mLeft->SetFrequencyOffset(shift, mSampleRate);
        mRight->SetFrequencyOffset(shift, mSampleRate);

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
        (void)minBlockSize;
        (void)maxBlockSize;
        mSampleRate = static_cast<float>(sampleRate);
        mLeft = std::make_unique<kitdsp::FrequencyShifter>(mSampleRate);
        mRight = std::make_unique<kitdsp::FrequencyShifter>(mSampleRate);
    }

   private:
    float mSampleRate;
    std::unique_ptr<kitdsp::FrequencyShifter> mLeft;
    std::unique_ptr<kitdsp::FrequencyShifter> mRight;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        ImGui::TextWrapped(
            "KitFreqsOut shifts the frequencies of the input sound by a fixed hz amount. this is most useful for "
            "inharmonic sounds; aka sound effects, percussion, etc. Use it on a voice clip to get alien sounds, or "
            "combine with delays and delay feedback for wacky results!");
        kitgui::DebugParam<ParamsFeature, Params::Shift>(mParams);
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

        ParamsFeature& params =
            ConfigFeature<ParamsFeature>(GetHost(), Params::Count).Parameter<Params::Shift>().Parameter<Params::Mix>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

CLAPEZE_REGISTER_PLUGIN(
    Plugin,
    AudioEffectDescriptor("kitsblips.freqshift",
                          "KitFreqsOut",
                          "Bode-style frequency shifter. More useful for sound effects than for pitched instruments."));
}  // namespace freqshift
