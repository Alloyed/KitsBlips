#include "sines/sines.h"

#include "clapApi/ext/parameters.h"
#include "clapApi/ext/state.h"
#include "descriptor.h"
#include "kitdsp/math/util.h"

enum Params : ParamId { Params_Count };

const PluginEntry Sines::Entry{AudioInstrumentDescriptor("me.alloyed.sines", "Sines", "a simple sine wave synth"),
                               [](PluginHost& host) -> BasePlugin* { return new Sines(host); }};

void Sines::Config() {
    InstrumentPlugin::Config();
    ConfigExtension<ParametersExt>(Params_Count);
    ConfigExtension<StateExt>();
}

void Sines::ProcessAudio(StereoAudioBuffer& out, ParametersExt::AudioParameters& params) {
    if (mAmplitude < 0.01) {
        // TODO: memset 0 necessary?
        return;
    }

    for (uint32_t index = 0; index < out.left.size(); index++) {
        float sum = 0.0f;

        float s = mOsc.Process();
        out.left[index] = s;
        out.right[index] = s;
    }
}

void Sines::ProcessNoteOn(const NoteTuple& note, float velocity) {
    // mono, voice stealing
    mAmplitude = 1.0f;
    mNote = note;
    mOsc.SetFrequency(kitdsp::midiToFrequency(mNote.key), mSampleRate);
}
void Sines::ProcessNoteOff(const NoteTuple& note) {
    if (note.Match(mNote)) {
        mAmplitude = 0.0f;
        mNote = {};
    }
}
void Sines::ProcessNoteChoke(const NoteTuple& note) {
    mAmplitude = 0.0f;
    mNote = {};
}

bool Sines::Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) {
    mSampleRate = static_cast<float>(sampleRate);
    return true;
}

void Sines::Reset() {
    mAmplitude = 0.0f;
    mNote = {};
    mOsc.Reset();
}