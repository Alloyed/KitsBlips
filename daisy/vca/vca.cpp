#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/dbMeter.h>
#include <kitdsp/math/util.h>
#include "kitDaisy/controls.h"

/**
 * Patch.init() starting vca
 */

using namespace daisy;
using namespace kitdsp;
using namespace kitDaisy::controls;
using namespace patch_sm;

DaisyPatchSM hw;
Switch button, toggle;
kitdsp::DbMeter meter(0.0f);
kitDaisy::controls::LinearControl baseGain1(hw.controls[CV_1], nullptr, 0.0f, 1.0f);
kitDaisy::controls::LinearControl baseGain2(hw.controls[CV_2], nullptr, 0.0f, 1.0f);
kitDaisy::controls::AttenuvertedControl vc1(nullptr, hw.controls[CV_3], hw.controls[CV_5], 0.0f, 1.0f);
kitDaisy::controls::AttenuvertedControl vc2(nullptr, hw.controls[CV_4], hw.controls[CV_7], 0.0f, 1.0f);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    float basegain1 = powf(GetKnobN(hw.controls[CV_1]), 2);
    float basegain2 = powf(GetKnobN(hw.controls[CV_2]), 2);
    float vcgain1 = powf(kitdsp::lerpf(-1.f, 1.f, GetKnobN(hw.controls[CV_3])), 2);
    float vcgain2 = powf(kitdsp::lerpf(-1.f, 1.f, GetKnobN(hw.controls[CV_4])), 2);
    float vc1 = GetJackN(hw.controls[CV_5]);
    float vc2 = GetJackN(hw.controls[CV_7]);
    float gain1 = basegain1 + (vc1 * vcgain1);
    float gain2 = basegain2 + (vc2 * vcgain2);

    float meterOut = 0.0f;

    for (size_t i = 0; i < size; i++) {
        OUT_L[i] = IN_L[i] * gain1;
        OUT_R[i] = IN_R[i] * gain2;
        meterOut = meter.Process(OUT_L[i]);
    }

    hw.WriteCvOut(CV_OUT_1, kitdsp::dbToRatio(meterOut));
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(1);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    meter = kitdsp::DbMeter(hw.AudioSampleRate());
    hw.StartAudio(AudioCallback);

    for (;;) {
    }
}
