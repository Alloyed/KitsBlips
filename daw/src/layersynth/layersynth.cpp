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
#include <misc/cpp/imgui_stdlib.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#include "gui/presetBrowser.h"
#endif

namespace {

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
    Count = 8u + static_cast<clap_id>(PartialParams::Count) * 2 + static_cast<clap_id>(LfoParams::Count) * 3 +
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
                mSamplers[pair.first] = {pair.second->samples[0]};
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
            if (ImGui::InputText(fmt::format("Sample {}", i).c_str(), &mSamplePaths[i],
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                LoadSample(i, mSamplePaths[i]);
            }
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
        : InstrumentProcessor(params), mSynth(*this), mSampleLoader(sampleLoader) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
#define XID(enum) (first + static_cast<clap_id>(enum))
        float sr = static_cast<float>(GetSampleRate());
        mSynth.mVoices.ForEach([&](Voice& v) {
            auto LfoParams = [&](kitdsp::lfo::TriangleOscillator& lfo, clap_id first) {
                // float wave = mParams.Get<PercentParam>(XID(LfoParams::Wave));
                float rate = mParams.Get<LfoRateParam>(XID(LfoParams::Rate));
                // float delay = mParams.Get<PercentParam>(XID(LfoParams::Delay));
                // float sync = mParams.Get<PercentParam>(XID(LfoParams::Sync));
                lfo.SetFrequency(rate, sr);
            };
            auto EnvParams = [&](kitdsp::ApproachAdsr& adsr, clap_id first) {
                float a = mParams.Get<EnvParam>(XID(EnvParams::Attack));
                float d = mParams.Get<EnvParam>(XID(EnvParams::Decay));
                float s = mParams.Get<PercentParam>(XID(EnvParams::Sustain));
                float r = mParams.Get<EnvParam>(XID(EnvParams::Release));
                adsr.SetParams(a, d, s, r, sr);
            };
            auto PartialParams = [&](Partial& part, clap_id first) {
                part.mPcmSampler = mSampleLoader.GetSampler(0);
                part.sr = sr;
                part.mWave = mParams.Get<EnumParam<Partial::Wave>>(XID(PartialParams::Wave));
                part.mNoteOffset = mParams.Get<NumericParam>(XID(PartialParams::PitchOffset));
                part.mTune = mParams.Get<NumericParam>(static_cast<clap_id>(GlobalParams::Tune));
                part.mPitchEnvMult = mParams.Get<PercentParam>(XID(PartialParams::PitchEnvMult));
                part.mPitchLfoMult = mParams.Get<PercentParam>(XID(PartialParams::PitchLfoMult));
                part.mDuty = mParams.Get<PercentParam>(XID(PartialParams::Duty));
                part.mDutyLfoMult = mParams.Get<PercentParam>(XID(PartialParams::DutyLfoMult));
                part.mFilterNote = mParams.Get<PercentParam>(XID(PartialParams::FilterNote)) * 80.0f;
                part.mFilterTrackingMult = mParams.Get<PercentParam>(XID(PartialParams::FilterTrackingMult));
                part.mFilterEnvMult = mParams.Get<PercentParam>(XID(PartialParams::FilterEnvMult));
                part.mFilterLfoMult = mParams.Get<PercentParam>(XID(PartialParams::FilterLfoMult));
                part.mFilterRes = mParams.Get<PercentParam>(XID(PartialParams::FilterRes));
                part.mVcaMult = mParams.Get<PercentParam>(XID(PartialParams::VcaMult));
                part.mVcaLfoMult = mParams.Get<PercentParam>(XID(PartialParams::VcaLfoMult));
                first += 16;
                EnvParams(part.mFilterEnv, first);
                first += static_cast<clap_id>(EnvParams::Count);
                EnvParams(part.mVolumeEnv, first);
            };
            auto ToneParams = [&](Tone& tone, clap_id first) {
                if (tone.mChorus) {
                    auto& cfg = tone.mChorus->cfg;
                    cfg.mix = mParams.Get<PercentParam>(XID(ToneParams::ChorusMix));
                    cfg.mix = mParams.Get<PercentParam>(XID(ToneParams::ChorusRate));
                    cfg.mix = mParams.Get<PercentParam>(XID(ToneParams::ChorusDepth));
                }
                {
                    auto& cfg = tone.mEq.cfg;
                    cfg.lowFreq = mParams.Get<NumericParam>(XID(ToneParams::EqLowFrequency));
                    cfg.lowGainDb = mParams.Get<NumericParam>(XID(ToneParams::EqLowGain));
                    cfg.highFreq = mParams.Get<NumericParam>(XID(ToneParams::EqHighFrequency));
                    cfg.highGainDb = mParams.Get<NumericParam>(XID(ToneParams::EqHighGain));
                    cfg.sampleRate = sr;
                }
                tone.mPartialMix = mParams.Get<PercentParam>(XID(ToneParams::PartialMix));
                first += 8;

                PartialParams(tone.mPartial1, first);
                tone.SetLfoRoute(1, 2, mParams.Get<ModSourceParam>(XID(PartialParams::DutyLfoSource)));
                tone.SetLfoRoute(1, 3, mParams.Get<ModSourceParam>(XID(PartialParams::FilterLfoSource)));
                tone.SetLfoRoute(1, 4, mParams.Get<ModSourceParam>(XID(PartialParams::VcaLfoSource)));

                first += static_cast<clap_id>(PartialParams::Count);
                PartialParams(tone.mPartial2, first);
                tone.SetLfoRoute(2, 2, mParams.Get<ModSourceParam>(XID(PartialParams::DutyLfoSource)));
                tone.SetLfoRoute(2, 3, mParams.Get<ModSourceParam>(XID(PartialParams::FilterLfoSource)));
                tone.SetLfoRoute(2, 4, mParams.Get<ModSourceParam>(XID(PartialParams::VcaLfoSource)));

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
                clap_id first = 0;
                v.mAlgo = mParams.Get<EnumParam<Voice::ToneAlgorithm>>(XID(GlobalParams::ToneAlgorithm));
                ToneParams(v.mTone1, 2);
                ToneParams(v.mTone2, 2 + static_cast<clap_id>(ToneParams::Count));
            };

            GlobalParams();
#undef XID
        });
        return mSynth.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mSynth.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mSynth.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mSynth.ProcessNoteChoke(note); }

    void ProcessReset() override { mSynth.ProcessReset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        (void)minBlockSize;
        (void)maxBlockSize;
        (void)sampleRate;
    }

   private:
    SynthProcessor<Processor> mSynth;
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
            ImGui::EndMainMenuBar();
        }

#define XID(enum) (first + static_cast<clap_id>(enum))
        auto LfoParams = [&](clap_id first) {
            // kitgui::DebugParam(mParams, XID(LfoParams::Wave));
            kitgui::DebugParam(mParams, XID(LfoParams::Rate));
            // kitgui::DebugParam(mParams, XID(LfoParams::Delay));
            // kitgui::DebugParam(mParams, XID(LfoParams::Sync));
        };
        auto EnvParams = [&](clap_id first) {
            kitgui::DebugParam(mParams, XID(EnvParams::Attack));
            kitgui::DebugParam(mParams, XID(EnvParams::Decay));
            kitgui::DebugParam(mParams, XID(EnvParams::Sustain));
            kitgui::DebugParam(mParams, XID(EnvParams::Release));
        };
        auto PartialParams = [&](clap_id first) {
            ImGui::Indent();
            ImGui::SeparatorText("Wave Generator");
            kitgui::DebugEnumParam<Partial::Wave>(mParams, XID(PartialParams::Wave));
            kitgui::DebugParam(mParams, XID(PartialParams::PitchOffset));
            kitgui::DebugParam(mParams, XID(PartialParams::PitchEnvMult));
            kitgui::DebugParam(mParams, XID(PartialParams::PitchLfoMult));
            kitgui::DebugParam(mParams, XID(PartialParams::Duty));
            kitgui::DebugParam(mParams, XID(PartialParams::DutyLfoSource));
            kitgui::DebugParam(mParams, XID(PartialParams::DutyLfoMult));

            ImGui::SeparatorText("Filter");
            ImGui::PushID("vcf");
            kitgui::DebugParam(mParams, XID(PartialParams::FilterNote));
            kitgui::DebugParam(mParams, XID(PartialParams::FilterTrackingMult));
            kitgui::DebugParam(mParams, XID(PartialParams::FilterEnvMult));
            kitgui::DebugParam(mParams, XID(PartialParams::FilterLfoSource));
            kitgui::DebugParam(mParams, XID(PartialParams::FilterLfoMult));
            kitgui::DebugParam(mParams, XID(PartialParams::FilterRes));
            EnvParams(first + 16);  // filter env
            ImGui::PopID();

            ImGui::SeparatorText("Volume");
            ImGui::PushID("vca");
            kitgui::DebugParam(mParams, XID(PartialParams::VcaMult));
            kitgui::DebugParam(mParams, XID(PartialParams::VcaLfoSource));
            kitgui::DebugParam(mParams, XID(PartialParams::VcaLfoMult));
            EnvParams(first + 16 + static_cast<clap_id>(EnvParams::Count));  // volume env
            ImGui::PopID();
            ImGui::Unindent();
        };
        auto ToneParams = [&](clap_id first) {
            ImGui::Indent();
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusMix));
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusRate));
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusDepth));
            kitgui::DebugParam(mParams, XID(ToneParams::EqLowFrequency));
            kitgui::DebugParam(mParams, XID(ToneParams::EqLowGain));
            kitgui::DebugParam(mParams, XID(ToneParams::EqHighFrequency));
            kitgui::DebugParam(mParams, XID(ToneParams::EqHighGain));
            kitgui::DebugParam(mParams, XID(ToneParams::PartialMix));
            first += 8;
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
#undef XID
    }

    void OnGuiUpdate() override {}

    void OnDraw() override {}

   private:
    clapeze::BasePlugin& mPlugin;
    ParamsFeature& mParams;
    SampleLoader& mSampleLoader;
    kitgui::PresetBrowser mPresetBrowser;
};
#endif

class StateFeature : public TomlStateFeature<ParamsFeature> {
   public:
    StateFeature(BasePlugin& self, SampleLoader& sampleLoader)
        : TomlStateFeature(self, 0), mSampleLoader(sampleLoader) {}
    bool OnSave(toml::table& file) const override {
        toml::table sampleskv{};
        for (size_t idx = 0; idx < 4; ++idx) {
            sampleskv.insert(fmt::format("sample_{}", idx), mSampleLoader.GetSamplePath(idx));
        }
        file.insert("samples", sampleskv);
        return true;
    }
    bool OnLoad(const toml::table& file) override {
        auto sampleskv = file["samples"].as_table();
        if (sampleskv) {
            for (size_t idx = 0; idx < 4; ++idx) {
                std::string path = sampleskv->get(fmt::format("sample_{}", idx))->value_or("");
                mSampleLoader.LoadSample(idx, path);
            }
        }
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
