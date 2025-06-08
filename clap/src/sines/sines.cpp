#include "sines/sines.h"

#include "clapeze/common.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/instrumentPlugin.h"
#include "clapeze/voice.h"
#include "kitdsp/osc/naiveOscillator.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/gui/imguiExt.h"
#include "descriptor.h"
#include "kitdsp/math/util.h"

namespace sines {
enum class Params : clap_id { Volume, Count };
using ParamsExt = ParametersExt<Params>;

class Processor : public InstrumentProcessor<ParamsExt::ProcessParameters> {
    class Voice {
       public:
        Voice() {}
        void ProcessNoteOn(Processor& p, const NoteTuple& note, float velocity) {
            mTargetAmplitude = .3f;
            mOsc.SetFrequency(kitdsp::midiToFrequency(note.key), p.GetSampleRate());
        }
        void ProcessNoteOff(Processor& p) { mTargetAmplitude = 0.0f; }
        void ProcessChoke(Processor& p) {
            mAmplitude = 0.0f;
            mTargetAmplitude = 0.0f;
        }
        bool ProcessAudio(Processor& p, StereoAudioBuffer& out) {
            for (uint32_t index = 0; index < out.left.size(); index++) {
                mAmplitude = kitdsp::lerpf(mAmplitude, mTargetAmplitude, 0.001);
                float s = mOsc.Process() * mAmplitude;
                out.left[index] += s;
                out.right[index] += s;
            }
            return mTargetAmplitude != 0.0f || mAmplitude < 0.0001f;
        }

       private:
        kitdsp::naive::TriangleOscillator mOsc{};
        float mAmplitude{};
        float mTargetAmplitude{};
    };

   public:
    Processor(ParamsExt::ProcessParameters& params) : InstrumentProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(StereoAudioBuffer& out) override {
        mVoices.ProcessAudio(*this, out);
    }

    void ProcessNoteOn(const NoteTuple& note, float velocity) override {
        mVoices.ProcessNoteOn(*this, note, velocity);
    }

    void ProcessNoteOff(const NoteTuple& note) override {
        mVoices.ProcessNoteOff(*this, note);
    }

    void ProcessNoteChoke(const NoteTuple& note) override {
        mVoices.ProcessNoteChoke(*this, note);
    }

    void ProcessReset() override {
        mVoices.StopAllVoices(*this);
    }

   private:
    PolyphonicVoicePool<Processor, Voice, 1> mVoices {};
};

class Plugin : public InstrumentPlugin {
   public:
    Plugin(PluginHost& host) : InstrumentPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        InstrumentPlugin::Config();
        ParamsExt& params = ConfigExtension<ParamsExt>(GetHost(), Params::Count)
                                .configNumeric(Params::Volume, -20.0f, 0.0f, 0.0f, "Volume");
        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace Sines
