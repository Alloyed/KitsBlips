#include <clap/ext/params.h>
#include <clap/id.h>
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

#include "kitdsp/apps/psxReverbPresets.h"
#include "kitdsp/control/adsr.h"
#include "kitdsp/control/lfo.h"
#include "kitdsp/macros.h"
#include "kitdsp/sampler.h"
#include "kitgui/wrap_nfd.h"
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
    Count,
};
enum class EnvParams : clap_id {
    Attack,
    Decay,
    Sustain,
    Release,
    Count,
};
enum class PartialParams : clap_id {
    Wave,
    PitchCoarse,
    PitchFine,
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
    FilterEnvStart,
    // + volumeEnv
    VolumeEnvStart = FilterEnvStart + static_cast<clap_id>(EnvParams::Count),
    Count = VolumeEnvStart + static_cast<clap_id>(EnvParams::Count),
};
enum class ToneParams {
    ChorusMix,
    ChorusRate,
    ChorusDelay,
    ChorusDelayMod,
    ChorusFeedback,
    EqLowFrequency,
    EqLowGain,
    EqHighFrequency,
    EqHighGain,
    PartialMix,
    // + partial1
    Partial1Start,
    // + partial2
    Partial2Start = Partial1Start + static_cast<clap_id>(PartialParams::Count),
    // + lfo1
    Lfo1Start = Partial2Start + static_cast<clap_id>(PartialParams::Count),
    // + lfo2
    Lfo2Start = Lfo1Start + static_cast<clap_id>(LfoParams::Count),
    // + lfo3
    Lfo3Start = Lfo2Start + static_cast<clap_id>(LfoParams::Count),
    // + pitchEnv
    PitchEnvStart = Lfo3Start + static_cast<clap_id>(LfoParams::Count),
    Count = PitchEnvStart + static_cast<clap_id>(EnvParams::Count),
};

enum class GlobalParams {
    Tune,
    ToneAlgorithm,
    ReverbMix,
    ReverbPreset,
    // + tone1
    Tone1Start,
    // + tone2
    Tone2Start = Tone1Start + static_cast<clap_id>(ToneParams::Count),
    Count = Tone2Start + static_cast<clap_id>(ToneParams::Count),
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

struct FreqParam : public clapeze::NumericParam {
    FreqParam(std::string_view key, std::string_view name, float min, float max, float defaultV)
        : clapeze::NumericParam(key, name, min, max, defaultV, "hz") {
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
        bool OnAudioUpdate() {
            bool changed = false;
            std::pair<size_t, File*> pair;
            while (mMainToAudio.pop(pair)) {
                if (mSampleData[pair.first]) {
                    ReleaseSample(pair.first);
                }
                mSampleData[pair.first] = pair.second;
                mSamplers[pair.first] = std::make_optional<kitdsp::Sampler1D<float>>(
                    pair.second->samples[0], narrow_cast<float>(pair.second->getSampleRate()));
                changed = true;
            }
            return changed;
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

    void OnImGui(kitgui::Context& ctx) {
        for (size_t i = 0; i < TNumSamples; ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InputText(fmt::format("Sample {}", i).c_str(), &mSamplePaths[i],
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                LoadSample(i, mSamplePaths[i]);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                nfdu8char_t* outPath{};
                nfdu8filteritem_t filters[2] = {{"Audio", "wav"}};
                nfdopendialogu8args_t args{};
                args.filterList = filters;
                args.filterCount = 1;
                args.parentWindow = NFD_GetWindow(ctx.GetWindow());
                nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
                if (result == NFD_OKAY) {
                    LoadSample(i, outPath);
                    NFD_FreePathU8(outPath);
                } else if (result == NFD_CANCEL) {
                    mSampleStatus[i] = "<canceled>";
                } else {
                    mSampleStatus[i] = NFD_GetError();
                }
            }
            // ImGui::Text();
            ImGui::PopID();
        }
    }

   private:
    Queue mMainToAudio{};
    Queue mAudioToMain{};
    AudioHandle mAudioHandle;
    etl::array<std::string, TNumSamples> mSamplePaths{};
    etl::array<std::string, TNumSamples> mSampleStatus{};
};
using SampleLoader = RawSampleLoader<4>;

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::AudioHandle> {
   public:
    explicit Processor(ParamsFeature::AudioHandle& params, SampleLoader::AudioHandle& sampleLoader)
        : InstrumentProcessor(params), mSynth(*this), mSampleLoader(sampleLoader) {
        params.RegisterHandler([&](clap_id id) {
            auto HandleLfo = [&](kitdsp::lfo::TriangleOscillator& lfo, clap_id inner) {
                if (inner == static_cast<clap_id>(LfoParams::Rate)) {
                    float rate = mParams.Get<LfoRateParam>(id);
                    lfo.SetFrequency(rate, static_cast<float>(GetSampleRate()));
                }
            };
            auto HandleEnv = [&](kitdsp::ApproachAdsr& adsr, clap_id inner) {
                clap_id start = id - inner;
                float a = mParams.Get<EnvParam>(start);
                float d = mParams.Get<EnvParam>(start + 1);
                float s = mParams.Get<PercentParam>(start + 2);
                float r = mParams.Get<EnvParam>(start + 3);
                adsr.SetParams(a, d, s, r, static_cast<float>(GetSampleRate()));
            };
            auto HandlePartial = [&](Tone& tone, Partial& part, clap_id inner) {
                clap_id start = id - inner;
                if (inner == static_cast<clap_id>(PartialParams::Wave)) {
                    part.mWave = mParams.Get<EnumParam<Partial::Wave>>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::PitchCoarse) ||
                           inner == static_cast<clap_id>(PartialParams::PitchFine)) {
                    part.mNoteOffset =
                        mParams.Get<IntegerParam>(start + static_cast<clap_id>(PartialParams::PitchCoarse)) +
                        mParams.Get<NumericParam>(start + static_cast<clap_id>(PartialParams::PitchFine)) / 100.0f;
                } else if (inner == static_cast<clap_id>(PartialParams::PitchEnvMult)) {
                    part.mPitchEnvMult = mParams.Get<NumericParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::PitchLfoMult)) {
                    part.mPitchLfoMult = mParams.Get<NumericParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::Duty)) {
                    part.mDuty = mParams.Get<PercentParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::DutyLfoSource)) {
                    tone.SetLfoRoute(part.mNumber, 2, mParams.Get<ModSourceParam>(id));
                } else if (inner == static_cast<clap_id>(PartialParams::DutyLfoMult)) {
                    part.mDutyLfoMult = mParams.Get<PercentParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::FilterNote)) {
                    part.mFilterNote = mParams.Get<PercentParam>(id) * 80.0f;
                } else if (inner == static_cast<clap_id>(PartialParams::FilterTrackingMult)) {
                    part.mFilterTrackingMult = mParams.Get<PercentParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::FilterEnvMult)) {
                    part.mFilterEnvMult = mParams.Get<PercentParam>(id) * 80.0f;
                } else if (inner == static_cast<clap_id>(PartialParams::FilterLfoSource)) {
                    tone.SetLfoRoute(part.mNumber, 3, mParams.Get<ModSourceParam>(id));
                } else if (inner == static_cast<clap_id>(PartialParams::FilterLfoMult)) {
                    part.mFilterLfoMult = mParams.Get<PercentParam>(id) * 80.0f;
                } else if (inner == static_cast<clap_id>(PartialParams::FilterRes)) {
                    part.mFilterRes = mParams.Get<PercentParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::VcaMult)) {
                    part.mVcaMult = mParams.Get<PercentParam>(id);
                } else if (inner == static_cast<clap_id>(PartialParams::VcaLfoSource)) {
                    tone.SetLfoRoute(part.mNumber, 4, mParams.Get<ModSourceParam>(id));
                } else if (inner == static_cast<clap_id>(PartialParams::VcaLfoMult)) {
                    part.mVcaLfoMult = mParams.Get<PercentParam>(id);
                } else if (inner >= static_cast<clap_id>(PartialParams::FilterEnvStart) &&
                           inner < static_cast<clap_id>(PartialParams::VolumeEnvStart)) {
                    HandleEnv(part.mFilterEnv, inner - static_cast<clap_id>(PartialParams::FilterEnvStart));
                } else if (inner >= static_cast<clap_id>(PartialParams::VolumeEnvStart) &&
                           inner < static_cast<clap_id>(PartialParams::Count)) {
                    HandleEnv(part.mVolumeEnv, inner - static_cast<clap_id>(PartialParams::VolumeEnvStart));
                }
            };
            auto HandleTone = [&](Tone& tone, clap_id inner) {
                if (inner == static_cast<clap_id>(ToneParams::ChorusMix)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.mix = mParams.Get<PercentParam>(id);
                    }
                } else if (inner == static_cast<clap_id>(ToneParams::ChorusRate)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.lfoRateHz = mParams.Get<NumericParam>(id);
                    }
                } else if (inner == static_cast<clap_id>(ToneParams::ChorusDelay)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.delayBaseMs = mParams.Get<NumericParam>(id);
                    }
                } else if (inner == static_cast<clap_id>(ToneParams::ChorusDelayMod)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.delayModMs = mParams.Get<NumericParam>(id);
                    }
                } else if (inner == static_cast<clap_id>(ToneParams::ChorusFeedback)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.feedback = mParams.Get<PercentParam>(id);
                    }
                } else if (inner == static_cast<clap_id>(ToneParams::EqLowFrequency)) {
                    tone.mEq.cfg.lowFreq = mParams.Get<NumericParam>(id);
                } else if (inner == static_cast<clap_id>(ToneParams::EqLowGain)) {
                    tone.mEq.cfg.lowGainDb = mParams.Get<NumericParam>(id);
                } else if (inner == static_cast<clap_id>(ToneParams::EqHighFrequency)) {
                    tone.mEq.cfg.highFreq = mParams.Get<NumericParam>(id);
                } else if (inner == static_cast<clap_id>(ToneParams::EqHighGain)) {
                    tone.mEq.cfg.highGainDb = mParams.Get<NumericParam>(id);
                } else if (inner == static_cast<clap_id>(ToneParams::PartialMix)) {
                    tone.mPartialMix = mParams.Get<PercentParam>(id);
                } else if (inner >= static_cast<clap_id>(ToneParams::Partial1Start) &&
                           inner < static_cast<clap_id>(ToneParams::Partial2Start)) {
                    HandlePartial(tone, tone.mPartial1, inner - static_cast<clap_id>(ToneParams::Partial1Start));
                } else if (inner >= static_cast<clap_id>(ToneParams::Partial2Start) &&
                           inner < static_cast<clap_id>(ToneParams::Lfo1Start)) {
                    HandlePartial(tone, tone.mPartial2, inner - static_cast<clap_id>(ToneParams::Partial2Start));
                } else if (inner >= static_cast<clap_id>(ToneParams::Lfo1Start) &&
                           inner < static_cast<clap_id>(ToneParams::Lfo2Start)) {
                    HandleLfo(tone.mLfo1, inner - static_cast<clap_id>(ToneParams::Lfo1Start));
                } else if (inner >= static_cast<clap_id>(ToneParams::Lfo2Start) &&
                           inner < static_cast<clap_id>(ToneParams::Lfo3Start)) {
                    HandleLfo(tone.mLfo2, inner - static_cast<clap_id>(ToneParams::Lfo2Start));
                } else if (inner >= static_cast<clap_id>(ToneParams::Lfo3Start) &&
                           inner < static_cast<clap_id>(ToneParams::PitchEnvStart)) {
                    HandleLfo(tone.mLfo3, inner - static_cast<clap_id>(ToneParams::Lfo3Start));
                } else if (inner >= static_cast<clap_id>(ToneParams::Lfo3Start) &&
                           inner < static_cast<clap_id>(ToneParams::Count)) {
                    HandleEnv(tone.mPitchEnv, inner - static_cast<clap_id>(ToneParams::PitchEnvStart));
                }
            };

            auto HandleGlobal = [&](Voice& voice, clap_id inner) {
                if (inner == static_cast<clap_id>(GlobalParams::Tune)) {
                    float tune = mParams.Get<NumericParam>(id);
                    voice.mTone1.mPartial1.mTune = tune;
                    voice.mTone1.mPartial2.mTune = tune;
                    voice.mTone2.mPartial1.mTune = tune;
                    voice.mTone2.mPartial2.mTune = tune;
                } else if (inner == static_cast<clap_id>(GlobalParams::ToneAlgorithm)) {
                    voice.mAlgo = mParams.Get<EnumParam<Voice::ToneAlgorithm>>(id);
                } else if (inner == static_cast<clap_id>(GlobalParams::ReverbMix)) {
                    mSynth.mReverbMix = mParams.Get<PercentParam>(id);
                } else if (inner == static_cast<clap_id>(GlobalParams::ReverbPreset)) {
                    if (mSynth.mReverb) {
                        mSynth.mReverb->cfg.preset = narrow_cast<int16_t>(mParams.Get<IntegerParam>(id));
                    }
                } else if (inner >= static_cast<clap_id>(GlobalParams::Tone1Start) &&
                           inner < static_cast<clap_id>(GlobalParams::Tone2Start)) {
                    HandleTone(voice.mTone1, inner - static_cast<clap_id>(GlobalParams::Tone1Start));
                } else if (inner >= static_cast<clap_id>(GlobalParams::Tone2Start) &&
                           inner < static_cast<clap_id>(GlobalParams::Count)) {
                    HandleTone(voice.mTone2, inner - static_cast<clap_id>(GlobalParams::Tone2Start));
                }
            };
            mSynth.mVoices.ForEach([&](Voice& v) { HandleGlobal(v, id); });
        });
    }
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        if (mSampleLoader.OnAudioUpdate()) {
            mSynth.mVoices.ForEach([this](Voice& voice) {
                voice.mTone1.mPartial1.mPcmSampler = mSampleLoader.GetSampler(0);
                voice.mTone1.mPartial2.mPcmSampler = mSampleLoader.GetSampler(1);
                voice.mTone2.mPartial1.mPcmSampler = mSampleLoader.GetSampler(2);
                voice.mTone2.mPartial2.mPcmSampler = mSampleLoader.GetSampler(3);
            });
        }
        return mSynth.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mSynth.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mSynth.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mSynth.ProcessNoteChoke(note); }

    void ProcessReset() override { mSynth.ProcessReset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        mSynth.Activate(sampleRate, minBlockSize, maxBlockSize);
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
        // float scale = static_cast<float>(GetContext().GetUIScale());
        //  TODO: to update all this if the viewport changes
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
            kitgui::DebugParam(mParams, XID(PartialParams::PitchCoarse));
            kitgui::DebugParam(mParams, XID(PartialParams::PitchFine));
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
            EnvParams(XID(PartialParams::FilterEnvStart));  // filter env
            ImGui::PopID();

            ImGui::SeparatorText("Volume");
            ImGui::PushID("vca");
            kitgui::DebugParam(mParams, XID(PartialParams::VcaMult));
            kitgui::DebugParam(mParams, XID(PartialParams::VcaLfoSource));
            kitgui::DebugParam(mParams, XID(PartialParams::VcaLfoMult));
            EnvParams(XID(PartialParams::VolumeEnvStart));  // volume env
            ImGui::PopID();
            ImGui::Unindent();
        };
        auto ToneParams = [&](clap_id first) {
            ImGui::Indent();
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusMix));
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusRate));
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusDelay));
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusDelayMod));
            kitgui::DebugParam(mParams, XID(ToneParams::ChorusFeedback));
            kitgui::DebugParam(mParams, XID(ToneParams::EqLowFrequency));
            kitgui::DebugParam(mParams, XID(ToneParams::EqLowGain));
            kitgui::DebugParam(mParams, XID(ToneParams::EqHighFrequency));
            kitgui::DebugParam(mParams, XID(ToneParams::EqHighGain));
            kitgui::DebugParam(mParams, XID(ToneParams::PartialMix));
            if (ImGui::BeginTabBar("part")) {
                if (ImGui::BeginTabItem("Partial 1")) {
                    PartialParams(XID(ToneParams::Partial1Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Partial 2")) {
                    PartialParams(XID(ToneParams::Partial2Start));
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            if (ImGui::BeginTabBar("lfo")) {
                if (ImGui::BeginTabItem("LFO 1")) {
                    LfoParams(XID(ToneParams::Lfo1Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("LFO 2")) {
                    LfoParams(XID(ToneParams::Lfo2Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("LFO 3")) {
                    LfoParams(XID(ToneParams::Lfo3Start));
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::SeparatorText("Pitch Envelope");
            ImGui::PushID("pitchenv");
            EnvParams(XID(ToneParams::PitchEnvStart));
            ImGui::PopID();
            ImGui::Unindent();
        };
        auto GlobalParams = [&]() {
            kitgui::DebugParam(mParams, static_cast<clap_id>(GlobalParams::Tune));
            kitgui::DebugEnumParam<Voice::ToneAlgorithm>(mParams, static_cast<clap_id>(GlobalParams::ToneAlgorithm));
            kitgui::DebugParam(mParams, static_cast<clap_id>(GlobalParams::ReverbMix));
            kitgui::DebugParam(mParams, static_cast<clap_id>(GlobalParams::ReverbPreset));
            mSampleLoader.OnImGui(GetContext());
            if (ImGui::BeginTabBar("tone")) {
                if (ImGui::BeginTabItem("Tone 1")) {
                    ToneParams(static_cast<clap_id>(GlobalParams::Tone1Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tone 2")) {
                    ToneParams(static_cast<clap_id>(GlobalParams::Tone2Start));
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
            params.Parameter(idx++,
                             new IntegerParam(pre + "_pitchCoarse", "Pitch Coarse", -32.0f, 32.0f, 0.0f, "semis"));
            params.Parameter(idx++, new NumericParam(pre + "_pitchFine", "Pitch Fine", -100.0f, 100.0f, 0.0f, "cents"));
            params.Parameter(idx++,
                             new NumericParam(pre + "_pitchEnv", "Pitch Env amount", -32.0f, 32.0f, 0.0f, "semis"));
            params.Parameter(idx++,
                             new NumericParam(pre + "_pitchLfo", "Pitch LFO amount", -12.0f, 12.0f, 0.0f, "semis"));
            params.Parameter(idx++, new PercentParam(pre + "_duty", "Duty", 0.5f));
            params.Parameter(idx++, new ModSourceParam(pre + "_dutySrc", "Duty Mod source", 1));
            params.Parameter(idx++, new PercentParam(pre + "_dutyMult", "Duty Mod amount", 0.0f));
            params.Parameter(idx++, new PercentParam(pre + "_filter", "Filter Cutoff", 1.0f));
            params.Parameter(idx++, new PercentParam(pre + "_filterTrack", "Filter Tracking", 0.0f));
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
            params.Parameter(idx++, new FreqParam(pre + "_chorusRate", "Chorus Rate", 0.1f, 20.0f, 1.0f));
            params.Parameter(idx++, new NumericParam(pre + "_chorusDelay", "Chorus Delay", 4.0f, 20.0f, 4.0f, "ms"));
            params.Parameter(idx++, new NumericParam(pre + "_chorusDepth", "Chorus Depth", 2.0f, 20.0f, 2.0f, "ms"));
            params.Parameter(idx++, new PercentParam(pre + "_chorusFeedback", "Chorus Feedback", 0.0f));
            params.Parameter(idx++, new FreqParam(pre + "_eqLowFreq", "Low Frequency", 20.0f, 20000.0f, 300.0f));
            params.Parameter(idx++, new NumericParam(pre + "_eqLowGain", "Low Gain", -60.0f, 10.0f, 0.0f, "db"));
            params.Parameter(idx++, new FreqParam(pre + "_eqHighFreq", "High Frequency", 20.0f, 20000.0f, 1800.0f));
            params.Parameter(idx++, new NumericParam(pre + "_eqHighGain", "High Gain", -60.0f, 10.0f, 0.0f, "db"));
            params.Parameter(idx++, new PercentParam(pre + "_partialMix", "Partial Mix", 0.0f));
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
            params.Parameter(idx++, new PercentParam("reverbMix", "Reverb Mix", 0.0f));
            params.Parameter(idx++, new IntegerParam("reverbPreset", "Reverb Preset", 1, kitdsp::PSX::kNumPresets, 1));
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
