#include <clapeze/entryPoint.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/voice.h>
#include <kitdsp/control/adsr.h>
#include <kitdsp/control/gate.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/units.h>
#include <kitdsp/osc/blepOscillator.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/debugui.h"
#include "gui/feature.h"
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
    LfoSync,
    EnvAttack,
    EnvDecay,
    EnvSustain,
    EnvRelease,
    VcaGain,
    VcaEnvDisabled,
    VcaLfoAmount,
    PolyMode,
    PolyCount,
    PolyChordType,
    Count
};
enum class PolyMode {
    Polyphonic,
    Paraphonic,
    Monophonic,
    Chord,
};
enum class PolyChordType {
    Octave,
    Fifth,
    Maj,
    Min,
    Maj7,
    Min7,
};
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::OscOctave> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Octave", -2, 2, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::OscTune> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Tune", -12, 12, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::OscModMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("OSC ModMix (LFO <-> EG)", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::OscModAmount> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("OSC Mod Amount", -1.0f, 1.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::FilterCutoff> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Cutoff", 1.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::FilterResonance> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Resonance", 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::FilterModMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("VCF ModMix (LFO <-> EG)", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::FilterModAmount> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("VCF Mod Amount", -1.0f, 1.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::LfoRate> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Rate", 0.001f, 20.0f, 0.2f, "hz") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::LfoShape> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("LFO Shape", 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::EnvAttack> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Attack", 0.001f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::EnvDecay> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Decay", 0.001f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::EnvSustain> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Sustain", 1.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::EnvRelease> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Release", 0.001f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::VcaGain> : public clapeze::DbParam {
    ParamTraits() : clapeze::DbParam("VCA Volume", -80.0f, 12.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::VcaEnvDisabled> : public clapeze::OnOffParam {
    ParamTraits() : clapeze::OnOffParam("VCA Env/Gate", OnOff::Off) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::VcaLfoAmount> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("VCA LFO Amount", -1.0f, 1.0f, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::PolyMode> : public clapeze::EnumParam<PolyMode> {
    ParamTraits()
        : clapeze::EnumParam<PolyMode>("Voice Mode", {"Poly", "Para", "Mono", "Chord"}, PolyMode::Polyphonic) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::PolyCount> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Voice Count", 1, 32, 4) {}
};

template <>
struct clapeze::ParamTraits<Params, Params::PolyChordType> : public clapeze::EnumParam<PolyChordType> {
    ParamTraits()
        : clapeze::EnumParam<PolyChordType>("Chord",
                                            {"Octave", "5th", "Major", "Minor", "Major 7th", "Minor 7th"},
                                            PolyChordType::Octave) {}
};

using namespace clapeze;

namespace keysynth {

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::ProcessParameters> {
    class Voice {
       public:
        explicit Voice(Processor& p) : mProcessor(p) {}
        void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
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
            float res = params.Get<Params::FilterResonance>();
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
            float vcaGain = kitdsp::dbToRatio(params.Get<Params::VcaGain>());
            float vcaModAmount = params.Get<Params::VcaLfoAmount>();
            float vcaEnvDisabled = params.Get<Params::VcaEnvDisabled>() == OnOff::On ? 1.0f: 0.0f; // TODO: xfade

            for (uint32_t index = 0; index < out.left.size(); index++) {
                // modulation
                float lfoTri = mLfo.Process();  // [-1, 1]
                float lfoSquare = lfoTri > 0.0 ? -1 : 1;
                float lfo = kitdsp::lerpf(lfoTri, lfoSquare, lfoShape);
                float env = mEnv.Process();  // [0, 1]
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
    explicit Processor(ParamsFeature::ProcessParameters& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    void ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        mVoices.SetNumVoices(mParams.Get<Params::PolyCount>());
        mVoices.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mVoices.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() override { mVoices.StopAllVoices(); }

   private:
    // TODO: store voice pool in unique ptr and allocate polymorphically
    clapeze::PolyphonicVoicePool<Processor, Voice, 16> mVoices;
    // clapeze::MonophonicVoicePool<Processor, Voice> mVoices;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {
        ctx.SetSizeConfig({1000, 600});
    }
    ~GuiApp() = default;

    void OnUpdate() override {
        ImGui::TextWrapped("WIP keys synthesizer. don't tell anybody but this is a volca keys for ur computer");

        ImGui::TextWrapped("Voicing");
        kitgui::DebugParam<ParamsFeature, Params::PolyMode>(mParams);
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

   private:
    ParamsFeature& mParams;
};
#endif

class Plugin : public InstrumentPlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(PluginHost& host) : InstrumentPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        InstrumentPlugin::Config();

        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
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
                                    .Module("ADSR")
                                    .Parameter<Params::EnvAttack>()
                                    .Parameter<Params::EnvDecay>()
                                    .Parameter<Params::EnvSustain>()
                                    .Parameter<Params::EnvRelease>()
                                    .Module("VCA")
                                    .Parameter<Params::VcaLfoAmount>()
                                    .Module("Voicing")
                                    .Parameter<Params::PolyMode>()
                                    .Parameter<Params::PolyCount>()
                                    .Parameter<Params::PolyChordType>();
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>([&params](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, params); });
#endif

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioInstrumentDescriptor("kitsblips.kitskeys",
                                                  "KitsKeys",
                                                  "Small Polysynth with a keys/chord focus"));
}  // namespace keysynth
