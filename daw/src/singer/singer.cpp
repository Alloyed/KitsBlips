#include <clapeze/basePlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/features/notePortsFeature.h>
#include <clapeze/features/stereoAudioPortsFeature.h>
#include <clapeze/features/params/enumParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/state/tomlStateFeature.h>
#include <clapeze/processor/baseProcessor.h>
#include <kitdsp/math/util.h>
#include <clapeze/processor/transport.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include <kitgui/wrap_nfd.h>
#include <kitgui/gfx/image.h>
#include <misc/cpp/imgui_stdlib.h>
#include "gui/kitguiFeature.h"
#include "gui/presetBrowser.h"
#endif

namespace {
enum class Params : clap_id { Voices, Threshold, Count };
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::Voices> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Voices", "Voices", 1, 8, 1) {}
};
template <>
struct ParamTraits<Params, Params::Threshold> : public clapeze::DbParam {
    ParamTraits() : clapeze::DbParam("Threshold", "Audio Threshold", -80.0, 0.0, -10.0f) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace singer {

struct VoiceUpdate {
    size_t voiceIndex;
    bool active;
};
using VoiceQueue = etl::queue_spsc_atomic<VoiceUpdate, 50, etl::memory_model::MEMORY_MODEL_SMALL>;

class StateFeature : public TomlStateFeature<ParamsFeature> {
   public:
    StateFeature(BasePlugin& self)
        : TomlStateFeature(self, 0) {}
    bool OnSave(toml::table& file) const override {
        (void)file;
        return true;
    }
    bool OnLoad(const toml::table& file) override {
        (void)file;
        return true;
    }

   private:
};

class Processor : public BaseProcessor {
   public:
    explicit Processor(PluginHost& host, ParamsFeature::AudioHandle& params, VoiceQueue& voiceQueue) : BaseProcessor(host), mParams(params), mVoiceQueue(voiceQueue) {}
    ~Processor() = default;

    void ProcessReset() override {}

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)sampleRate;
        (void)minBlockSize;
        (void)maxBlockSize;
    }

    void ProcessEvent(const clap_event_header_t& event) final {
        if (mParams.ProcessEvent(event)) {
            return;
        }

        if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
            // clap events
            switch (event.type) {
                case CLAP_EVENT_NOTE_ON: {
                    const auto& noteChange = reinterpret_cast<const clap_event_note_t&>(event);
                    NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                    //ProcessNoteOn(note, static_cast<float>(noteChange.velocity));
                    break;
                }
                case CLAP_EVENT_NOTE_OFF: {
                    const auto& noteChange = reinterpret_cast<const clap_event_note_t&>(event);
                    NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                    //ProcessNoteOff(note);
                    break;
                }
                case CLAP_EVENT_NOTE_CHOKE: {
                    const auto& noteChange = reinterpret_cast<const clap_event_note_t&>(event);
                    NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                    //ProcessNoteChoke(note);
                    break;
                }
                case CLAP_EVENT_NOTE_EXPRESSION: {
                    const auto& noteChange = reinterpret_cast<const clap_event_note_expression_t&>(event);
                    NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                    //ProcessNoteExpression(note, noteChange.expression_id, static_cast<float>(noteChange.value));
                    break;
                }
                case CLAP_EVENT_TRANSPORT: {
                    const auto& transport = reinterpret_cast<const clap_event_transport_t&>(event);
                    mTransport.ProcessEvent(transport);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

    const Transport& GetTransport() const { return mTransport; }

    // impl
    void ProcessFlush(const clap_process_t& process) final {
        if (process.transport) {
            mTransport.ProcessEvent(*process.transport);
        }
        mParams.FlushEventsFromMain(*this, process.out_events);
    }
    ProcessStatus ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) final {
        size_t numSamples = rangeStop - rangeStart;
        const StereoAudioBuffer in{
            // inLeft
            {process.audio_inputs[0].data32[0] + rangeStart, numSamples},
            // inRight
            {process.audio_inputs[0].data32[1] + rangeStart, numSamples},
            // isInLeftConstant
            (process.audio_inputs[0].constant_mask & (1 << 0)) != 0,
            // isInRightConstant
            (process.audio_inputs[0].constant_mask & (1 << 1)) != 0,
        };
        StereoAudioBuffer out{
            // outLeft
            {process.audio_outputs[0].data32[0] + rangeStart, numSamples},
            // outRight
            {process.audio_outputs[0].data32[1] + rangeStart, numSamples},
            // isOutLeftConstant
            false,
            // isOutRightConstant
            false,
        };
        // bypass
        if(in.left.data() != out.left.data()) {
            out.CopyFrom(in);
        }

        return ProcessStatus::Continue;
    }

    ParamsFeature::AudioHandle& mParams;
    VoiceQueue& mVoiceQueue;
    Transport mTransport{};
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, clapeze::BasePlugin& plugin, ParamsFeature& params, VoiceQueue& voiceQueue) : kitgui::BaseApp(ctx), mPresetBrowser(plugin), mParams(params), mVoiceQueue(voiceQueue) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        if(mOpenImage) {
            ImGui::Image(mOpenImage->GetId(), ImVec2(100, 100));
        }
    }
    void OnGuiUpdate() override {
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("Options");
        }
        if(ImGui::BeginPopup("Options")) {
            if(ImGui::MenuItem("Setup...")) {
                ImGui::OpenPopup("Setup");
            }
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("Setup")) {
            if (ImGui::InputText("Open Image", &mOpenImagePath, ImGuiInputTextFlags_EnterReturnsTrue)) {
                //LoadSample(i, mSamplePaths[i]);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                nfdu8char_t* outPath{};
                nfdu8filteritem_t filters[2] = {{"Image", "png"}};
                nfdopendialogu8args_t args{};
                args.filterList = filters;
                args.filterCount = 1;
                args.parentWindow = NFD_GetWindow(GetContext().GetWindow());
                nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
                if (result == NFD_OKAY) {
                    //LoadSample(i, outPath);
                    NFD_FreePathU8(outPath);
                } else if (result == NFD_CANCEL) {
                    //mSampleStatus[i] = "<canceled>";
                } else {
                    //mSampleStatus[i] = NFD_GetError();
                }
            }
        }
    }

   private:
    std::unique_ptr<kitgui::Image> mOpenImage;
    std::unique_ptr<kitgui::Image> mClosedImage;
    std::string mOpenImagePath;
    std::string mClosedImagePath;
    kitgui::PresetBrowser mPresetBrowser;
    ParamsFeature& mParams;
    VoiceQueue& mVoiceQueue;
};
#endif

class Plugin : public BasePlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(const clap_plugin_descriptor_t& meta) : BasePlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
        .Parameter<Params::Voices>()
        .Parameter<Params::Threshold>();


        ConfigFeature<NotePortsFeature>(1, 0);
        ConfigFeature<StereoAudioPortsFeature>(1, 1, true);
        ConfigFeature<StateFeature>(*this);
        ConfigFeature<AssetsFeature>();

#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [this, &params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, *this, params, mVoiceQueue); });
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>(), mVoiceQueue);
    }
    private:
    VoiceQueue mVoiceQueue;
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.singer", "singer", "Plugin description"));

}  // namespace singer
