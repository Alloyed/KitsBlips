#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/osc/naiveOscillator.h>

/**
 * Patch.init() starting vco
 */

using namespace daisy;
using namespace patch_sm;

DaisyPatchSM hw;
Switch button, toggle;

// TODO: minbleps
kitdsp::naive::RampUpOscillator saw;
kitdsp::naive::PulseOscillator pulse;
kitdsp::naive::TriangleOscillator tri;
kitdsp::naive::PulseOscillator sub;

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    float freq = 220.0f;
    float duty = 0.5f;
    
    saw.SetFrequency(freq, hw.AudioSampleRate());
    pulse.SetFrequency(freq, hw.AudioSampleRate());
    tri.SetFrequency(freq, hw.AudioSampleRate());
    sub.SetFrequency(freq, hw.AudioSampleRate());
    pulse.SetDuty(duty);
    sub.SetDuty(duty);

    for (size_t i = 0; i < size; i++) {
        float sawf = saw.Process();
        float pulsef = pulse.Process();
        float trif = tri.Process();
        OUT_L[i] = sawf;
        OUT_R[i] = sub.Process();
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    hw.StartAudio(AudioCallback);

    for (;;) {
    }
}
