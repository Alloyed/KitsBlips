#include "keysynth/keysynth.h"

#include <clapeze/entryPoint.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/voice.h>
#include <kitdsp/control/adsr.h>
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
    LfoInput,
    EnvAttack,
    EnvDecay,
    EnvSustain,
    EnvRelease,
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
struct clapeze::ParamTraits<Params::OscOctave> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Octave", -2, 2, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::OscTune> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Octave", -12, 12, 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::OscModMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("ModMix (LFO <-> EG)", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::FilterCutoff> : public clapeze::PercentParam {
    // TODO: report cutoff in hz?
    ParamTraits() : clapeze::PercentParam("Cutoff", 1.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::FilterResonance> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Resonance", 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::OscModAmount> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mod Amount", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::FilterModMix> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("ModMix (LFO <-> EG)", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::FilterModAmount> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Mod Amount", 0.5f) {}
};

template <>
struct clapeze::ParamTraits<Params::LfoRate> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Rate", 0.001f, 20.0f, 0.2f, "hz") {}
};

template <>
struct clapeze::ParamTraits<Params::LfoInput> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("(secret)LFO Override", 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::EnvAttack> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Attack", 0.001f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params::EnvDecay> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Decay", 0.001f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params::EnvSustain> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("Sustain", 1.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::EnvRelease> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Release", 0.001f, 1000.0f, 1.0f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params::VcaLfoAmount> : public clapeze::PercentParam {
    ParamTraits() : clapeze::PercentParam("VCA LFO amount", 0.0f) {}
};

template <>
struct clapeze::ParamTraits<Params::PolyMode> : public clapeze::EnumParam<PolyMode> {
    ParamTraits()
        : clapeze::EnumParam<PolyMode>("Voice Mode", {"Poly", "Para", "Mono", "Chord"}, PolyMode::Polyphonic) {}
};

template <>
struct clapeze::ParamTraits<Params::PolyCount> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Voice Count", 1, 32, 4) {}
};

template <>
struct clapeze::ParamTraits<Params::PolyChordType> : public clapeze::EnumParam<PolyChordType> {
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
            mNoteFrequency = kitdsp::midiToFrequency(note.key);
            mEnv.TriggerOpen();
        }
        void ProcessNoteOff() { mEnv.TriggerClose(); }
        void ProcessChoke() { mEnv.Reset(); }
        bool ProcessAudio(clapeze::StereoAudioBuffer& out) {
            const auto& params = mProcessor.mParams;
            const float sampleRate = static_cast<float>(mProcessor.GetSampleRate());
            mEnv.SetParams(params.Get<Params::EnvAttack>(), params.Get<Params::EnvDecay>(),
                           params.Get<Params::EnvSustain>(), params.Get<Params::EnvRelease>(), sampleRate);

            for (uint32_t index = 0; index < out.left.size(); index++) {
                // modulation
                float lfo = mLfo.Process();
                float env = mEnv.Process();

                float oscMod =
                    kitdsp::lerpf(env, lfo, params.Get<Params::OscModMix>()) * params.Get<Params::OscModAmount>();
                float filterMod =
                    kitdsp::lerpf(env, lfo, params.Get<Params::FilterModMix>()) * params.Get<Params::FilterModAmount>();
                float vcaMod = env * (1.0f - (lfo * params.Get<Params::VcaLfoAmount>()));

                mOsc.SetFrequency(mNoteFrequency + oscMod, sampleRate);
                mFilter.SetFrequency(params.Get<Params::FilterCutoff>() + filterMod, sampleRate,
                                     params.Get<Params::FilterResonance>());

                // audio
                float oscOut = mOsc.Process();
                float filterOut = mFilter.Process<kitdsp::SvfFilterMode::LowPass>(oscOut);
                float vcaOut = filterOut * vcaMod;

                out.left[index] = vcaOut;
                out.right[index] = out.left[index];
            }

            // if no longer processing, time to sleep
            return mEnv.IsProcessing();
        }

       private:
        Processor& mProcessor;
        float mNoteFrequency;
        kitdsp::blep::RampUpOscillator mOsc{};
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
    clapeze::PolyphonicVoicePool<Processor, Voice, 16> mVoices;
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    GuiApp(kitgui::Context& ctx, ParamsFeature& params) : kitgui::BaseApp(ctx), mParams(params) {}
    void OnUpdate() override {
        ImGui::TextWrapped("WIP keys synthesizer. don't tell anybody but this is a volca keys for ur computer");

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

        ImGui::TextWrapped("LFO (Low frequency Oscillator)");
        kitgui::DebugParam<ParamsFeature, Params::LfoRate>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::LfoInput>(mParams);

        ImGui::TextWrapped("EG (Envelope Generator)");
        kitgui::DebugParam<ParamsFeature, Params::EnvAttack>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EnvDecay>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EnvSustain>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::EnvRelease>(mParams);

        ImGui::TextWrapped("Output");
        kitgui::DebugParam<ParamsFeature, Params::VcaLfoAmount>(mParams);

        ImGui::TextWrapped("Polyphony");
        kitgui::DebugParam<ParamsFeature, Params::PolyMode>(mParams);
        kitgui::DebugParam<ParamsFeature, Params::PolyCount>(mParams);
        // TODO: conditionally visible
        kitgui::DebugParam<ParamsFeature, Params::PolyChordType>(mParams);
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
                                    .Parameter<Params::LfoInput>()
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

const clapeze::RegisterPlugin p(
    {AudioInstrumentDescriptor("kitsblips.kitskeys", "KitsKeys", "Small Polysynth with a keys/chord focus"),
     [](clapeze::PluginHost& host) -> clapeze::BasePlugin* { return new Plugin(host); }});

}  // namespace keysynth
