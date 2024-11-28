#include <daisy.h>
#include <daisy_patch_sm.h>

/**
 * Patch.init() starting template
 */

using namespace daisy;
using namespace patch_sm;

DaisyPatchSM hw;
Switch button, toggle;

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    for (size_t i = 0; i < size; i++) {
        OUT_L[i] = IN_L[i];
        OUT_R[i] = IN_R[i];
    }

    hw.WriteCvOut(CV_OUT_1, 0.0f);
    hw.WriteCvOut(CV_OUT_2, 0.0f);
    hw.gate_out_1.Write(false);
    hw.gate_out_2.Write(false);
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
