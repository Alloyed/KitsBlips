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
#include <sstream>

#include "descriptor.h"
#include "kitdsp/filters/svf.h"
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
    Rate,
    Count,
};
enum class EnvParams : clap_id {
    Attack,
    Decay,
    Sustain,
    Release,
    Count,
};
enum class LayerParams : clap_id {
    WaveIndex,
    PitchCoarse,
    PitchFine,
    FilterMode,
    FilterNote,
    FilterRes,
    FilterTrackingMult,
    VcaMult,
    VcaVelocityMult,
    ChorusMix,
    ChorusRate,
    ChorusDepth,
    ChorusFeedback,
    VoiceEnvStart,
    VoiceLfoStart = VoiceEnvStart + P_(EnvParams::Count),
    Count = VoiceLfoStart + P_(LfoParams::Count),
};
enum class GlobalParams {
    Tune,
    Portamento,
    ReverbMix,
    ReverbPreset,
    Layer1Start,
    Layer2Start = Layer1Start + P_(LayerParams::Count),
    Layer3Start = Layer2Start + P_(LayerParams::Count),
    Layer4Start = Layer3Start + P_(LayerParams::Count),
    Count = Layer4Start + P_(LayerParams::Count),
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

struct LfoRateParam : public clapeze::NumericParam {
    LfoRateParam(std::string_view key, std::string_view name)
        : clapeze::NumericParam(key, name, 0.001f, 20.0f, 0.2f, "hz") {
        mCurve = clapeze::cPowCurve<3.0f>;
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
            f->sampleRate = narrow_cast<float>(pFlac->sampleRate);
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
            f->sampleRate = narrow_cast<float>(wav.sampleRate);
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
        : InstrumentProcessor(params), mGlobal(*this, params), mSampleLoader(sampleLoader) {
        static_assert(P_(GlobalParams::Count) == 76, "Update handlers");
        params.RegisterHandler([&](clap_id id) {
            // printf("Id: %u (%s), value: %f\n", id, mParams.GetKey(id).c_str(), mParams.GetRawValue(id));
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
            auto HandleLayer = [&](Layer& layer, clap_id inner) {
                clap_id start = id - inner;
                if (inner == P_(LayerParams::WaveIndex)) {
                    layer.mWaveIndex = mParams.Get<IntegerParam>(id);
                } else if (inner == P_(LayerParams::PitchCoarse) || inner == P_(LayerParams::PitchFine)) {
                    layer.mNoteOffset =
                        narrow_cast<float>(mParams.Get<IntegerParam>(start + P_(LayerParams::PitchCoarse))) +
                        mParams.Get<NumericParam>(start + P_(LayerParams::PitchFine)) / 100.0f;
                } else if (inner == P_(LayerParams::FilterMode)) {
                    for (Voice& voice : layer.mVoices.IterAll()) {
                        voice.mFilterMode = mParams.Get<EnumParam<kitdsp::SvfFilterMode>>(id);
                    }
                } else if (inner == P_(LayerParams::FilterNote)) {
                    layer.mFilterNote = mParams.Get<PercentParam>(id) * 127.0f;
                } else if (inner == P_(LayerParams::FilterTrackingMult)) {
                    layer.mFilterTrackingMult = mParams.Get<PercentParam>(id);
                } else if (inner == P_(LayerParams::FilterRes)) {
                    layer.mFilterRes = mParams.Get<PercentParam>(id);
                } else if (inner == P_(LayerParams::VcaMult)) {
                    layer.mVcaMult = mParams.Get<PercentParam>(id);
                } else if (inner == P_(LayerParams::VcaVelocityMult)) {
                    layer.mVcaVelocityMult = mParams.Get<PercentParam>(id);
                } else if (inner == P_(LayerParams::ChorusMix)) {
                    if (layer.mChorus) {
                        layer.mChorus->cfg.mix = mParams.Get<PercentParam>(id);
                    }
                } else if (inner == P_(LayerParams::ChorusRate)) {
                    if (layer.mChorus) {
                        layer.mChorus->cfg.lfoRateHz = mParams.Get<FreqParam>(id);
                    }
                } else if (inner == P_(LayerParams::ChorusDepth)) {
                    if (layer.mChorus) {
                        layer.mChorus->cfg.delayBaseMs = mParams.Get<PercentParam>(id) * 20.0f;
                        layer.mChorus->cfg.delayModMs = mParams.Get<PercentParam>(id) * 8.0f;
                    }
                } else if (inner == P_(LayerParams::ChorusFeedback)) {
                    if (layer.mChorus) {
                        layer.mChorus->cfg.feedback = mParams.Get<PercentParam>(id);
                    }
                } else if (inner >= P_(LayerParams::VoiceEnvStart) && inner < P_(LayerParams::VoiceLfoStart)) {
                    for (Voice& voice : layer.mVoices.IterAll()) {
                        HandleEnv(voice.mVolumeEnv, inner - P_(LayerParams::VoiceEnvStart));
                    }
                } else if (inner >= P_(LayerParams::VoiceLfoStart) && inner < P_(LayerParams::Count)) {
                    for (Voice& voice : layer.mVoices.IterAll()) {
                        HandleLfo(voice.mPitchLfo, inner - P_(LayerParams::VoiceLfoStart));
                    }
                }
            };

            auto HandleGlobal = [&](clap_id inner) {
                if (inner == P_(GlobalParams::Tune)) {
                    mGlobal.mTune = mParams.Get<NumericParam>(id);
                } else if (inner == P_(GlobalParams::Portamento)) {
                    mGlobal.mPortamento = mParams.Get<EnvParam>(id);
                } else if (inner == P_(GlobalParams::ReverbMix)) {
                    mGlobal.mReverbMix = mParams.Get<PercentParam>(id);
                } else if (inner == P_(GlobalParams::ReverbPreset)) {
                    if (mGlobal.mReverb) {
                        mGlobal.mReverb->cfg.preset = narrow_cast<int16_t>(mParams.Get<IntegerParam>(id));
                    }
                } else if (inner >= P_(GlobalParams::Layer1Start) && inner < P_(GlobalParams::Layer2Start)) {
                    HandleLayer(mGlobal.mLayers[0], inner - P_(GlobalParams::Layer1Start));
                } else if (inner >= P_(GlobalParams::Layer2Start) && inner < P_(GlobalParams::Layer3Start)) {
                    HandleLayer(mGlobal.mLayers[1], inner - P_(GlobalParams::Layer2Start));
                } else if (inner >= P_(GlobalParams::Layer3Start) && inner < P_(GlobalParams::Layer4Start)) {
                    HandleLayer(mGlobal.mLayers[2], inner - P_(GlobalParams::Layer3Start));
                } else if (inner >= P_(GlobalParams::Layer4Start) && inner < P_(GlobalParams::Count)) {
                    HandleLayer(mGlobal.mLayers[3], inner - P_(GlobalParams::Layer4Start));
                }
            };
            HandleGlobal(id);
            mParams.SetModulationMask({});
        });
    }
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        if (mSampleLoader.OnAudioUpdate()) {
            for (auto& layer : mGlobal.mLayers) {
                for (Voice& voice : layer.mVoices.IterAll()) {
                    voice.mPcmSampler = mSampleLoader.GetSampler(0);
                }
            }
        }
        return mGlobal.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mGlobal.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mGlobal.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mGlobal.ProcessNoteChoke(note); }

    void ProcessReset() override { mGlobal.ProcessReset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        mGlobal.Activate(sampleRate, minBlockSize, maxBlockSize);
    }

   private:
    Global mGlobal;
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

        static_assert(P_(GlobalParams::Count) == 76, "Update UI");
#define XID(enum) (first + P_(enum))
        auto LfoParams = [&](clap_id first) { kitgui::DebugParam(mParams, XID(LfoParams::Rate)); };
        auto EnvParams = [&](clap_id first) {
            kitgui::DebugParam(mParams, XID(EnvParams::Attack));
            kitgui::DebugParam(mParams, XID(EnvParams::Decay));
            kitgui::DebugParam(mParams, XID(EnvParams::Sustain));
            kitgui::DebugParam(mParams, XID(EnvParams::Release));
        };
        auto LayerParams = [&](clap_id first) {
            ImGui::Indent();
            ImGui::SeparatorText("Wave Generator");
            ImGui::PushID("wg");
            kitgui::DebugParam(mParams, XID(LayerParams::WaveIndex));
            kitgui::DebugParam(mParams, XID(LayerParams::PitchCoarse));
            kitgui::DebugParam(mParams, XID(LayerParams::PitchFine));
            LfoParams(XID(LayerParams::VoiceLfoStart));
            ImGui::PopID();

            ImGui::SeparatorText("Filter");
            ImGui::PushID("vcf");
            kitgui::DebugEnumParam<kitdsp::SvfFilterMode>(mParams, XID(LayerParams::FilterMode));
            kitgui::DebugParam(mParams, XID(LayerParams::FilterNote));
            kitgui::DebugParam(mParams, XID(LayerParams::FilterRes));
            kitgui::DebugParam(mParams, XID(LayerParams::FilterTrackingMult));
            ImGui::PopID();

            ImGui::SeparatorText("Volume");
            ImGui::PushID("vca");
            kitgui::DebugParam(mParams, XID(LayerParams::VcaMult));
            kitgui::DebugParam(mParams, XID(LayerParams::VcaVelocityMult));
            EnvParams(XID(LayerParams::VoiceEnvStart));
            ImGui::PopID();

            ImGui::SeparatorText("Chorus");
            ImGui::PushID("chorus");
            kitgui::DebugParam(mParams, XID(LayerParams::ChorusMix));
            kitgui::DebugParam(mParams, XID(LayerParams::ChorusRate));
            kitgui::DebugParam(mParams, XID(LayerParams::ChorusDepth));
            kitgui::DebugParam(mParams, XID(LayerParams::ChorusFeedback));
            EnvParams(XID(LayerParams::VoiceEnvStart));
            ImGui::PopID();

            ImGui::Unindent();
        };
        auto GlobalParams = [&]() {
            kitgui::DebugParam(mParams, P_(GlobalParams::Tune));
            kitgui::DebugParam(mParams, P_(GlobalParams::Portamento));
            kitgui::DebugParam(mParams, P_(GlobalParams::ReverbMix));
            kitgui::DebugParam(mParams, P_(GlobalParams::ReverbPreset));
            mSampleLoader.OnImGui(GetContext());
            if (ImGui::BeginTabBar("layer")) {
                if (ImGui::BeginTabItem("Layer 1")) {
                    LayerParams(P_(GlobalParams::Layer1Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Layer 2")) {
                    LayerParams(P_(GlobalParams::Layer2Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Layer 3")) {
                    LayerParams(P_(GlobalParams::Layer3Start));
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Layer 4")) {
                    LayerParams(P_(GlobalParams::Layer4Start));
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
        static_assert(P_(GlobalParams::Count) == 76, "Update Traits");
#define XID(enum) (first + P_(enum))
        auto LfoParams = [&](const std::string& pre, clap_id first) {
            params.Parameter(XID(LfoParams::Rate), new LfoRateParam(pre + "_rate", "Rate"));
        };
        auto EnvParams = [&](const std::string& pre, clap_id first) {
            params.Parameter(XID(EnvParams::Attack), new EnvParam(pre + "_attack", "Attack", 5.0f, 1000.0f));
            params.Parameter(XID(EnvParams::Decay), new EnvParam(pre + "_decay", "Decay", 20.0f, 10000.0f));
            params.Parameter(XID(EnvParams::Sustain), new PercentParam(pre + "_sustain", "Sustain", 1.0f));
            params.Parameter(XID(EnvParams::Release), new EnvParam(pre + "_release", "Release", 20.0f, 10000.0f));
        };
        auto LayerParams = [&](const std::string& pre, clap_id first) {
            params.Parameter(XID(LayerParams::WaveIndex), new IntegerParam(pre + "_wave", "Wave", 1, 4, 1));
            params.Parameter(XID(LayerParams::PitchCoarse),
                             new IntegerParam(pre + "_pitchCoarse", "Pitch Coarse", -32, 32, 0, "semis"));
            params.Parameter(XID(LayerParams::PitchFine),
                             new NumericParam(pre + "_pitchFine", "Pitch Fine", -100.0f, 100.0f, 0.0f, "cents"));
            params.Parameter(XID(LayerParams::FilterMode),
                             new EnumParam<kitdsp::SvfFilterMode>(pre + "_filterMode", "Filter Mode",
                                                                  {"Lowpass", "Highpass", "Bandpass"},
                                                                  kitdsp::SvfFilterMode::LowPass));
            params.Parameter(XID(LayerParams::FilterNote), new PercentParam(pre + "_filter", "Filter Cutoff", 1.0f));
            params.Parameter(XID(LayerParams::FilterRes),
                             new PercentParam(pre + "_filterRes", "Filter Resonance", 0.0f));
            params.Parameter(XID(LayerParams::FilterTrackingMult),
                             new PercentParam(pre + "_filterTrack", "Filter Tracking", 0.0f));
            params.Parameter(XID(LayerParams::VcaMult), new PercentParam(pre + "_vcaMult", "Volume", 0.5f));
            params.Parameter(XID(LayerParams::VcaVelocityMult), new PercentParam(pre + "_vcaMult", "Velocity", 0.0f));
            params.Parameter(XID(LayerParams::ChorusMix), new PercentParam(pre + "_chorusMix", "Chorus Mix", 0.0f));
            params.Parameter(XID(LayerParams::ChorusRate),
                             new FreqParam(pre + "_chorusRate", "Chorus Rate", 0.001f, 5.0f, 1.0f));
            params.Parameter(XID(LayerParams::ChorusDepth),
                             new PercentParam(pre + "_chorusDepth", "Chorus Depth", 1.0f));
            params.Parameter(XID(LayerParams::ChorusFeedback),
                             new PercentParam(pre + "_chorusFeedback", "Chorus Feedback", 0.0f));
            EnvParams(pre + "VoiceEnv", XID(LayerParams::VoiceEnvStart));
            LfoParams(pre + "VoiceLfo", XID(LayerParams::VoiceLfoStart));
        };
        auto GlobalParams = [&]() {
            params.Parameter(P_(GlobalParams::Tune), new NumericParam("tune", "Tune", 20.0f, 1000.0f, 440.0f, "hz"));
            params.Parameter(P_(GlobalParams::Portamento), new EnvParam("portamento", "Portamento", 0.0f, 20.0f));
            params.Parameter(P_(GlobalParams::ReverbMix), new PercentParam("reverbMix", "Reverb Mix", 0.0f));
            params.Parameter(P_(GlobalParams::ReverbPreset),
                             new IntegerParam("reverbPreset", "Reverb Preset", 1, kitdsp::PSX::kNumPresets, 1));
            LayerParams("Layer1", P_(GlobalParams::Layer1Start));
            LayerParams("Layer2", P_(GlobalParams::Layer2Start));
            LayerParams("Layer3", P_(GlobalParams::Layer3Start));
            LayerParams("Layer4", P_(GlobalParams::Layer4Start));
        };

        GlobalParams();
#undef XID

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
