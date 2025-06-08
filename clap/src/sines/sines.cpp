#include "sines/sines.h"

#include "clapeze/common.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/instrumentPlugin.h"
#include "kitdsp/osc/naiveOscillator.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/gui/imguiExt.h"
#include "descriptor.h"
#include "kitdsp/math/util.h"

namespace sines {
enum class Params : clap_id { Volume, Count };
using ParamsExt = ParametersExt<Params>;

class Processor : public InstrumentProcessor<ParamsExt::ProcessParameters> {
   public:
    Processor(ParamsExt::ProcessParameters& params) : InstrumentProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(StereoAudioBuffer& out) override {
        for (uint32_t index = 0; index < out.left.size(); index++) {
            mAmplitude = kitdsp::lerpf(mAmplitude, mTargetAmplitude, 0.001);
            float s = mOsc.Process() * mAmplitude;
            out.left[index] = s;
            out.right[index] = s;
        }
    }

    void ProcessNoteOn(const NoteTuple& note, float velocity) override {
        // mono, voice stealing
        mTargetAmplitude = .2f;
        mNote = note;
        mOsc.SetFrequency(kitdsp::midiToFrequency(mNote.key), GetSampleRate());
    }

    void ProcessNoteOff(const NoteTuple& note) override {
        if (note.Match(mNote)) {
            mTargetAmplitude = 0.0f;
            mNote = {};
        }
    }

    void ProcessNoteChoke(const NoteTuple& note) override {
        if (note.Match(mNote)) {
            mAmplitude = 0.0f;
            mTargetAmplitude = 0.0f;
            mNote = {};
        }
    }

    void ProcessReset() override {
        mAmplitude = 0.0f;
        mTargetAmplitude = 0.0f;
        mNote = {};
        mOsc.Reset();
    }

   private:
    kitdsp::naive::TriangleOscillator mOsc{};
    float mAmplitude{};
    float mTargetAmplitude{};
    NoteTuple mNote{};
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
