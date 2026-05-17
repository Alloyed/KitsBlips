#include <clap/plugin.h>
#include <clapeze/effectPlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/state/tomlStateFeature.h>
#include <clapeze/processor/baseProcessor.h>
#include <kitdsp/apps/disperser.h>
#include <kitdsp/math/util.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class Params : clap_id { Frequency, Feedback, Iterations, Count };
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::Frequency> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Frequency", "Frequency", 0.100f, 15000.0f, 440.0f) {}
};
template <>
struct ParamTraits<Params, Params::Feedback> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Feedback", "Feedback", 0.0f) {}
};
template <>
struct ParamTraits<Params, Params::Iterations> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Iterations", "Iterations", 1, static_cast<int32_t>(kitdsp::Disperser::GetMaxFilters()), 1) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace disperser {

class Processor : public EffectProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(ParamsFeature::AudioHandle& params) : EffectProcessor(params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        if (in.left.data() != out.left.data()) {
            // not inplace, time to fake it
            out.CopyFrom(in);
        }
        float sampleRate = narrow_cast<float>(GetSampleRate());
        float freq = mParams.Get<Params::Frequency>();
        float feedback = mParams.Get<Params::Feedback>();
        int32_t iterations = mParams.Get<Params::Iterations>();

        mLeft->SetParams(freq, sampleRate, feedback, iterations);
        mLeft->ProcessInPlace(out.left);

        mRight->SetParams(freq, sampleRate, feedback, iterations);
        mRight->ProcessInPlace(out.right);

        return ProcessStatus::ContinueIfNotQuiet;
    }

    void ProcessReset() override {}

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)sampleRate;
        (void)minBlockSize;
        (void)maxBlockSize;

        float sampleRatef = static_cast<float>(sampleRate);
        constexpr float maxSizeSeconds = .02f;

        mBufLen = static_cast<size_t>(sampleRatef * maxSizeSeconds * kitdsp::Disperser::GetMaxFilters());
        mBufLeft.reset(new float[mBufLen]);
        mLeft = std::make_unique<kitdsp::Disperser>(etl::span(mBufLeft.get(), mBufLen));
        mBufRight.reset(new float[mBufLen]);
        mRight = std::make_unique<kitdsp::Disperser>(etl::span(mBufRight.get(), mBufLen));
    }

   private:
    // TODO: create resizable span class that doesn't re-alloc in between
    std::unique_ptr<float[]> mBufLeft;
    std::unique_ptr<float[]> mBufRight;
    size_t mBufLen;
    std::unique_ptr<kitdsp::Disperser> mLeft;
    std::unique_ptr<kitdsp::Disperser> mRight;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        ImGui::TextWrapped("idk man");
        kitgui::DebugParam<ParamsFeature, Params::Frequency>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Feedback>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::Iterations>(mParams);
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
        EffectPlugin::BaseConfig(true);

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Frequency>()
                                    .Parameter<Params::Feedback>()
                                    .Parameter<Params::Iterations>();
        ConfigFeature<TomlStateFeature<ParamsFeature>>(*this);

#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<clapeze::AssetsFeature>();
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioEffectDescriptor("kitsblips.disperser",
                                              "KitDisperses",
                                              "Dispersal effect based on stacked allpass filters"));

}  // namespace disperser
