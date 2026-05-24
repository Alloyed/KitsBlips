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
#include <kitdsp/apps/psxReverbPresets.h>
#include <kitdsp/control/adsr.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/macros.h>
#include <kitdsp/sampler.h>
#include <memory>
#include <sstream>

#include "descriptor.h"
#include "shared/dr_flac.h"
#include "shared/dr_wav.h"

#include "layersynth/dsp.h"

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/features/assetsFeature.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <kitgui/app.h>
#include <kitgui/context.h>
#include <kitgui/gfx/scene.h>
#include <kitgui/wrap_nfd.h>
#include <misc/cpp/imgui_stdlib.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#include "gui/presetBrowser.h"
#endif

namespace {

#define P_(id) static_cast<clap_id>(id)
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
    VolumeEnvStart = FilterEnvStart + P_(EnvParams::Count),
    Count = VolumeEnvStart + P_(EnvParams::Count),
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
    Partial2Start = Partial1Start + P_(PartialParams::Count),
    // + lfo1
    Lfo1Start = Partial2Start + P_(PartialParams::Count),
    // + lfo2
    Lfo2Start = Lfo1Start + P_(LfoParams::Count),
    // + lfo3
    Lfo3Start = Lfo2Start + P_(LfoParams::Count),
    // + pitchEnv
    PitchEnvStart = Lfo3Start + P_(LfoParams::Count),
    Count = PitchEnvStart + P_(EnvParams::Count),
};
enum class GlobalParams {
    Tune,
    ToneAlgorithm,
    ReverbMix,
    ReverbPreset,
    // + tone1
    Tone1Start,
    // + tone2
    Tone2Start = Tone1Start + P_(ToneParams::Count),
    Count = Tone2Start + P_(ToneParams::Count),
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
    struct File {
        std::vector<float> samples;
        float sampleRate;
    };
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
                    pair.second->samples, narrow_cast<float>(pair.second->sampleRate));
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
        File* f = new File();
        if (path.ends_with(".flac")) {
            drflac* pFlac = drflac_open_file(path.c_str(), nullptr);
            if (pFlac == nullptr) {
                return;
            }

            size_t numSamples = pFlac->totalPCMFrameCount;
            f->samples.resize(numSamples);
            std::vector<float> raw(numSamples * pFlac->channels);
            size_t numSamplesRead = drflac_read_pcm_frames_f32(pFlac, numSamples, raw.data());
            if (numSamplesRead != numSamples) {
                return;
            }
            for (size_t idx = 0; idx < numSamples; ++idx) {
                // always take left channel for simplicity
                f->samples[idx] = raw[idx * pFlac->channels];
            }

            drflac_close(pFlac);
        } else if (path.ends_with(".wav")) {
            drwav wav;
            if (!drwav_init_file(&wav, path.c_str(), nullptr)) {
                return;
            }

            size_t numSamples = wav.totalPCMFrameCount;
            f->samples.resize(numSamples);
            std::vector<float> raw(numSamples * wav.channels);
            size_t numSamplesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, raw.data());
            if (numSamplesRead != numSamples) {
                return;
            }
            for (size_t idx = 0; idx < numSamples; ++idx) {
                // always take left channel for simplicity
                f->samples[idx] = raw[idx * wav.channels];
            }

            drwav_uninit(&wav);
        }

        if (f) {
            mMainToAudio.push({idx, f});
            mSamplePaths[idx] = path;
        }
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
        : InstrumentProcessor(params), mSynth(*this, params), mSampleLoader(sampleLoader) {
        static_assert(P_(GlobalParams::Count) == 156, "Update handlers");
        params.RegisterHandler([&](clap_id id) {
            auto HandleLfo = [&](kitdsp::lfo::TriangleOscillator& lfo, clap_id inner) {
                if (inner == P_(LfoParams::Rate)) {
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

                float sr = static_cast<float>(GetSampleRate());
                adsr.SetParams(a, d, s, r, sr);
            };
            auto HandlePartial = [&](Tone& tone, Partial& part, clap_id inner) {
                clap_id start = id - inner;
                if (inner == P_(PartialParams::Wave)) {
                    part.mWave = mParams.Get<EnumParam<Partial::Wave>>(id);
                } else if (inner == P_(PartialParams::PitchCoarse) || inner == P_(PartialParams::PitchFine)) {
                    part.mNoteOffset =
                        narrow_cast<float>(mParams.Get<IntegerParam>(start + P_(PartialParams::PitchCoarse))) +
                        mParams.Get<NumericParam>(start + P_(PartialParams::PitchFine)) / 100.0f;
                } else if (inner == P_(PartialParams::PitchEnvMult)) {
                    part.mPitchEnvMult = mParams.Get<NumericParam>(id);
                } else if (inner == P_(PartialParams::PitchLfoMult)) {
                    part.mPitchLfoMult = mParams.Get<NumericParam>(id);
                } else if (inner == P_(PartialParams::Duty)) {
                    part.mDuty = mParams.Get<PercentParam>(id);
                } else if (inner == P_(PartialParams::DutyLfoSource)) {
                    tone.SetLfoRoute(part.mNumber, 2, mParams.Get<ModSourceParam>(id));
                } else if (inner == P_(PartialParams::DutyLfoMult)) {
                    part.mDutyLfoMult = mParams.Get<PercentParam>(id);
                } else if (inner == P_(PartialParams::FilterNote)) {
                    part.mFilterNote = mParams.Get<PercentParam>(id) * 80.0f;
                } else if (inner == P_(PartialParams::FilterTrackingMult)) {
                    part.mFilterTrackingMult = mParams.Get<PercentParam>(id);
                } else if (inner == P_(PartialParams::FilterEnvMult)) {
                    part.mFilterEnvMult = mParams.Get<PercentParam>(id) * 80.0f;
                } else if (inner == P_(PartialParams::FilterLfoSource)) {
                    tone.SetLfoRoute(part.mNumber, 3, mParams.Get<ModSourceParam>(id));
                } else if (inner == P_(PartialParams::FilterLfoMult)) {
                    part.mFilterLfoMult = mParams.Get<PercentParam>(id) * 80.0f;
                } else if (inner == P_(PartialParams::FilterRes)) {
                    part.mFilterRes = mParams.Get<PercentParam>(id);
                } else if (inner == P_(PartialParams::VcaMult)) {
                    part.mVcaMult = mParams.Get<PercentParam>(id);
                } else if (inner == P_(PartialParams::VcaLfoSource)) {
                    tone.SetLfoRoute(part.mNumber, 4, mParams.Get<ModSourceParam>(id));
                } else if (inner == P_(PartialParams::VcaLfoMult)) {
                    part.mVcaLfoMult = mParams.Get<PercentParam>(id);
                } else if (inner >= P_(PartialParams::FilterEnvStart) && inner < P_(PartialParams::VolumeEnvStart)) {
                    HandleEnv(part.mFilterEnv, inner - P_(PartialParams::FilterEnvStart));
                } else if (inner >= P_(PartialParams::VolumeEnvStart) && inner < P_(PartialParams::Count)) {
                    HandleEnv(part.mVolumeEnv, inner - P_(PartialParams::VolumeEnvStart));
                }
            };
            auto HandleTone = [&](Tone& tone, clap_id inner) {
                if (inner == P_(ToneParams::ChorusMix)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.mix = mParams.Get<PercentParam>(id);
                    }
                } else if (inner == P_(ToneParams::ChorusRate)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.lfoRateHz = mParams.Get<NumericParam>(id);
                    }
                } else if (inner == P_(ToneParams::ChorusDelay)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.delayBaseMs = mParams.Get<NumericParam>(id);
                    }
                } else if (inner == P_(ToneParams::ChorusDelayMod)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.delayModMs = mParams.Get<NumericParam>(id);
                    }
                } else if (inner == P_(ToneParams::ChorusFeedback)) {
                    if (tone.mChorus) {
                        tone.mChorus->cfg.feedback = mParams.Get<PercentParam>(id);
                    }
                } else if (inner == P_(ToneParams::EqLowFrequency)) {
                    tone.mEq.cfg.lowFreq = mParams.Get<NumericParam>(id);
                } else if (inner == P_(ToneParams::EqLowGain)) {
                    tone.mEq.cfg.lowGainDb = mParams.Get<NumericParam>(id);
                } else if (inner == P_(ToneParams::EqHighFrequency)) {
                    tone.mEq.cfg.highFreq = mParams.Get<NumericParam>(id);
                } else if (inner == P_(ToneParams::EqHighGain)) {
                    tone.mEq.cfg.highGainDb = mParams.Get<NumericParam>(id);
                } else if (inner == P_(ToneParams::PartialMix)) {
                    tone.mPartialMix = mParams.Get<PercentParam>(id);
                } else if (inner >= P_(ToneParams::Partial1Start) && inner < P_(ToneParams::Partial2Start)) {
                    HandlePartial(tone, tone.mPartial1, inner - P_(ToneParams::Partial1Start));
                } else if (inner >= P_(ToneParams::Partial2Start) && inner < P_(ToneParams::Lfo1Start)) {
                    HandlePartial(tone, tone.mPartial2, inner - P_(ToneParams::Partial2Start));
                } else if (inner >= P_(ToneParams::Lfo1Start) && inner < P_(ToneParams::Lfo2Start)) {
                    HandleLfo(tone.mLfo1, inner - P_(ToneParams::Lfo1Start));
                } else if (inner >= P_(ToneParams::Lfo2Start) && inner < P_(ToneParams::Lfo3Start)) {
                    HandleLfo(tone.mLfo2, inner - P_(ToneParams::Lfo2Start));
                } else if (inner >= P_(ToneParams::Lfo3Start) && inner < P_(ToneParams::PitchEnvStart)) {
                    HandleLfo(tone.mLfo3, inner - P_(ToneParams::Lfo3Start));
                } else if (inner >= P_(ToneParams::PitchEnvStart) && inner < P_(ToneParams::Count)) {
                    HandleEnv(tone.mPitchEnv, inner - P_(ToneParams::PitchEnvStart));
                }
            };

            auto HandleGlobal = [&](Voice& voice, clap_id inner) {
                if (inner == P_(GlobalParams::Tune)) {
                    float tune = mParams.Get<NumericParam>(id);
                    voice.mTone1.mPartial1.mTune = tune;
                    voice.mTone1.mPartial2.mTune = tune;
                    voice.mTone2.mPartial1.mTune = tune;
                    voice.mTone2.mPartial2.mTune = tune;
                } else if (inner == P_(GlobalParams::ToneAlgorithm)) {
                    voice.mAlgo = mParams.Get<EnumParam<Voice::ToneAlgorithm>>(id);
                } else if (inner == P_(GlobalParams::ReverbMix)) {
                    mSynth.mReverbMix = mParams.Get<PercentParam>(id);
                } else if (inner == P_(GlobalParams::ReverbPreset)) {
                    if (mSynth.mReverb) {
                        mSynth.mReverb->cfg.preset = narrow_cast<int16_t>(mParams.Get<IntegerParam>(id));
                    }
                } else if (inner >= P_(GlobalParams::Tone1Start) && inner < P_(GlobalParams::Tone2Start)) {
                    HandleTone(voice.mTone1, inner - P_(GlobalParams::Tone1Start));
                } else if (inner >= P_(GlobalParams::Tone2Start) && inner < P_(GlobalParams::Count)) {
                    HandleTone(voice.mTone2, inner - P_(GlobalParams::Tone2Start));
                }
            };
            mSynth.mVoices.ForEach([&](Voice& v, const std::optional<NoteTuple>& note) {
                mParams.SetModulationMask(note ? *note : NoteTuple{});
                HandleGlobal(v, id);
            });
            mParams.SetModulationMask({});
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

        static_assert(P_(GlobalParams::Count) == 156, "Update UI");
#define XID(enum) (first + P_(enum))
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
            kitgui::DebugParam(mParams, P_(GlobalParams::Tune));
            kitgui::DebugEnumParam<Voice::ToneAlgorithm>(mParams, P_(GlobalParams::ToneAlgorithm));
            kitgui::DebugParam(mParams, P_(GlobalParams::ReverbMix));
            kitgui::DebugParam(mParams, P_(GlobalParams::ReverbPreset));
            mSampleLoader.OnImGui(GetContext());
            if (ImGui::BeginTabBar("tone")) {
                if (ImGui::BeginTabItem("Tone 1")) {
                    ToneParams(P_(GlobalParams::Tone1Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tone 2")) {
                    ToneParams(P_(GlobalParams::Tone2Start));
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

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), P_(GlobalParams::Count));
        clap_id idx = 0;
        static_assert(P_(GlobalParams::Count) == 156, "Update Traits");
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
            params.Parameter(idx++, new IntegerParam(pre + "_pitchCoarse", "Pitch Coarse", -32, 32, 0, "semis"));
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
        kitgui::SizeConfig cfg{750, 750, false, true};
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
