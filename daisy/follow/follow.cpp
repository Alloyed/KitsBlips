#include "daisy_patch_sm.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

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
Switch       button, toggle;
float        state = 0.0f;
float        dt    = 0;

inline constexpr float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline constexpr float clampf(float in, float min, float max)
{
    return in > max ? max : in < min ? min : in;
}

inline constexpr float dampf(float a, float b, float halflife, float dt)
{
    //https://x.com/FreyaHolmer/status/1757836988495847568
    return b + (a - b) * exp2f(-dt / halflife);
}

float knobValue(int32_t cvEnum)
{
    return clampf(hw.controls[cvEnum].Value(), 0.0f, 1.0f);
}

float jackValue(int32_t cvEnum)
{
    return clampf(hw.controls[cvEnum].Value(), -1.0f, 1.0f);
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    // units are ms until half life, exponential curve
    float rise = lerpf(
        0.1f,
        5.0f,
        powf(clampf(knobValue(CV_1) + jackValue(CV_5), 0.0f, 1.0f), 2.0f));
    float fall = lerpf(
        0.1f,
        5000.0f,
        powf(clampf(knobValue(CV_2) + jackValue(CV_6), 0.0f, 1.0f), 2.0f));
    float gain = powf(
        lerpf(
            0.8f, 5.0f, clampf(knobValue(CV_3) + jackValue(CV_7), 0.0f, 1.0f)),
        4.0f);

    for(size_t i = 0; i < size; i++)
    {
        float input = fabsf(IN_L[i]) * gain;
        state       = dampf(state, input, input > state ? rise : fall, dt);
        OUT_L[i]    = state;
        if(i == size - 1)
        {
            hw.WriteCvOut(CV_OUT_2, OUT_L[i] * 5.0f);
            dsy_gpio_write(&hw.gate_out_1, OUT_L[i] > 0.3f);
        }
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    dt = 1000.0f / hw.AudioCallbackRate();

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while(1) {}
}
