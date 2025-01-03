#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/ScaleQuantizer.h>
#include <kitdsp/math/util.h>
#include "kitDaisy/controls.h"

/**
 * Patch.init() starting fourstep
 */

using namespace kitdsp;
using namespace kitDaisy::controls;
using namespace daisy;
using namespace patch_sm;

constexpr size_t cNumSteps = 4;

DaisyPatchSM hw;
Switch button, toggle;
float sequence[cNumSteps] = {0.0f};
size_t indexInternal = 0;
bool hasWrapped = false;
kitdsp::ScaleQuantizer quantizer(kitdsp::kChromaticScale, 12);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    // TODO: calibration on these seems way off
    sequence[0] = lerpf(-1.0f, 1.0f, GetKnobN(hw.controls[CV_1]));
    sequence[1] = lerpf(-1.0f, 1.0f, GetKnobN(hw.controls[CV_2]));
    sequence[2] = lerpf(-1.0f, 1.0f, GetKnobN(hw.controls[CV_3]));
    sequence[3] = lerpf(-1.0f, 1.0f, GetKnobN(hw.controls[CV_4]));

    if (hw.gate_in_1.Trig() || button.RisingEdge()) {
        size_t lastIndex = indexInternal;
        indexInternal = indexInternal + 1 % cNumSteps;
        hasWrapped = indexInternal < lastIndex;
    }

    if (hw.gate_in_2.Trig()) {
        indexInternal = 0;
    }

    size_t index = (indexInternal + static_cast<size_t>(GetJackN(hw.controls[CV_5]) * cNumSteps + 0.5f)) % 4;

    // hehe
    float outBi = sequence[index];
    float outUni = (sequence[index] + 1.0f) / 2.0f;

    for (size_t i = 0; i < size; i++) {
        // unipolar
        OUT_L[i] = -outUni;
        // bipolar
        OUT_R[i] = -outBi;
    }

    // range: 2 octaves 0-2v
    float noteIn = lerpf(0.0f, 24.0f, outUni);
    float noteOut = clamp(quantizer.Process(noteIn), 0.f, 24.f) * 2.0f / 24.0f;
    hw.WriteCvOut(CV_OUT_1, noteOut);
    hw.WriteCvOut(CV_OUT_2, outUni * 5.0f);
    hw.gate_out_1.Write(outBi > 0.0f);
    hw.gate_out_2.Write(hasWrapped);
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    hw.StartAudio(AudioCallback);

    for (;;) {
    }
}
