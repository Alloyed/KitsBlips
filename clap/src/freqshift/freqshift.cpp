#include "freqshift/freqshift.h"

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
#include <imgui.h>
#include <kitgui/app.h>
#include <kitgui/context.h>
#endif

namespace {
enum class Params : clap_id { Mix, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params::Mix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mix", 1.0f) {}
};

using namespace clapeze;

namespace freqshift {

class Processor : public EffectProcessor<ParamsFeature::ProcessParameters> {
   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
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
    }

    void ProcessReset() override {
    }

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
        ImGui::Text("Helooooo");
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

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Mix>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>([&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioEffectDescriptor("kitsblips.freqshift", "freqshift 2!", "Waveshaping-based distortion"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace freqshift
