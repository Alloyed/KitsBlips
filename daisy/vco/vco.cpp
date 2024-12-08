#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/osc/blepOscillator.h>
#include <kitdsp/osc/oscillatorUtil.h>
#include "kitDaisy/controls.h"

/**
 * Patch.init() starting vco
 */

using namespace daisy;
using namespace patch_sm;

DaisyPatchSM hw;
Switch button, toggle;

kitdsp::OversampledOscillator<kitdsp::blep::RampUpOscillator, 2> saw;
kitdsp::OversampledOscillator<kitdsp::blep::PulseOscillator, 2> pulse;
kitdsp::OversampledOscillator<kitdsp::blep::TriangleOscillator, 2> tri;
kitdsp::OversampledOscillator<kitdsp::blep::PulseOscillator, 2> sub;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    float freq = 260 * pow(2, kitDaisy::controls::GetKnobN(hw.controls[CV_1]) * 5.0f +
                                  kitDaisy::controls::GetJackN(hw.controls[CV_5]) * 5.0f);
    float duty = 0.5f;

    saw.SetFrequency(freq, hw.AudioSampleRate());
    pulse.SetFrequency(freq, hw.AudioSampleRate());
    tri.SetFrequency(freq, hw.AudioSampleRate());
    sub.SetFrequency(freq * 0.5f, hw.AudioSampleRate());
    pulse.GetOscillator().SetDuty(duty);
    sub.GetOscillator().SetDuty(duty);

    for (size_t i = 0; i < size; i++) {
        float sawf = saw.Process();
        float pulsef = pulse.Process();
        float trif = tri.Process();
        OUT_L[i] = sawf;
        OUT_R[i] = sub.Process();
    }
}

int main(void) {
    kitDaisy::controls::ControlCalibrator calibrate{hw.qspi};
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    calibrate.Init();
    calibrate.ApplyControlCalibration(hw);

    hw.StartAudio(AudioCallback);

    for (;;) {
    }
}
