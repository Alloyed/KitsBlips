#include <kitdsp/math/util.h>
#include <kitdsp/dbMeter.h>
#include "daisy_patch_sm.h"
#include "kitDaisy/controls.h"
#include <string>

using namespace kitdsp;
using namespace daisy;
using namespace patch_sm;

/**
 * follow is an envelope follower. pitch detection too, eventually(?)
 * Inputs:
 * - CV_1: Env Rise
 * - CV_2: Env Fall
 * - CV_3: Gain
 * - IN_L: audio in
 * Outputs:
 * - OUT_L: env out
 * - GATE_OUT_1: env to gate
 */

DaisyPatchSM hw;
Switch button, toggle;
kitDaisy::controls::LinearControl riseCv{hw.controls[CV_1], &hw.controls[CV_5]};
kitDaisy::controls::LinearControl fallCv{hw.controls[CV_1], &hw.controls[CV_5]};
kitDaisy::controls::LinearControl gainCv{hw.controls[CV_1], &hw.controls[CV_5], -6.0f, 6.0f};
float state = 0.0f;
float dt = 0;

inline constexpr float dampf(float a, float b, float halflife, float dt) {
    // https://x.com/FreyaHolmer/status/1757836988495847568
    return b + (a - b) * exp2f(-dt / halflife);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    hw.ProcessAllControls();

    // units are ms until half life, exponential curve
    float rise = lerpf(0.1f, 5.0f, powf(riseCv.Get(), 2.0f));
    float fall = lerpf(0.1f, 5000.0f, powf(fallCv.Get(), 2.0f));
    float gain = dbToRatio(gainCv.Get());

    for (size_t i = 0; i < size; i++) {
        float input = fabsf(IN_L[i]) * gain;
        state = dampf(state, input, input > state ? rise : fall, dt);
        OUT_L[i] = state;
        if (i == size - 1) {
            hw.WriteCvOut(CV_OUT_2, OUT_L[i] * 5.0f);
            hw.gate_out_1.Write(OUT_L[i] > 0.3f);
        }
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(4);  // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    dt = 1000.0f / hw.AudioSampleRate();

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while (1) {
    }
}
