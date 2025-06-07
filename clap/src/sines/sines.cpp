#include "sines/sines.h"

#include "clapApi/ext/parameters.h"
#include "clapApi/ext/state.h"
#include "clapApi/ext/timerSupport.h"
#include "descriptor.h"
#include "gui/imguiHelpers.h"
#include "gui/sdlImgui.h"
#include "kitdsp/math/util.h"


const PluginEntry Sines::Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                               [](PluginHost& host) -> BasePlugin* { return new Sines(host); }};

void Sines::Config() {
    GetHost().LogSupportMatrix();
    InstrumentPlugin::Config();
    ConfigExtension<SinesParamsExt>(GetHost(), SinesParams::Count)
        .configParam(SinesParams::Volume, -20.0f, 0.0f, 0.0f, "Volume");
    ConfigExtension<StateExt>();
    if(GetHost().SupportsExtension(CLAP_EXT_TIMER_SUPPORT))
    {
        ConfigExtension<TimerSupportExt>(GetHost());
        ConfigExtension<SdlImguiExt>(GetHost(), SdlImguiConfig{[this](){
            auto& params = ParametersExt<clap_id>::GetFromPlugin<ParametersExt<clap_id>>(*this);
            ImGuiHelpers::displayParametersBasic(params);
        }});
    }
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