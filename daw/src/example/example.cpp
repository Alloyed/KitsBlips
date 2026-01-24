#include <clapeze/baseProcessor.h>
#include <clapeze/effectPlugin.h>
#include <clapeze/params/enumParametersFeature.h>
#include <clapeze/params/parameterTypes.h>
#include <kitdsp/math/util.h>
#include "clapeze/params/parameterOnlyStateFeature.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/kitguiFeature.h"
#endif

namespace {
enum class Params : clap_id { Mix, Count };
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::params::ParamTraits<Params, Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", 1.0f) {}
};

using namespace clapeze;

namespace example {

class Processor : public EffectProcessor<ParamsFeature::ProcessorHandle> {
   public:
    explicit Processor(ParamsFeature::ProcessorHandle& params) : EffectProcessor(params) {}
    ~Processor() = default;

    ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        float mixf = mParams.Get<Params::Mix>();

        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            // in
            float left = in.left[idx];
            float right = in.right[idx];

            float processedLeft = 0.0f;
            float processedRight = 0.0f;

            // outputs
            out.left[idx] = kitdsp::lerpf(left, processedLeft, mixf);
            out.right[idx] = kitdsp::lerpf(right, processedRight, mixf);
        }
        return ProcessStatus::Continue;
    }

    void ProcessReset() override {}

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)sampleRate;
        (void)minBlockSize;
        (void)maxBlockSize;
    }

   private:
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        ImGui::TextWrapped("UI example (TODO)");
        /*mParams.DebugImGui();*/
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

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count).Parameter<Params::Mix>();
        ConfigFeature<ParameterOnlyStateFeature<ParamsFeature>>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetProcessorHandle());
    }
};

// CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.example", "example", "Plugin description"));

}  // namespace example
