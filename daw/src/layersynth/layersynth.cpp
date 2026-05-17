#include <clap/ext/params.h>
#include <clapeze/basePlugin.h>
#include <clapeze/common.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/params/dynamicParametersFeature.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/presetFeature.h>
#include <clapeze/features/state/baseStateFeature.h>
#include <clapeze/features/state/tomlStateFeature.h>
#include <clapeze/impl/stringUtils.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/processor/transport.h>
#include <clapeze/processor/voice.h>
#include <etl/flat_multimap.h>
#include <etl/vector.h>
#include <cfloat>
#include <memory>
#include <sstream>

#include <descriptor.h>

#include "layersynth/dsp.h"

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/features/assetsFeature.h>
#include <imgui.h>
#include <kitgui/app.h>
#include <kitgui/gfx/scene.h>
#include <kitgui/context.h>
#include "gui/kitguiFeature.h"
#include "gui/debugui.h"
#include "gui/parameterControls.h"
#include "gui/presetBrowser.h"
#endif

namespace {
constexpr size_t cMaxVoices = 16;

enum class LfoParams : clap_id {
    Wave,
    Rate,
    Delay,
    Sync,
    Count = 4u,
};
enum class EnvParams : clap_id {
    Attack,
    Decay,
    Sustain,
    Release,
    Count = 4u,
};
enum class PartialParams : clap_id {
    Wave,
    PitchOffset,
    PitchEnvMult,
    PitchLfoMult,
    Duty,
    DutyLfoSource,
    DutyLfoMult,
    FilterNote,
    FilterTrackingMult,
    FilterEnvMult,
    FilterLfoSource,
    FilterLfoMult,
    FilterRes,
    VcaMult,
    VcaLfoSource,
    VcaLfoMult,
    // + filterEnv
    // + volumeEnv
    Count = 16u + static_cast<clap_id>(EnvParams::Count) * 2, 
};
enum class ToneParams {
    ChorusMix,
    ChorusRate,
    ChorusDepth,
    PartialMix,
    // + partial1
    // + partial2
    // + lfo1
    // + lfo2
    // + lfo3
    // + pitchEnv
    Count = 4u + static_cast<clap_id>(PartialParams::Count) * 2 + static_cast<clap_id>(LfoParams::Count) * 3 +
            static_cast<clap_id>(EnvParams::Count) * 1,
};
enum class GlobalParams {
    Tune,
    ToneMix,
    // + tone1
    // + tone2
    Count = 2u + static_cast<clap_id>(ToneParams::Count) * 2,
};

struct LfoRateParam : public clapeze::NumericParam {
    LfoRateParam(std::string_view key, std::string_view name) : clapeze::NumericParam(key, name, 0.001f, 20.0f, 0.2f, "hz") { mCurve = clapeze::cPowCurve<3.0f>; }
};

struct EnvParam : public clapeze::NumericParam {
    EnvParam(std::string_view key, std::string_view name, float min, float max) : clapeze::NumericParam(key, name, min, max, min, "ms") {
        mCurve = clapeze::cPowCurve<2.0f>;
    }
};

using ParamsFeature = clapeze::params::DynamicParametersFeature;
}  // namespace

using namespace clapeze;

namespace layersynth {

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(ParamsFeature::AudioHandle& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        float sr = static_cast<float>(GetSampleRate());
        mVoices.SetNumVoices(16);
        mVoices.SetStrategy(clapeze::VoiceStrategy::Poly);
        mVoices.ForEach([&](Voice& v) {
            auto LfoParams = [&](kitdsp::lfo::TriangleOscillator& lfo, clap_id first) {
                float wave = mParams.Get<PercentParam>(first+static_cast<clap_id>(LfoParams::Wave));
                float rate = mParams.Get<PercentParam>(first+static_cast<clap_id>(LfoParams::Rate));
                float delay = mParams.Get<PercentParam>(first+static_cast<clap_id>(LfoParams::Delay));
                float sync = mParams.Get<PercentParam>(first+static_cast<clap_id>(LfoParams::Sync));
                lfo.SetFrequency(rate, sr);
            };
            auto EnvParams = [&](kitdsp::ApproachAdsr& adsr, clap_id first) {
                float a = mParams.Get<NumericParam>(first+static_cast<clap_id>(EnvParams::Attack));
                float d = mParams.Get<NumericParam>(first+static_cast<clap_id>(EnvParams::Decay));
                float s = mParams.Get<PercentParam>(first+static_cast<clap_id>(EnvParams::Sustain));
                float r = mParams.Get<NumericParam>(first+static_cast<clap_id>(EnvParams::Release));
                adsr.SetParams(a, d, s, r, sr);
            };
            auto PartialParams = [&](Partial& part, clap_id first) {
                // = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::Wave));
                part.mNoteOffset = mParams.Get<NumericParam>(first+static_cast<clap_id>(PartialParams::PitchOffset));
                part.mPitchEnvMult = mParams.Get<NumericParam>(first+static_cast<clap_id>(PartialParams::PitchEnvMult));
                part.mPitchLfoMult = mParams.Get<NumericParam>(first+static_cast<clap_id>(PartialParams::PitchLfoMult));
                part.mDuty = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::Duty));
                //mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::DutyLfoSource));
                part.mDutyLfoMult= mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::DutyLfoMult));
                part.mFilterNote = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::FilterNote)) * 80.0f;
                part.mFilterTrackingMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::FilterTrackingMult)) * 80.0f;
                part.mFilterEnvMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::FilterEnvMult));
                //part.mFilterLfoMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::FilterLfoSource));
                part.mFilterLfoMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::FilterLfoMult));
                part.mFilterRes = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::FilterRes));
                part.mVcaMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::VcaMult));
                //part.mVcaLfoMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::VcaLfoSource));
                part.mVcaLfoMult = mParams.Get<PercentParam>(first+static_cast<clap_id>(PartialParams::VcaLfoMult));
                first += 16;
                EnvParams(part.mFilterEnv, first);
                first += static_cast<clap_id>(EnvParams::Count);
                EnvParams(part.mVolumeEnv, first);
            };
            auto ToneParams = [&](Tone& tone, clap_id first) {
                if(tone.mChorus) {
                    auto& cfg = tone.mChorus->cfg;
                    cfg.mix = mParams.Get<PercentParam>(first+static_cast<clap_id>(ToneParams::ChorusMix));
                    cfg.mix = mParams.Get<PercentParam>(first+static_cast<clap_id>(ToneParams::ChorusRate));
                    cfg.mix = mParams.Get<PercentParam>(first+static_cast<clap_id>(ToneParams::ChorusDepth));
                }
                tone.mPartialMix = mParams.Get<PercentParam>(first+static_cast<clap_id>(ToneParams::PartialMix));
                first += 4;
                PartialParams(tone.mPartial1, first);
                first += static_cast<clap_id>(PartialParams::Count);
                PartialParams(tone.mPartial2, first);
                first += static_cast<clap_id>(PartialParams::Count);
                LfoParams(tone.mLfo1, first);
                first += static_cast<clap_id>(LfoParams::Count);
                LfoParams(tone.mLfo2, first);
                first += static_cast<clap_id>(LfoParams::Count);
                LfoParams(tone.mLfo3, first);
                first += static_cast<clap_id>(LfoParams::Count);
                EnvParams(tone.mPitchEnv, first);
            };
            auto GlobalParams = [&]() {
                //mParams.Get<PercentParam>(static_cast<clap_id>(GlobalParams::Tune));
                //mParams.Get<PercentParam>(static_cast<clap_id>(GlobalParams::ToneMix));
                ToneParams(v.mTone1, 2);
                ToneParams(v.mTone2, 2 + static_cast<clap_id>(ToneParams::Count));
            };

            GlobalParams();
        });
        auto status = mVoices.ProcessAudio(out);
        return status;
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mVoices.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override {
        mVoices.ProcessNoteOff(note); 
    }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() override { mVoices.Reset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRatef = static_cast<float>(sampleRate);
    }


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

        ImGui::TextWrapped(
            "layersynth is an intentionally simple-ish virtual analog polysynth. This is the first one I'm "
            "really dumping effort into the UI for so extra feedback there is very appreciated :)");
        ImGui::BulletText("Shift-click for fine adjustment");
        ImGui::BulletText("double-click to reset to default");
        ImGui::BulletText("right-click to get a text input");
        for(clap_id idx = 0; idx < static_cast<clap_id>(GlobalParams::Count); ++idx) 
        {
            kitgui::DebugParam(mParams, idx);
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
        InstrumentPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), static_cast<clap_id>(GlobalParams::Count));
        clap_id next_idx; 
        auto LfoParams = [&] (std::string pre) {
            params.Parameter(next_idx++, new PercentParam(pre+"_wave", pre+": Wave", 0.0f));
            params.Parameter(next_idx++, new LfoRateParam(pre+"_rate", pre+": Rate"));
            params.Parameter(next_idx++, new PercentParam(pre+"_delay", pre+": Delay", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_sync", pre+": Sync", 0.0f));
        };
        auto EnvParams = [&] (std::string pre) {
            params.Parameter(next_idx++, new EnvParam(pre+"_attack", pre+": Attack", 1.0f, 1000.0f));
            params.Parameter(next_idx++, new EnvParam(pre+"_decay", pre+": Decay", 10.0f, 30000.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_sustain", pre+": Sustain", 1.0f));
            params.Parameter(next_idx++, new EnvParam(pre+"_release", pre+": Release", 10.0f, 30000.0f));
        };
        auto PartialParams = [&] (std::string pre) {
            params.Parameter(next_idx++, new PercentParam(pre+"_wave", pre+": Wave", 0.0f));
            params.Parameter(next_idx++, new NumericParam(pre+"_pitch", pre+": Pitch", -32.0f, 32.0f, 0.0f, "semis"));
            params.Parameter(next_idx++, new PercentParam(pre+"_pitchEnv", pre+": Pitch Env amount", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_pitchLfo", pre+": Pitch LFO amount", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_duty", pre+": Duty", 0.5f));
            params.Parameter(next_idx++, new PercentParam(pre+"_dutySrc", pre+": Duty Mod source", 0.5f));
            params.Parameter(next_idx++, new PercentParam(pre+"_dutyMult", pre+": Duty Mod amount", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_filter", pre+": Filter Cutoff", 1.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_filterTrack", pre+": Filter Tracking", 0.5f));
            params.Parameter(next_idx++, new PercentParam(pre+"_filterEnvMult", pre+": Filter Env amount", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_filterSrc", pre+": Filter Mod source", 0.5f));
            params.Parameter(next_idx++, new PercentParam(pre+"_filterSrcMult", pre+": Filter Mod amount", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_filterRes", pre+": Filter Resonance", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_vcaMult", pre+": Volume", 0.5f));
            params.Parameter(next_idx++, new PercentParam(pre+"_vcaLfoSource", pre+": Volume Mod source", 0.5f));
            params.Parameter(next_idx++, new PercentParam(pre+"_vcaLfoMult", pre+": Volume Mod amount", 0.0f));
            EnvParams("FilterEnv");
            EnvParams("volumeEnv");
        };
        auto ToneParams = [&] (std::string pre) {
            params.Parameter(next_idx++, new PercentParam(pre+"_chorusMix", pre+": Chorus Mix", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_chorusRate", pre+": Chorus Rate", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_chorusDepth", pre+": Chorus Depth", 0.0f));
            params.Parameter(next_idx++, new PercentParam(pre+"_partialMix", pre+": Partial Mix", 0.5f));
            PartialParams(pre+"Part1");
            PartialParams(pre+"Part2");
            LfoParams(pre+"Lfo1");
            LfoParams(pre+"Lfo2");
            LfoParams(pre+"Lfo3");
            EnvParams(pre+"PitchEnv");
        };
        auto GlobalParams = [&] () {
            params.Parameter(next_idx++, new NumericParam("tune", "Tune", 20.0f, 1000.0f, 440.0f, "hz"));
            params.Parameter(next_idx++, new PercentParam("toneMix", "Tone Mix", 0.5f));
            ToneParams("Tone1");
            ToneParams("Tone2");
        };

        GlobalParams();

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
