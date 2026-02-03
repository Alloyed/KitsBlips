#include <clapeze/entryPoint.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/params/enumParametersFeature.h>
#include <clapeze/params/parameterTypes.h>
#include <clapeze/state/noopStateFeature.h>
#include <clapeze/state/tomlStateFeature.h>
#include <clapeze/voice.h>
#include <etl/vector.h>
#include <kitdsp/control/adsr.h>
#include <kitdsp/control/gate.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/units.h>
#include <kitdsp/osc/blepOscillator.h>
#include <memory>

#include "descriptor.h"
#include "kitdsp/math/util.h"
#include "kitgui/context.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "clapeze/ext/assets.h"
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#include "kitgui/gfx/scene.h"
#endif

namespace {
enum class Params : clap_id {
    OscOctave,
    OscTune,
    OscModMix,
    OscModAmount,
    FilterCutoff,
    FilterResonance,
    FilterModMix,
    FilterModAmount,
    LfoRate,
    LfoShape,
    EnvAttack,
    EnvDecay,
    EnvSustain,
    EnvRelease,
    VcaGain,
    VcaEnvDisabled,
    VcaLfoAmount,
    PolyCount,
    PolyChordType,
    Count
};
enum class PolyChordType {
    None,
    Octave,
    Fifth,
    Maj,
    Min,
    Maj7,
    Min7,
};
const std::vector<std::vector<int32_t>> kChordNotes {
    {},
    {0},
    {0, 7},
    {0, 4, 7},
    {0, 3, 7},
    {0, 4, 7, 10},
    {0, 3, 7, 10},
};
constexpr size_t cMaxVoices = 16;
using ParamsFeature = clapeze::params::EnumParametersFeature<Params>;
}  // namespace

namespace clapeze::params {
template <>
struct ParamTraits<Params, Params::OscOctave> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Octave", -2, 2, 0) {}
};

template <>
struct ParamTraits<Params, Params::OscTune> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Tune", cPowBipolarCurve<2>, -12.0f, 12.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::OscModMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("OSC ModMix (LFO <-> EG)", 0.5f) {}
};

template <>
struct ParamTraits<Params, Params::OscModAmount> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("OSC Mod Amount", cPowBipolarCurve<2>, -1.0f, 1.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::FilterCutoff> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Cutoff", 1.0f) {}
};

template <>
struct ParamTraits<Params, Params::FilterResonance> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Resonance", 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::FilterModMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("VCF ModMix (LFO <-> EG)", 0.5f) {}
};

template <>
struct ParamTraits<Params, Params::FilterModAmount> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("VCF Mod Amount", cLinearCurve, -1.0f, 1.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::LfoRate> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Rate", cLinearCurve, 0.001f, 20.0f, 0.2f, "hz") {}
};

template <>
struct ParamTraits<Params, Params::LfoShape> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("LFO Shape", 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::EnvAttack> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Attack", cPowCurve<2>, 1.0f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct ParamTraits<Params, Params::EnvDecay> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Decay", cPowCurve<2>, 10.0f, 30000.0f, 10.0f, "ms") {}
};

template <>
struct ParamTraits<Params, Params::EnvSustain> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Sustain", 1.0f) {}
};

template <>
struct ParamTraits<Params, Params::EnvRelease> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Release", cPowCurve<2>, 10.0f, 30000.0f, 10.0f, "ms") {}
};

template <>
struct ParamTraits<Params, Params::VcaGain> : public clapeze::DbParam {
    ParamTraits() : clapeze::DbParam("VCA Volume", -80.0f, 12.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::VcaEnvDisabled> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("VCA Env/Gate", OnOff::Off) {}
};

template <>
struct ParamTraits<Params, Params::VcaLfoAmount> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("VCA LFO Amount", cLinearCurve, -1.0f, 1.0f, 0.0f) {}
};

template <>
struct ParamTraits<Params, Params::PolyCount> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Voice Count", 1, cMaxVoices, 8) {}
};

template <>
struct ParamTraits<Params, Params::PolyChordType> : public clapeze::EnumParam<PolyChordType> {
    ParamTraits()
        : clapeze::EnumParam<PolyChordType>("Chord",
                                            {"None", "Octave", "5th", "Major", "Minor", "Major 7th", "Minor 7th"},
                                            PolyChordType::None) {}
};
}  // namespace clapeze::params

using namespace clapeze;

namespace keysynth {

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::ProcessorHandle> {
    class NoteProcessor {
       public:
        template <typename T>
        void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity, T callback) {
            for(auto noteOffset : mChord) {
                clapeze::NoteTuple tmp = note;
                tmp = note;
                // TODO: we need to reassign ids. to have N voices per id they
                // each need a new id, but this also means we need maintain a
                // mapping from host ids <-> processor ids, and also apply the
                // new IDs to modulations/anything else that wants them
                tmp.id = -1; // TODO
                tmp.key += noteOffset;
                callback(tmp, velocity);
            }
        }

        template <typename T>
        void ProcessNoteOff(const clapeze::NoteTuple& note, T callback) {
            for(auto noteOffset : mChord) {
                clapeze::NoteTuple tmp = note;
                tmp.id = -1; // TODO
                tmp.key += noteOffset;
                callback(tmp);
            }
        }

        template <typename T>
        void ProcessNoteChoke(const clapeze::NoteTuple& note, T callback) {
            for(auto noteOffset : mChord) {
                clapeze::NoteTuple tmp = note;
                tmp.id = -1; // TODO
                tmp.key += noteOffset;
                callback(tmp);
            }
        }

        bool SetChordType(PolyChordType chordType, int32_t numVoices) {
            if(chordType != mChordType || numVoices != mNumVoices) {
                mChord.clear();
                if(chordType == PolyChordType::None || numVoices == 1) {
                    // chord mode off
                    mChord.push_back(0);
                } else {
                    // chord mode on
                    auto& offsets = kChordNotes[static_cast<size_t>(chordType)];
                    for(int32_t idx = 0; idx < numVoices; ++idx) {
                        int32_t x = idx % offsets.size();
                        int32_t y = idx / offsets.size();
                        mChord.push_back(y * 12 + offsets[x]);
                    }
                }
                mChordType = chordType;
                mNumVoices = numVoices;
                // caller should reset existing voices
                return true;
            }
            return false;
        }

       private:
        PolyChordType mChordType{};
        int32_t mNumVoices{};
        etl::vector<int32_t, 4> mChord{};
    };

    class Voice {
       public:
        explicit Voice(Processor& p) : mProcessor(p) {}
        void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
            (void)velocity;
            mNote = note.key;
            mEnv.TriggerOpen();
            mGateEnv.TriggerOpen();
        }
        void ProcessNoteOff() {
            mEnv.TriggerClose();
            mGateEnv.TriggerClose();
        }
        void ProcessChoke() {
            mEnv.TriggerChoke();
            mGateEnv.TriggerChoke();
        }
        bool ProcessAudio(clapeze::StereoAudioBuffer& out) {
            const float sampleRate = static_cast<float>(mProcessor.GetSampleRate());

            // collect params
            const auto& params = mProcessor.mParams;

            // osc
            float oscNote =
                mNote + (static_cast<float>(params.Get<Params::OscOctave>()) * 12.0f) + params.Get<Params::OscTune>();
            float oscModMix = params.Get<Params::OscModMix>();
            float oscModAmount = params.Get<Params::OscModAmount>() * 64.0f;

            // filter
            // range from 8hz to 12.5khz
            float filterNote = kitdsp::lerpf(0.0f, 127.0f, params.Get<Params::FilterCutoff>());
            float res = params.Get<Params::FilterResonance>() * 0.89f;  // 0.85 acts as cap, experimentally determined
            float filterSteepness = 0.5f;  // steeper means "achieves self-oscillation quicker"
            float filterQ = 0.5f * std::exp(filterSteepness * (res / (1 - res)));  // [0, 1] -> [0.5, inf]
            float filterModMix = params.Get<Params::FilterModMix>();
            float filterModAmount = params.Get<Params::FilterModAmount>() * 64.0f;

            // env
            mEnv.SetParams(params.Get<Params::EnvAttack>(), params.Get<Params::EnvDecay>(),
                           params.Get<Params::EnvSustain>(), params.Get<Params::EnvRelease>(), sampleRate);

            // lfo
            mLfo.SetFrequency(params.Get<Params::LfoRate>(), sampleRate);
            float lfoShape = params.Get<Params::LfoShape>();

            // vca
            // We know this will usually be used polyphonically, so let's get some headroom
            constexpr float cVcaBaseGainRatio = 0.2f;
            float vcaGain = kitdsp::dbToRatio(params.Get<Params::VcaGain>()) * cVcaBaseGainRatio;
            float vcaModAmount = params.Get<Params::VcaLfoAmount>();
            float vcaEnvDisabled = params.Get<Params::VcaEnvDisabled>() == OnOff::On ? 1.0f : 0.0f;  // TODO: xfade

            for (uint32_t index = 0; index < out.left.size(); index++) {
                // modulation
                float lfoTri = mLfo.Process();  // [-1, 1]
                float lfoSquare = lfoTri > 0.0f ? -1.0f : 1.0f;
                float lfo = kitdsp::lerpf(lfoTri, lfoSquare, lfoShape);
                float env = mEnv.Process();          // [0, 1]
                float gateEnv = mGateEnv.Process();  // [0, 1]

                float oscMod = kitdsp::lerpf(lfo, env, oscModMix) * oscModAmount;
                float filterMod = kitdsp::lerpf(lfo, env, filterModMix) * filterModAmount;
                float vcaMod = kitdsp::lerpf(env, gateEnv, vcaEnvDisabled) * (1.0f - (-lfo * vcaModAmount));
                vcaMod = vcaMod * vcaMod * vcaMod;  // approximate db curve (not really but yknow)

                mOsc.SetFrequency(kitdsp::midiToFrequency(oscNote + oscMod), sampleRate);
                mFilter.SetFrequency(kitdsp::midiToFrequency(filterNote + filterMod), sampleRate, filterQ);

                // audio
                float oscOut = mOsc.Process();
                float filterOut = mFilter.Process<kitdsp::SvfFilterMode::LowPass>(oscOut);
                float vcaOut = filterOut * vcaMod * vcaGain;

                out.left[index] += vcaOut;
                out.right[index] += vcaOut;
            }

            // if no longer processing, time to sleep
            return mEnv.IsProcessing();
        }

       private:
        Processor& mProcessor;
        float mNote;
        kitdsp::blep::RampUpOscillator mOsc{};
        kitdsp::Gate mGateEnv{};
        // TODO: paraphony, maybe
        kitdsp::EmileSvf mFilter{};
        kitdsp::lfo::TriangleOscillator mLfo{};
        kitdsp::ApproachAdsr mEnv{};
    };

   public:
    explicit Processor(ParamsFeature::ProcessorHandle& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        int32_t polyCount = mParams.Get<Params::PolyCount>();
        mVoices.SetNumVoices(polyCount);
        mVoices.SetStrategy(polyCount > 1 ? clapeze::VoiceStrategy::Poly : clapeze::VoiceStrategy::MonoLast);
        if(mNoteProcessor.SetChordType(mParams.Get<Params::PolyChordType>(), polyCount)) {
            mVoices.StopAllVoices();
        }
        return mVoices.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mNoteProcessor.ProcessNoteOn(note, velocity, [this](const clapeze::NoteTuple& inote, float ivelocity) {
            mVoices.ProcessNoteOn(inote, ivelocity);
        });
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override {
        mNoteProcessor.ProcessNoteOff(note, [this](const clapeze::NoteTuple& inote) {
            mVoices.ProcessNoteOff(inote);
        });
     }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override {
        mNoteProcessor.ProcessNoteChoke(note, [this](const clapeze::NoteTuple& inote) {
            mVoices.ProcessNoteChoke(inote);
        });
    }

    void ProcessReset() override { mVoices.StopAllVoices(); }

   private:
    clapeze::VoicePool<Processor, Voice, cMaxVoices> mVoices;
    NoteProcessor mNoteProcessor{};
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params)
        : kitgui::BaseApp(ctx), mParams(params), mScene(std::make_unique<kitgui::Scene>(ctx)) {}
    ~GuiApp() = default;

    void OnActivate() override {
        float scale = static_cast<float>(GetContext().GetUIScale());
        mScene->Load("assets/kitskeys.glb");
        // TODO: to update all this if the viewport changes
        uint32_t w{};
        uint32_t h{};
        GetContext().GetSizeInPixels(w, h);
        mScene->SetViewport({static_cast<float>(w), static_cast<float>(h)});
        mScene->SetBrightness(0.0070f);  // idk why magnum is so intense by default, to investigate

        struct KnobSetupInfo {
            Params param;
            std::string node;
        };
        const std::vector<KnobSetupInfo> knobs{
            {Params::PolyCount, "knob-mid-Davies-1900h.001"},
            {Params::PolyChordType, "knob-mid-Davies-1900h.002"},
            {Params::OscOctave, "knob-mid-Davies-1900h.003"},
            {Params::OscTune, "knob-mid-Davies-1900h.004"},
            {Params::OscModMix, "knob-small-trimpot-R-0904N-L-25KC"},
            {Params::OscModAmount, "knob-small-trimpot-R-0904N-L-25KC.001"},
            {Params::FilterCutoff, "knob-mid-Davies-1900h.006"},
            {Params::FilterResonance, "knob-mid-Davies-1900h.005"},
            {Params::FilterModMix, "knob-small-trimpot-R-0904N-L-25KC.002"},
            {Params::FilterModAmount, "knob-small-trimpot-R-0904N-L-25KC.003"},
            {Params::LfoRate, "knob-mid-Davies-1900h.008"},
            {Params::LfoShape, "knob-mid-Davies-1900h.009"},
            {Params::EnvAttack, "knob-mid-Davies-1900h.010"},
            {Params::EnvDecay, "knob-mid-Davies-1900h.011"},
            {Params::EnvSustain, "knob-mid-Davies-1900h.012"},
            {Params::EnvRelease, "knob-mid-Davies-1900h.013"},
            {Params::VcaGain, "knob-mid-Davies-1900h.007"},
            {Params::VcaLfoAmount, "knob-small-trimpot-R-0904N-L-25KC.004"},
        };

        // TODO
        const std::vector<KnobSetupInfo> toggles{
            {Params::VcaEnvDisabled, "switch-toggle"},
        };

        // TODO: data-driven
        for (const auto& knobInfo : knobs) {
            clap_id id = static_cast<clap_id>(knobInfo.param);
            mKnobs.push_back(std::make_unique<kitgui::BaseParamKnob>(*mParams.GetBaseParam(id), id, knobInfo.node));
            auto pos = mScene->GetObjectScreenPositionByName(knobInfo.node);
            if (pos) {
                // TODO: to size the knob appropriately we need the bounding range of the object and then transform the
                // corners of that into screen positions. for now, hardcoded.
                float w = 40.0f * scale;
                float hw = w * 0.5f;
                mKnobs.back()->mPos = {pos->x() - hw, pos->y() - hw};
                mKnobs.back()->mWidth = w;
                mKnobs.back()->mShowDebug = false;
            }
        }
    }

    void DebugParamList() {
        ImGui::TextWrapped("WIP keys synthesizer. don't tell anybody but this is a volca keys for ur computer");

        ImGui::TextWrapped("Voicing");
        kitgui::DebugParam<ParamsFeature, Params::PolyCount>(mParams);

        // TODO: conditionally visible
        kitgui::DebugParam<ParamsFeature, Params::PolyChordType>(mParams);
        ImGui::TextWrapped("Oscillator");
        kitgui::DebugParam<ParamsFeature, Params::OscOctave>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::OscTune>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::OscModMix>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::OscModAmount>(mParams);

        ImGui::TextWrapped("Filter");
        kitgui::DebugParam<ParamsFeature, Params::FilterCutoff>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::FilterResonance>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::FilterModMix>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::FilterModAmount>(mParams);

        ImGui::TextWrapped("MOD A: Low frequency Oscillator");
        kitgui::DebugParam<ParamsFeature, Params::LfoRate>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::LfoShape>(mParams);

        ImGui::TextWrapped("MOD B: Envelope");
        kitgui::DebugParam<ParamsFeature, Params::EnvAttack>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EnvDecay>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EnvSustain>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EnvRelease>(mParams);

        ImGui::TextWrapped("Volume");
        kitgui::DebugParam<ParamsFeature, Params::VcaGain>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::VcaEnvDisabled>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::VcaLfoAmount>(mParams);
    }
    void OnUpdate() override {
        mParams.FlushFromAudio();
        mScene->Update();
        // imgui
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Debug Window", NULL, &mShowDebugWindow);
                ImGui::EndMenu();
            }
        }

        for (auto& knob : mKnobs) {
            clap_id id = knob->GetParamId();
            double raw = mParams.GetMainHandle().GetRawValue(id);

            knob->mShowDebug = mShowDebugWindow;
            if (knob->Update(raw)) {
                mParams.GetMainHandle().SetRawValue(id, raw);
            }
            // we assume the knob is at 12-o-clock, and knobs have 0.75turn(270deg) ranges
            if (knob->IsStepped()) {
                raw = std::trunc(raw);
            }
            auto normalizedValue = kitdsp::clamp((raw - knob->mMin) / (knob->mMax - knob->mMin), 0.0, 1.0);
            constexpr float maxTurn = kitdsp::kPi * 2.0f * 0.75f;
            mScene->SetObjectRotationByName(knob->GetSceneNode(),
                                            (static_cast<float>(normalizedValue) - 0.5f) * -maxTurn);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
            mShowDebugWindow = !mShowDebugWindow;
        }

        if (mShowDebugWindow) {
            if (ImGui::Begin("Debug Window", &mShowDebugWindow)) {
                DebugParamList();
            }
            ImGui::End();
        }
    }

    void OnDraw() override { mScene->Draw(); }

   private:
    ParamsFeature& mParams;
    std::unique_ptr<kitgui::Scene> mScene;
    std::vector<std::unique_ptr<kitgui::BaseParamKnob>> mKnobs;
    bool mShowDebugWindow = true;
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

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Module("Voicing")
                                    .Parameter<Params::PolyCount>()
                                    .Parameter<Params::PolyChordType>()
                                    .Module("Oscillator")
                                    .Parameter<Params::OscOctave>()
                                    .Parameter<Params::OscTune>()
                                    .Parameter<Params::OscModMix>()
                                    .Parameter<Params::OscModAmount>()
                                    .Module("Filter")
                                    .Parameter<Params::FilterCutoff>()
                                    .Parameter<Params::FilterResonance>()
                                    .Parameter<Params::FilterModAmount>()
                                    .Parameter<Params::FilterModMix>()
                                    .Module("LFO")
                                    .Parameter<Params::LfoRate>()
                                    .Parameter<Params::LfoShape>()
                                    .Module("ADSR")
                                    .Parameter<Params::EnvAttack>()
                                    .Parameter<Params::EnvDecay>()
                                    .Parameter<Params::EnvSustain>()
                                    .Parameter<Params::EnvRelease>()
                                    .Module("VCA")
                                    .Parameter<Params::VcaGain>()
                                    .Parameter<Params::VcaEnvDisabled>()
                                    .Parameter<Params::VcaLfoAmount>();
        ConfigFeature<clapeze::TomlStateFeature<ParamsFeature>>();
        // ConfigFeature<clapeze::NoopStateFeature<ParamsFeature>>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<clapeze::AssetsFeature>(GetHost());
        // aspect ratio 1.5
        kitgui::SizeConfig cfg{600, 400, false, true};
        ConfigFeature<KitguiFeature>(
            GetHost(), [&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); }, cfg);
#endif

        ConfigProcessor<Processor>(params.GetProcessorHandle());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioInstrumentDescriptor("kitsblips.kitskeys",
                                                  "KitsKeys",
                                                  "Small Polysynth with a keys/chord focus"));
}  // namespace keysynth
