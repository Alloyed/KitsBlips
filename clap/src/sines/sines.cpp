#include "sines/sines.h"

#include "clapeze/ext/parameters.h"
#include "clapeze/gui/imguiExt.h"
#include "descriptor.h"
#include "kitdsp/math/util.h"

const PluginEntry Sines::Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                               [](PluginHost& host) -> BasePlugin* { return new Sines(host); }};

void Sines::Config() {
    InstrumentPlugin::Config();
    SinesParamsExt& params = ConfigExtension<SinesParamsExt>(GetHost(), SinesParams::Count)
                                 .configNumeric(SinesParams::Volume, -20.0f, 0.0f, 0.0f, "Volume");
    ConfigProcessor<SinesProcessor>(params.GetStateForAudioThread());
}

void SinesProcessor::ProcessAudio(StereoAudioBuffer& out) {
    for (uint32_t index = 0; index < out.left.size(); index++) {
        mAmplitude = kitdsp::lerpf(mAmplitude, mTargetAmplitude, 0.001);
        float s = mOsc.Process() * mAmplitude;
        out.left[index] = s;
        out.right[index] = s;
    }
}

void SinesProcessor::ProcessNoteOn(const NoteTuple& note, float velocity) {
    // mono, voice stealing
    mTargetAmplitude = .2f;
    mNote = note;
    mOsc.SetFrequency(kitdsp::midiToFrequency(mNote.key), GetSampleRate());
}

void SinesProcessor::ProcessNoteOff(const NoteTuple& note) {
    if (note.Match(mNote)) {
        mTargetAmplitude = 0.0f;
        mNote = {};
    }
}

void SinesProcessor::ProcessNoteChoke(const NoteTuple& note) {
    if (note.Match(mNote)) {
        mAmplitude = 0.0f;
        mTargetAmplitude = 0.0f;
        mNote = {};
    }
}

void SinesProcessor::ProcessReset() {
    mAmplitude = 0.0f;
    mTargetAmplitude = 0.0f;
    mNote = {};
    mOsc.Reset();
}