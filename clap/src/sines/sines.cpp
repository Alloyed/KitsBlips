#include "sines/sines.h"

#include "clapeze/common.h"
#include "clapeze/ext/parameterConfigs.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/gui/imguiExt.h"
#include "clapeze/instrumentPlugin.h"
#include "clapeze/voice.h"
#include "descriptor.h"
#include <kitdsp/control/approach.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/util.h>
#include <kitdsp/osc/naiveOscillator.h>

namespace sines {
enum class Params : clap_id { Rise, Fall, VibratoRate, VibratoDepth, Portamento, Polyphony, Count };
using ParamsExt = ParametersExt<Params>;

class Processor : public InstrumentProcessor<ParamsExt::ProcessParameters> {
    class Voice {
       public:
        Voice() {}
        void ProcessNoteOn(Processor& p, const NoteTuple& note, float velocity) {
            mAmplitude.target = 1.0f;
            mPitch.target = note.key;
        }
        void ProcessNoteOff(Processor& p) {
            mAmplitude.target = 0.0f;
        }
        void ProcessChoke(Processor& p) {
            mOsc.Reset();
            mFilter.Reset();
            mAmplitude.Reset();
            mPitch.Reset();
            mVibrato.Reset();
        }
        bool ProcessAudio(Processor& p, StereoAudioBuffer& out) {
            // TODO: it'd be neat to adopt a signals based workflow for these, it'd need to be allocation-free though
            mPitch.SetHalfLife(p.mParams.Get<NumericParam>(Params::Portamento), p.GetSampleRate());
            mAmplitude.SetHalfLife(p.mParams.Get<NumericParam>(mAmplitude.target > mAmplitude.current ? Params::Rise : Params::Fall),
                                   p.GetSampleRate());
            mVibrato.SetFrequency(p.mParams.Get<NumericParam>(Params::VibratoRate), p.GetSampleRate());
            float vibratoDepth = p.mParams.Get<NumericParam>(Params::VibratoDepth) * 0.01f;

            for (uint32_t index = 0; index < out.left.size(); index++) {
                // doing midiToFrequency last to ensure all effects get exponential fm
                float freq = kitdsp::midiToFrequency(mPitch.Process() + (mVibrato.Process() * vibratoDepth));
                mOsc.SetFrequency(freq, p.GetSampleRate());
                mFilter.SetFrequency(freq * 1.5, p.GetSampleRate(), 10.0f);

                float s = mFilter.Process<kitdsp::FilterMode::LowPass>(mOsc.Process() * mAmplitude.Process() * 0.3f);
                out.left[index] += s;
                out.right[index] += s;
            }
            if(mAmplitude.current < 0.0001f && !mAmplitude.IsChanging())
            {
                // audio settled, time to sleep
                return false;
            }
            return true;
        }

       private:
        kitdsp::naive::RampUpOscillator mOsc{};
        kitdsp::EmileSvf mFilter {};
        kitdsp::Approach mAmplitude {};
        kitdsp::Approach mPitch {};
        kitdsp::lfo::SineOscillator mVibrato {};
    };

   public:
    Processor(ParamsExt::ProcessParameters& params) : InstrumentProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(StereoAudioBuffer& out) override {
        mVoices.SetNumVoices(*this, mParams.Get<IntegerParam>(Params::Polyphony));
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
    PolyphonicVoicePool<Processor, Voice, 16> mVoices {};
};

class Plugin : public InstrumentPlugin {
   public:
    Plugin(PluginHost& host) : InstrumentPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        InstrumentPlugin::Config();
        ParamsExt& params = ConfigExtension<ParamsExt>(GetHost(), Params::Count)
                                .configParam(Params::Rise, new NumericParam(1.0f, 1000.f, 500.0f, "Rise", "ms"))
                                .configParam(Params::Fall, new NumericParam(1.0f, 1000.f, 500.0f, "Fall", "ms"))
                                .configParam(Params::VibratoRate, new NumericParam(.1f, 10.0f, 1.0f, "Vibrato Rate", "hz"))
                                .configParam(Params::VibratoDepth, new NumericParam(0.0f, 200.0f, 50.0f, "Vibrato Depth", "cents"))
                                .configParam(Params::Portamento, new NumericParam(1.0f, 100.0f, 10.0f, "Portamento", "ms"))
                                .configParam(Params::Polyphony, new IntegerParam(1, 16, 8, "Polyphony", "voices", "voice"));
        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace Sines
