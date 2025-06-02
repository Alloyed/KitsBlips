#include "sines/sines.h"

#include "clapApi/ext/parameters.h"
#include "clapApi/ext/state.h"
#include "descriptor.h"
#include "kitdsp/math/util.h"

enum Params : ParamId { Params_Count };

const PluginEntry Sines::Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                               [](PluginHost& host) -> BasePlugin* { return new Sines(host); }};

void Sines::Config() {
    InstrumentPlugin::Config();
    ConfigExtension<ParametersExt>(Params_Count);
    ConfigExtension<StateExt>();
}

void Sines::ProcessAudio(StereoAudioBuffer& out, ParametersExt::AudioParameters& params) {
    for (uint32_t index = 0; index < out.left.size(); index++) {
        mAmplitude = kitdsp::lerpf(mAmplitude, mTargetAmplitude, 0.001);
        float s = mOsc.Process() * mAmplitude;
        out.left[index] = s;
        out.right[index] = s;
    }
}

void Sines::ProcessNoteOn(const NoteTuple& note, float velocity) {
    // mono, voice stealing
    mTargetAmplitude = .2f;
    mNote = note;
    mOsc.SetFrequency(kitdsp::midiToFrequency(mNote.key), GetSampleRate());
}
void Sines::ProcessNoteOff(const NoteTuple& note) {
    if (note.Match(mNote)) {
        mTargetAmplitude = 0.0f;
        mNote = {};
    }
}
void Sines::ProcessNoteChoke(const NoteTuple& note) {
    if (note.Match(mNote)) {
        mAmplitude = 0.0f;
        mTargetAmplitude = 0.0f;
        mNote = {};
    }
}

void Sines::Reset() {
    mAmplitude = 0.0f;
    mTargetAmplitude = 0.0f;
    mNote = {};
    mOsc.Reset();
}