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
#include <memory>
#include <sstream>
#include "AudioFile.h"

#include <descriptor.h>

#include "layersynth/dsp.h"

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/features/assetsFeature.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <kitgui/app.h>
#include <kitgui/context.h>
#include <kitgui/gfx/scene.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
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
    EqLowFrequency,
    EqLowGain,
    EqHighFrequency,
    EqHighGain,
    PartialMix,
    // + partial1
    // + partial2
    // + lfo1
    // + lfo2
    // + lfo3
    // + pitchEnv
    Count = 7u + static_cast<clap_id>(PartialParams::Count) * 2 + static_cast<clap_id>(LfoParams::Count) * 3 +
            static_cast<clap_id>(EnvParams::Count) * 1,
};
enum class GlobalParams {
    Tune,
    ToneAlgorithm,
    // + tone1
    // + tone2
    Count = 2u + static_cast<clap_id>(ToneParams::Count) * 2,
};

struct LfoRateParam : public clapeze::NumericParam {
    LfoRateParam(std::string_view key, std::string_view name)
        : clapeze::NumericParam(key, name, 0.001f, 20.0f, 0.2f, "hz") {
        mCurve = clapeze::cPowCurve<3.0f>;
    }
};

struct EnvParam : public clapeze::NumericParam {
    EnvParam(std::string_view key, std::string_view name, float min, float max)
        : clapeze::NumericParam(key, name, min, max, min, "ms") {
        mCurve = clapeze::cPowCurve<2.0f>;
    }
};

struct ModSourceParam : public clapeze::IntegerParam {
    ModSourceParam(std::string_view key, std::string_view name, int32_t defaultValue = 1)
        : clapeze::IntegerParam(key, name, 1, 3, defaultValue) {}
};

using ParamsFeature = clapeze::params::DynamicParametersFeature;
}  // namespace

using namespace clapeze;

namespace layersynth {

template <size_t TNumSamples>
class RawSampleLoader {
   public:
    using File = AudioFile<float>;
    using Queue = etl::queue_spsc_atomic<std::pair<size_t, File*>, 200, etl::memory_model::MEMORY_MODEL_SMALL>;

    class AudioHandle {
       public:
        AudioHandle(Queue& mainToAudio, Queue& audioToMain) : mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}
        void ReleaseSample(size_t idx) {
            mAudioToMain.push({idx, mSampleData[idx]});
            mSampleData[idx] = nullptr;
            mSamplers[idx] = std::nullopt;
        }
        void OnAudioUpdate() {
            std::pair<size_t, File*> pair;
            while (mMainToAudio.pop(pair)) {
                if (mSampleData[pair.first]) {
                    ReleaseSample(pair.first);
                }
                mSampleData[pair.first] = pair.second;
                mSamplers[pair.first] = std::make_optional<Sampler>(pair.second->samples[0]);
            }
        }
        const Sampler* GetSampler(size_t i) const { return mSamplers[i] ? &(*mSamplers[i]) : nullptr; }
        size_t GetNumSamplers() const { return mSamplers.size(); }

       private:
        Queue& mMainToAudio;
        Queue& mAudioToMain;
        etl::array<File*, TNumSamples> mSampleData;
        etl::array<std::optional<Sampler>, TNumSamples> mSamplers;
    };

    RawSampleLoader() : mAudioHandle(mMainToAudio, mAudioToMain) {}

    void LoadSample(size_t idx, const std::string& path) {
        File* f = new AudioFile<float>(path);
        mMainToAudio.push({idx, f});
        mSamplePaths[idx] = path;
    }

    std::string GetSamplePath(size_t idx) { return mSamplePaths[idx]; }

    void OnMainUpdate() {
        std::pair<size_t, File*> pair;
        while (mAudioToMain.pop(pair)) {
            delete pair.second;
        }
    }
    AudioHandle& GetAudioHandle() { return mAudioHandle; }

    void OnImGui() {
        for (size_t i = 0; i < TNumSamples; ++i) {
            ImGui::PushID(static_cast<int>(i));
            // ImGui::Text("%s", fmt::format("{}: {}", i, GetSamplePath(i)));
            ImGui::Text("%s", fmt::format("{}", i).c_str());

            ImGui::PopID();
        }
    }

   private:
    Queue mMainToAudio{};
    Queue mAudioToMain{};
    AudioHandle mAudioHandle;
    etl::array<std::string, TNumSamples> mSamplePaths{};
};
using SampleLoader = RawSampleLoader<4>;

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(ParamsFeature::AudioHandle& params, SampleLoader::AudioHandle& sampleLoader)
        : InstrumentProcessor(params), mVoices(*this), mSampleLoader(sampleLoader) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        float sr = static_cast<float>(GetSampleRate());
        mVoices.SetNumVoices(16);
        mVoices.SetStrategy(clapeze::VoiceStrategy::Poly);
        mVoices.ForEach([&](Voice& v) {
            auto LfoParams = [&](kitdsp::lfo::TriangleOscillator& lfo, clap_id first) {
                // float wave = mParams.Get<PercentParam>(first + static_cast<clap_id>(LfoParams::Wave));
                float rate = mParams.Get<LfoRateParam>(first + static_cast<clap_id>(LfoParams::Rate));
                // float delay = mParams.Get<PercentParam>(first + static_cast<clap_id>(LfoParams::Delay));
                // float sync = mParams.Get<PercentParam>(first + static_cast<clap_id>(LfoParams::Sync));
                lfo.SetFrequency(rate, sr);
            };
            auto EnvParams = [&](kitdsp::ApproachAdsr& adsr, clap_id first) {
                float a = mParams.Get<EnvParam>(first + static_cast<clap_id>(EnvParams::Attack));
                float d = mParams.Get<EnvParam>(first + static_cast<clap_id>(EnvParams::Decay));
                float s = mParams.Get<PercentParam>(first + static_cast<clap_id>(EnvParams::Sustain));
                float r = mParams.Get<EnvParam>(first + static_cast<clap_id>(EnvParams::Release));
                adsr.SetParams(a, d, s, r, sr);
            };
            auto PartialParams = [&](Partial& part, clap_id first) {
                part.mPcmSampler = mSampleLoader.GetSampler(1);
                part.sr = sr;
                part.mWave = mParams.Get<EnumParam<Partial::Wave>>(first + static_cast<clap_id>(PartialParams::Wave));
                part.mNoteOffset = mParams.Get<NumericParam>(first + static_cast<clap_id>(PartialParams::PitchOffset));
                part.mTune = mParams.Get<NumericParam>(static_cast<clap_id>(GlobalParams::Tune));
                part.mPitchEnvMult =
                    mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::PitchEnvMult));
                part.mPitchLfoMult =
                    mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::PitchLfoMult));
                part.mDuty = mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::Duty));
                part.mDutyLfoMult = mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::DutyLfoMult));
                part.mFilterNote =
                    mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::FilterNote)) * 80.0f;
                part.mFilterTrackingMult =
                    mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::FilterTrackingMult));
                part.mFilterEnvMult =
                    mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::FilterEnvMult));
                part.mFilterLfoMult =
                    mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::FilterLfoMult));
                part.mFilterRes = mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::FilterRes));
                part.mVcaMult = mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::VcaMult));
                part.mVcaLfoMult = mParams.Get<PercentParam>(first + static_cast<clap_id>(PartialParams::VcaLfoMult));
                first += 16;
                EnvParams(part.mFilterEnv, first);
                first += static_cast<clap_id>(EnvParams::Count);
                EnvParams(part.mVolumeEnv, first);
            };
            auto ToneParams = [&](Tone& tone, clap_id first) {
                if (tone.mChorus) {
                    auto& cfg = tone.mChorus->cfg;
                    cfg.mix = mParams.Get<PercentParam>(first + static_cast<clap_id>(ToneParams::ChorusMix));
                    cfg.mix = mParams.Get<PercentParam>(first + static_cast<clap_id>(ToneParams::ChorusRate));
                    cfg.mix = mParams.Get<PercentParam>(first + static_cast<clap_id>(ToneParams::ChorusDepth));
                }
                {
                    auto& cfg = tone.mEq.cfg;
                    cfg.lowFreq = mParams.Get<NumericParam>(first + static_cast<clap_id>(ToneParams::EqLowFrequency));
                    cfg.lowGainDb = mParams.Get<NumericParam>(first + static_cast<clap_id>(ToneParams::EqLowGain));
                    cfg.highFreq = mParams.Get<NumericParam>(first + static_cast<clap_id>(ToneParams::EqHighFrequency));
                    cfg.highGainDb = mParams.Get<NumericParam>(first + static_cast<clap_id>(ToneParams::EqHighGain));
                    cfg.sampleRate = sr;
                }
                tone.mPartialMix = mParams.Get<PercentParam>(first + static_cast<clap_id>(ToneParams::PartialMix));
                first += 7;

                PartialParams(tone.mPartial1, first);
                tone.SetLfoRoute(
                    1, 2, mParams.Get<ModSourceParam>(first + static_cast<clap_id>(PartialParams::DutyLfoSource)));
                tone.SetLfoRoute(
                    1, 3, mParams.Get<ModSourceParam>(first + static_cast<clap_id>(PartialParams::FilterLfoSource)));
                tone.SetLfoRoute(
                    1, 4, mParams.Get<ModSourceParam>(first + static_cast<clap_id>(PartialParams::VcaLfoSource)));

                first += static_cast<clap_id>(PartialParams::Count);
                PartialParams(tone.mPartial2, first);
                tone.SetLfoRoute(
                    2, 2, mParams.Get<ModSourceParam>(first + static_cast<clap_id>(PartialParams::DutyLfoSource)));
                tone.SetLfoRoute(
                    2, 3, mParams.Get<ModSourceParam>(first + static_cast<clap_id>(PartialParams::FilterLfoSource)));
                tone.SetLfoRoute(
                    2, 4, mParams.Get<ModSourceParam>(first + static_cast<clap_id>(PartialParams::VcaLfoSource)));

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
                v.mAlgo =
                    mParams.Get<EnumParam<Voice::ToneAlgorithm>>(static_cast<clap_id>(GlobalParams::ToneAlgorithm));
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

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() override { mVoices.Reset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRatef = static_cast<float>(sampleRate);
    }

   private:
    clapeze::VoicePool<Processor, Voice, cMaxVoices> mVoices;
    SampleLoader::AudioHandle& mSampleLoader;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, clapeze::BasePlugin& plugin, ParamsFeature& params, SampleLoader& sampleLoader)
        : kitgui::BaseApp(ctx), mPlugin(plugin), mParams(params), mSampleLoader(sampleLoader), mPresetBrowser(plugin) {}
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
        // auto& paramsHandle = mParams.GetMainHandle();

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

        auto LfoParams = [&](clap_id first) {
            // kitgui::DebugParam(mParams, first + static_cast<clap_id>(LfoParams::Wave));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(LfoParams::Rate));
            // kitgui::DebugParam(mParams, first + static_cast<clap_id>(LfoParams::Delay));
            // kitgui::DebugParam(mParams, first + static_cast<clap_id>(LfoParams::Sync));
        };
        auto EnvParams = [&](clap_id first) {
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(EnvParams::Attack));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(EnvParams::Decay));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(EnvParams::Sustain));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(EnvParams::Release));
        };
        auto PartialParams = [&](clap_id first) {
            ImGui::Indent();
            ImGui::SeparatorText("Wave Generator");
            kitgui::DebugEnumParam<Partial::Wave>(mParams, first + static_cast<clap_id>(PartialParams::Wave));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::PitchOffset));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::PitchEnvMult));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::PitchLfoMult));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::Duty));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::DutyLfoSource));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::DutyLfoMult));

            ImGui::SeparatorText("Filter");
            ImGui::PushID("vcf");
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::FilterNote));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::FilterTrackingMult));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::FilterEnvMult));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::FilterLfoSource));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::FilterLfoMult));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::FilterRes));
            EnvParams(first + 16);  // filter env
            ImGui::PopID();

            ImGui::SeparatorText("Volume");
            ImGui::PushID("vca");
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::VcaMult));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::VcaLfoSource));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(PartialParams::VcaLfoMult));
            EnvParams(first + 16 + static_cast<clap_id>(EnvParams::Count));  // volume env
            ImGui::PopID();
            ImGui::Unindent();
        };
        auto ToneParams = [&](clap_id first) {
            ImGui::Indent();
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::ChorusMix));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::ChorusRate));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::ChorusDepth));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::EqLowFrequency));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::EqLowGain));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::EqHighFrequency));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::EqHighGain));
            kitgui::DebugParam(mParams, first + static_cast<clap_id>(ToneParams::PartialMix));
            first += 7;
            if (ImGui::BeginTabBar("part")) {
                if (ImGui::BeginTabItem("Partial 1")) {
                    PartialParams(first);  // part1
                    ImGui::EndTabItem();
                }
                first += static_cast<clap_id>(PartialParams::Count);
                if (ImGui::BeginTabItem("Partial 2")) {
                    PartialParams(first);  // part2
                    ImGui::EndTabItem();
                }
                first += static_cast<clap_id>(PartialParams::Count);
                ImGui::EndTabBar();
            }

            if (ImGui::BeginTabBar("lfo")) {
                if (ImGui::BeginTabItem("LFO 1")) {
                    LfoParams(first);  // lfo1
                    ImGui::EndTabItem();
                }
                first += static_cast<clap_id>(LfoParams::Count);
                if (ImGui::BeginTabItem("LFO 2")) {
                    LfoParams(first);  // lfo2
                    ImGui::EndTabItem();
                }
                first += static_cast<clap_id>(LfoParams::Count);
                if (ImGui::BeginTabItem("LFO 3")) {
                    LfoParams(first);  // lfo3
                    ImGui::EndTabItem();
                }
                first += static_cast<clap_id>(LfoParams::Count);
                ImGui::EndTabBar();
            }
            ImGui::SeparatorText("Pitch Envelope");
            ImGui::PushID("pitchenv");
            EnvParams(first);  // pitchEnv
            ImGui::PopID();
            ImGui::Unindent();
        };
        auto GlobalParams = [&]() {
            kitgui::DebugParam(mParams, static_cast<clap_id>(GlobalParams::Tune));
            kitgui::DebugEnumParam<Voice::ToneAlgorithm>(mParams, static_cast<clap_id>(GlobalParams::ToneAlgorithm));
            mSampleLoader.OnImGui();
            if (ImGui::BeginTabBar("tone")) {
                if (ImGui::BeginTabItem("Tone 1")) {
                    ToneParams(2);  // tone 1
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tone 2")) {
                    ToneParams(2 + static_cast<clap_id>(ToneParams::Count));  // tone 2
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        };

        GlobalParams();
    }

    void OnGuiUpdate() override {}

    void OnDraw() override {}

   private:
    clapeze::BasePlugin& mPlugin;
    ParamsFeature& mParams;
    SampleLoader& mSampleLoader;
    kitgui::PresetBrowser mPresetBrowser;
    bool mDebugMode = false;
    bool mShowHelpWindow = false;
};
#endif

class StateFeature : public TomlStateFeature<ParamsFeature> {
   public:
    StateFeature(BasePlugin& self, SampleLoader& sampleLoader)
        : TomlStateFeature(self, 0), mSampleLoader(sampleLoader) {}
    bool OnSave(toml::table& t) const override {
        (void)t;
        return true;
    }
    bool OnLoad(const toml::table& t) override {
        (void)t;
        return true;
    }

   private:
    SampleLoader& mSampleLoader;
};

class Plugin : public InstrumentPlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(const clap_plugin_descriptor_t& meta) : InstrumentPlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        InstrumentPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), static_cast<clap_id>(GlobalParams::Count));
        clap_id idx = 0;
        auto LfoParams = [&](const std::string& pre) {
            params.Parameter(idx++, new EnumParam<Partial::Wave>(pre + "_wave", "Wave", {"Pulse", "Saw", "PCM"},
                                                                 Partial::Wave::Pulse));
            params.Parameter(idx++, new LfoRateParam(pre + "_rate", "Rate"));
            params.Parameter(idx++, new PercentParam(pre + "_delay", "Delay", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_sync", "Sync", 0.0f));
        };
        auto EnvParams = [&](const std::string& pre) {
            params.Parameter(idx++, new EnvParam(pre + "_attack", "Attack", 1.0f, 1000.0f));
            params.Parameter(idx++, new EnvParam(pre + "_decay", "Decay", 10.0f, 30000.0f));
            params.Parameter(idx++, new PercentParam(pre + "_sustain", "Sustain", 1.0f));
            params.Parameter(idx++, new EnvParam(pre + "_release", "Release", 10.0f, 30000.0f));
        };
        auto PartialParams = [&](const std::string& pre) {
            params.Parameter(idx++, new EnumParam<Partial::Wave>(pre + "_wave", "Wave", {"Pulse", "Saw", "PCM"},
                                                                 Partial::Wave::Pulse));
            params.Parameter(idx++, new NumericParam(pre + "_pitch", "Pitch", -32.0f, 32.0f, 0.0f, "semis"));
            params.Parameter(idx++, new PercentParam(pre + "_pitchEnv", "Pitch Env amount", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_pitchLfo", "Pitch LFO amount", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_duty", "Duty", 0.5f));
            params.Parameter(idx++, new ModSourceParam(pre + "_dutySrc", "Duty Mod source", 1));
            params.Parameter(idx++, new PercentParam(pre + "_dutyMult", "Duty Mod amount", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_filter", "Filter Cutoff", 1.0f));
            params.Parameter(idx++, new PercentParam(pre + "_filterTrack", "Filter Tracking", 0.5f));
            params.Parameter(idx++, new PercentParam(pre + "_filterEnvMult", "Filter Env amount", 0.0f));
            params.Parameter(idx++, new ModSourceParam(pre + "_filterSrc", "Filter Mod source", 2));
            params.Parameter(idx++, new PercentParam(pre + "_filterSrcMult", "Filter Mod amount", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_filterRes", "Filter Resonance", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_vcaMult", "Volume", 0.5f));
            params.Parameter(idx++, new ModSourceParam(pre + "_vcaLfoSource", "Volume Mod source", 3));
            params.Parameter(idx++, new PercentParam(pre + "_vcaLfoMult", "Volume Mod amount", 0.0f));
            EnvParams(pre + "FilterEnv");
            EnvParams(pre + "volumeEnv");
        };
        auto ToneParams = [&](const std::string& pre) {
            params.Parameter(idx++, new PercentParam(pre + "_chorusMix", "Chorus Mix", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_chorusRate", "Chorus Rate", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_chorusDepth", "Chorus Depth", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_chorusRate", "Chorus Rate", 0.0f));
            params.Parameter(idx++,
                             new NumericParam(pre + "_eqLowFreq", "Low Frequency", 20.0f, 20000.0f, 300.0f, "hz"));
            params.Parameter(idx++, new NumericParam(pre + "_eqLowGain", "Low Gain", -60.0f, 10.0f, 0.0f, "db"));
            params.Parameter(idx++,
                             new NumericParam(pre + "_eqHighFreq", "High Frequency", 20.0f, 20000.0f, 1800.0f, "hz"));
            params.Parameter(idx++, new NumericParam(pre + "_eqHighGain", "High Gain", -60.0f, 10.0f, 0.0f, "db"));
            params.Parameter(idx++, new PercentParam(pre + "_partialMix", "Partial Mix", 0.5f));
            PartialParams(pre + "Part1");
            PartialParams(pre + "Part2");
            LfoParams(pre + "Lfo1");
            LfoParams(pre + "Lfo2");
            LfoParams(pre + "Lfo3");
            EnvParams(pre + "PitchEnv");
        };
        auto GlobalParams = [&]() {
            params.Parameter(idx++, new NumericParam("tune", "Tune", 20.0f, 1000.0f, 440.0f, "hz"));
            params.Parameter(idx++,
                             new EnumParam<Voice::ToneAlgorithm>("toneAlgo", "Routing", {"1 only", "2 only", "both"},
                                                                 Voice::ToneAlgorithm::OneOnly));
            ToneParams("Tone1");
            ToneParams("Tone2");
        };

        GlobalParams();

        ConfigFeature<clapeze::AssetsFeature>();
        ConfigFeature<StateFeature>(*this, mSampleLoader);
        ConfigFeature<clapeze::PresetFeature>(*this);

#if KITSBLIPS_ENABLE_GUI
        // aspect ratio 1.5
        kitgui::SizeConfig cfg{750, 500, false, true};
        ConfigFeature<KitguiFeature>(
            GetHost(),
            [this, &params](kitgui::Context& ctx) {
                return std::make_unique<GuiApp>(ctx, *this, params, mSampleLoader);
            },
            cfg);
#endif

        ConfigProcessor<Processor>(params.GetAudioHandle<ParamsFeature::AudioHandle>(), mSampleLoader.GetAudioHandle());
    }

   private:
    SampleLoader mSampleLoader;
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioInstrumentDescriptor("kitsblips.layersynth",
                                                  "layersynth",
                                                  "A Linear-Arithmetic inspired polysynth"));
}  // namespace layersynth
