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
#include <kitdsp/volume/dbMeter.h>
#include <clapeze/processor/transport.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include <kitgui/wrap_nfd.h>
#include <kitgui/gfx/image.h>
#include <misc/cpp/imgui_stdlib.h>
#include "gui/kitguiFeature.h"
#include "gui/debugui.h"
#include "gui/presetBrowser.h"
#endif
#include <clapeze/processor/voice.h>

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

struct SaveData {
    std::string talkImagePath;
    std::string idleImagePath;
    bool changed;
};

struct VoiceUpdate {
    size_t voiceIndex;
    bool talking;
};
using VoiceQueue = etl::queue_spsc_atomic<VoiceUpdate, 4000, etl::memory_model::MEMORY_MODEL_MEDIUM>;

class StateFeature : public TomlStateFeature<ParamsFeature> {
   public:
    StateFeature(BasePlugin& self, SaveData& saveData)
        : TomlStateFeature(self, 0), mSaveData(saveData) {}
    bool OnSave(toml::table& file) const override {
        toml::table table{};
        table.insert("talk", mSaveData.talkImagePath);
        table.insert("idle", mSaveData.idleImagePath);
        file.insert("paths", table);
        return true;
    }
    bool OnLoad(const toml::table& file) override {
        auto table = file["paths"].as_table();
        if (table) {
            mSaveData.talkImagePath = table->at_path("talk").value_or("");
            mSaveData.idleImagePath = table->at_path("idle").value_or("");
            mSaveData.changed = true;
        }
        return true;
    }

   private:
   SaveData& mSaveData;
};

class Processor : public BaseProcessor {
    class Voice {
       public:
        explicit Voice(Processor& p, size_t idx) : mProcessor(p), mIdx(idx) {}
        void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
            mNote = note.key;
            mActive = true;
        }
        void ProcessNoteOff() { mActive = false; }
        void ProcessChoke() { mActive = false; }
        void Reset() { mActive = false; }
        bool ProcessAudio(clapeze::StereoAudioBuffer& out) { return mActive; }

        bool mActive=false;
        bool mLastActive=false;
        size_t mIdx;
       private:
        Processor& mProcessor;
        int16_t mNote;
    };

   public:
    explicit Processor(PluginHost& host, ParamsFeature::AudioHandle& params, VoiceQueue& voiceQueue) : BaseProcessor(host), mParams(params), mVoiceQueue(voiceQueue), mVoices(*this, params) {}
    ~Processor() = default;

    void ProcessReset() override {}

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRatef = narrow_cast<float>(sampleRate);
        mMeter = kitdsp::DbMeter(sampleRatef);
        mMeter.SetFrequency(10.0f, sampleRatef);
    }

    void ProcessEvent(const clap_event_header_t& event) final {
        if (mParams.ProcessEvent(event)) {
            return;
        }

        if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
            // clap events
            switch (event.type) {
                case CLAP_EVENT_MIDI: {
                    const auto& midiChange = reinterpret_cast<const clap_event_midi_t&>(event);
                    uint8_t status = midiChange.data[0];
                    uint8_t channel = status & 0x0F;
                    if((status & 0x80) == 0x80) {
                        // note off
                        uint8_t note = midiChange.data[1];
                        mVoices.ProcessNoteOff({-1, -1, channel, note});
                    } else if ((status & 0x90) == 0x90) {
                        // note on
                        uint8_t note = midiChange.data[1];
                        uint8_t velocity = midiChange.data[2];
                        if(velocity != 0) {
                            mVoices.ProcessNoteOn({-1, -1, channel, note}, static_cast<float>(velocity) / 127.0f);
                        } else {
                            mVoices.ProcessNoteOff({-1, -1, channel, note});
                        }
                    //} else if ((status & 0xA0) == 0xA0) {
                    //    // pressure
                    //    uint8_t note = midiChange.data[1];
                    //    uint8_t value = midiChange.data[2];
                    //} else if ((status & 0xD0) == 0xD0) {
                    //    // channel pressure
                    //    uint8_t value = midiChange.data[1];
                    }
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

        int32_t polyCount = mParams.Get<Params::Voices>();
        mVoices.SetNumVoices(polyCount);
        mVoices.SetStrategy(polyCount > 1 ? clapeze::VoiceStrategy::Poly : clapeze::VoiceStrategy::MonoLast);

        float db{};
        for(float in : in.left) {
            db = mMeter.Process(in);
        }

        float dummyData{};
        StereoAudioBuffer dummyOut{
            {&dummyData, 1},
            {&dummyData, 1},
            false,
            false,
        };
        mVoices.ProcessAudio(dummyOut);

        for(auto& voice: mVoices.IterAll()) {
            if(voice.mLastActive != voice.mActive) {
                mVoiceQueue.push({voice.mIdx, voice.mActive});
                voice.mLastActive = voice.mActive;
            }
        }
        float threshold = mAudioActive ? mParams.Get<Params::Threshold>() - 3.0f : mParams.Get<Params::Threshold>();
        bool audioActive = db > threshold;
        if(audioActive != mAudioActive) {
            mVoiceQueue.push({0, audioActive});
            mAudioActive = audioActive;
        }

        // bypass
        if (in.left.data() != out.left.data()) {
            out.CopyFrom(in);
        }

        return ProcessStatus::Continue;
    }

    clapeze::VoicePool<Processor, Voice, 16> mVoices;
    kitdsp::DbMeter mMeter{44100};
    bool mAudioActive = false;
    ParamsFeature::AudioHandle& mParams;
    VoiceQueue& mVoiceQueue;
    Transport mTransport{};
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, clapeze::BasePlugin& plugin, ParamsFeature& params, VoiceQueue& voiceQueue, SaveData& saveData) : kitgui::BaseApp(ctx), mPresetBrowser(plugin), mParams(params), mVoiceQueue(voiceQueue), mSaveData(saveData) {}
    void OnUpdate() override {
        mParams.FlushFromAudio();
        VoiceUpdate update;
        while (mVoiceQueue.pop(update)) {
            // TODO: multi-voice
            mTalking = update.talking;
        }

        uint32_t w{};
        uint32_t h{};
        GetContext().GetSize(w, h);
        ImVec2 size(narrow_cast<float>(w), narrow_cast<float>(h));
        if(mTalking && mTalkImage) {
            ImGui::Image(mTalkImage->GetId(), size, ImVec2(0, 1), ImVec2(1, 0));
        } else if (mIdleImage) {
            ImGui::Image(mIdleImage->GetId(), size, ImVec2(0, 1), ImVec2(1, 0));
        }
    }
    void OnGuiUpdate() override {
        auto LoadImage = [&](std::string_view ipath, std::unique_ptr<kitgui::Image>* img) {
            (*img).reset();
            (*img) = std::make_unique<kitgui::Image>(GetContext());
            std::string uri;
            uri += "file://";
            uri += ipath;
            (*img)->Load(uri);
        };
        if(mSaveData.changed) {
            LoadImage(mSaveData.idleImagePath, &mIdleImage);
            LoadImage(mSaveData.talkImagePath, &mTalkImage);
            mSaveData.changed = false;
        }
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            mShowSetup = true;
        }
        if (mShowSetup) {
            if (ImGui::Begin("Setup", &mShowSetup)) {
                auto ImageLine = [this, LoadImage](const char* label, std::string& path, std::unique_ptr<kitgui::Image>* img) {
                    ImGui::PushID(label);
                    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * 0.33f);
                    if (ImGui::Button("Browse")) {
                        nfdu8char_t* outPath{};
                        nfdu8filteritem_t filters[2] = {{"Image", "png,webp,jpeg,jpg,gif,ktx,dds"}};
                        nfdopendialogu8args_t args{};
                        args.filterList = filters;
                        args.filterCount = 1;
                        args.parentWindow = NFD_GetWindow(GetContext().GetWindow());
                        nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
                        if (result == NFD_OKAY) {
                            path = outPath;
                            LoadImage(outPath, img);
                            NFD_FreePathU8(outPath);
                        } else if (result == NFD_CANCEL) {
                            // mSampleStatus[i] = "<canceled>";
                        } else {
                            // mSampleStatus[i] = NFD_GetError();
                        }
                    }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() * 0.66f);
                    if (ImGui::InputText(label, &path, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        LoadImage(path, img);
                    }
                    ImGui::PopID();
                };
                ImageLine("Singing", mSaveData.talkImagePath, &mTalkImage);
                ImageLine("Idle", mSaveData.idleImagePath, &mIdleImage);
                kitgui::DebugParam<ParamsFeature, Params::Voices>(mParams);
                kitgui::DebugParam<ParamsFeature, Params::Threshold>(mParams);
            }
            ImGui::End();
        }
    }

   private:
    std::unique_ptr<kitgui::Image> mTalkImage;
    std::unique_ptr<kitgui::Image> mIdleImage;
    bool mTalking;

    bool mShowSetup;
    kitgui::PresetBrowser mPresetBrowser;
    ParamsFeature& mParams;
    VoiceQueue& mVoiceQueue;
    SaveData& mSaveData;
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
        ConfigFeature<StateFeature>(*this, mSaveData);
        ConfigFeature<AssetsFeature>();

#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(GetHost(),
                                     [this, &params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, *this, params, mVoiceQueue, mSaveData); });
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>(), mVoiceQueue);
    }
    private:
    VoiceQueue mVoiceQueue;
    SaveData mSaveData;
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioEffectDescriptor("kitsblips.singer", "singer", "Plugin description"));

}  // namespace singer
