#include <clap/ext/params.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/presetFeature.h>
#include <clapeze/features/state/tomlStateFeature.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/processor/voice.h>
#include <etl/flat_multimap.h>
#include <etl/vector.h>
#include <kitdsp/control/adsr.h>
#include <kitdsp/control/gate.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>
#include <kitdsp/osc/blepOscillator.h>
#include <kitgui/context.h>
#include <cfloat>
#include <memory>
#include <sstream>

#include "clapeze/basePlugin.h"
#include "clapeze/common.h"
#include "clapeze/features/state/baseStateFeature.h"
#include "clapeze/impl/stringUtils.h"
#include "clapeze/processor/transport.h"
#include "descriptor.h"
#include "gui/parameterControls.h"
#include "gui/presetBrowser.h"
#include "kitdsp/control/approach.h"

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/features/assetsFeature.h>
#include <imgui.h>
#include <kitgui/app.h>
#include <kitgui/gfx/scene.h>
#include "gui/kitguiFeature.h"
#endif

#if KITSBLIPS_ENABLE_SENTRY
#include <sentry.h>
#endif

namespace {
enum class Params : clap_id {
    Count
};

constexpr size_t cMaxVoices = 16;
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
}  // namespace clapeze::params

using namespace clapeze;

namespace layersynth {

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::AudioHandle> {
    class Voice {
       public:
        explicit Voice(Processor& p) : mProcessor(p) {}
        void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
            (void)velocity;
            mNote = note.key;
        }
        void ProcessNoteOff() {
        }
        void ProcessChoke() {
        }
        void Reset() {
        }
        bool ProcessAudio(clapeze::StereoAudioBuffer& out) {
            const float sampleRate = static_cast<float>(mProcessor.GetSampleRate());

            //out.left[index] += vcaOut;
            //out.right[index] += vcaOut;

            // if no longer processing, time to sleep
            return false;
        }

       private:
        Processor& mProcessor;
        float mNote;
    };

   public:
    explicit Processor(ParamsFeature::AudioHandle& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        auto status = mVoices.ProcessAudio(out);
        return status;
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mVoices.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override {
        mVoices.ProcessNoteOff(note); 
    }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override {
                                        mVoices.ProcessNoteChoke(note);
    }

    void ProcessReset() override { mVoices.Reset(); }

   private:
    clapeze::VoicePool<Processor, Voice, cMaxVoices> mVoices;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, clapeze::BasePlugin& plugin, ParamsFeature& params)
        : kitgui::BaseApp(ctx),
          mPlugin(plugin),
          mParams(params),
          mPresetBrowser(plugin) {}
    ~GuiApp() = default;

    void OnActivate() override {
        float scale = static_cast<float>(GetContext().GetUIScale());
        // TODO: to update all this if the viewport changes
        uint32_t w{};
        uint32_t h{};
        GetContext().GetSizeInPixels(w, h);
    }

    void OnUpdate() override {
        mParams.FlushFromAudio();
        auto& paramsHandle = mParams.GetMainHandle();
    }

    void OnGuiUpdate() override {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Preset")) {
                if (ImGui::MenuItem("Reset All")) {
                    mParams.ResetAllParamsToDefault();
                }
                clapeze::BaseStateFeature& state =
                    clapeze::BaseFeature::GetFromPlugin<clapeze::BaseStateFeature>(mPlugin);
                if (ImGui::MenuItem("Copy to Clipboard")) {
                    std::stringstream ss;
                    if (state.Save(ss)) {
                        ImGui::SetClipboardText(ss.str().c_str());
                    }
                }
                if (ImGui::MenuItem("Load from Clipboard")) {
                    const char* s = ImGui::GetClipboardText();
                    std::stringstream ss(s);
                    state.Load(ss);
                }
                ImGui::EndMenu();
            }
            ImGui::MenuItem("Debug", NULL, &mDebugMode);
            ImGui::MenuItem("Help/About", NULL, &mShowHelpWindow);
            ImGui::EndMainMenuBar();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
            mDebugMode = !mDebugMode;
        }

        if (mShowHelpWindow) {
            if (ImGui::Begin("Help", &mShowHelpWindow)) {
                ImGui::TextWrapped(
                    "layersynth is an intentionally simple-ish virtual analog polysynth. This is the first one I'm "
                    "really dumping effort into the UI for so extra feedback there is very appreciated :)");
                ImGui::BulletText("Shift-click for fine adjustment");
                ImGui::BulletText("double-click to reset to default");
                ImGui::BulletText("right-click to get a text input");
            }
            ImGui::End();
        }
    }

    void OnDraw() override {}

   private:
    clapeze::BasePlugin& mPlugin;
    ParamsFeature& mParams;
    kitgui::PresetBrowser mPresetBrowser;
    bool mDebugMode = false;
    bool mShowHelpWindow = false;
};
#endif

class Plugin : public InstrumentPlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(const clap_plugin_descriptor_t& meta) : InstrumentPlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
#if KITSBLIPS_ENABLE_SENTRY
        GetHost().SetLogFn([](clapeze::LogSeverity severity, const std::string& message) {
            switch (severity) {
                case LogSeverity::Debug:
                    sentry_log_debug(message.c_str());
                    break;
                case LogSeverity::Info:
                    sentry_log_info(message.c_str());
                    break;
                case LogSeverity::Warning:
                    sentry_log_warn(message.c_str());
                    break;
                case LogSeverity::Error:
                    sentry_log_error(message.c_str());
                    break;
                case LogSeverity::Fatal:
                    sentry_log_fatal(message.c_str());
                    break;
            }
        });
#endif

        InstrumentPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count);

        ConfigFeature<clapeze::AssetsFeature>();
        ConfigFeature<TomlStateFeature<ParamsFeature>>(*this);
        ConfigFeature<clapeze::PresetFeature>(*this);

#if KITSBLIPS_ENABLE_GUI
        // aspect ratio 1.5
        kitgui::SizeConfig cfg{750, 500, false, true};
        ConfigFeature<KitguiFeature>(
            GetHost(), [this, &params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, *this, params); },
            cfg);
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioInstrumentDescriptor("kitsblips.layersynth",
                                                  "layersynth",
                                                  "A Linear-Arithmetic inspired "));
}  // namespace layersynth
