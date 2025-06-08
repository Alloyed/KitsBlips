#include "sines/sines.h"

#include "clapeze/ext/parameters.h"
#include "clapeze/ext/state.h"
#include "clapeze/ext/timerSupport.h"
#include "clapeze/gui/imguiExt.h"
#include "clapeze/gui/imguiHelpers.h"
#include "descriptor.h"
#include "kitdsp/math/util.h"

const PluginEntry Sines::Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                               [](PluginHost& host) -> BasePlugin* { return new Sines(host); }};

void Sines::Config() {
    InstrumentPlugin::Config();
    ConfigExtension<SinesParamsExt>(GetHost(), SinesParams::Count)
        .configNumeric(SinesParams::Volume, -20.0f, 0.0f, 0.0f, "Volume");
    ConfigExtension<StateExt>();
    if (GetHost().SupportsExtension(CLAP_EXT_TIMER_SUPPORT)) {
        ConfigExtension<TimerSupportExt>(GetHost());
    }
    if (GetHost().SupportsExtension(CLAP_EXT_GUI)) {
        ConfigExtension<ImGuiExt>(GetHost(), ImGuiConfig{[this]() { this->OnGui(); }});
    }
}

void Sines::OnGui() {
    BaseParamsExt& params = BaseParamsExt::GetFromPlugin<BaseParamsExt>(*this);
    ImGuiHelpers::displayParametersBasic(params);
}

void Sines::ProcessAudio(StereoAudioBuffer& out, SinesParamsExt::AudioParameters& params) {
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