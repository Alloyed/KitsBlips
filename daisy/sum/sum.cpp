#include "daisy_patch_sm.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

/**
 * sum is a simple cv/audio/gate mixer:
 * Inputs:
 * - GATE_IN_1 / GATE_IN_2: inputs
 * - CV_1 / CV_2 / CV_3 / CV_4: attenuverters
 * - CV_5 / CV_6 / CV_7 / CV_8: inputs
 * - IN_L / IN_R : inputs
 * Outputs:
 * - OUT_L: audio mix
 * - OUT_R: CV mix
 * - GATE_OUT_1: gate mix
 * - CV_OUT_2: LED out
 */


DaisyPatchSM hw;
Switch       button, toggle;

inline constexpr float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline constexpr float clampf(float in, float min, float max)
{
    return in > max ? max : in < min ? min : in;
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
    button.Debounce();
    toggle.Debounce();

    float cv1 = lerpf(-1.0f, 1.0f, knobValue(CV_1)) * jackValue(CV_5);
    float cv2 = lerpf(-1.0f, 1.0f, knobValue(CV_2)) * jackValue(CV_6);
    float cv3 = lerpf(-1.0f, 1.0f, knobValue(CV_3)) * jackValue(CV_7);
    float cv4 = lerpf(-1.0f, 1.0f, knobValue(CV_4)) * jackValue(CV_8);

    bool gate_or  = hw.gate_in_1.State() || hw.gate_in_2.State();
    bool gate_and = hw.gate_in_1.State() && hw.gate_in_2.State();

    for(size_t i = 0; i < size; i++)
    {
        OUT_L[i] = clampf(IN_L[i] + IN_R[i], -1.0f, 1.0f);
        OUT_R[i] = clampf(cv1 + cv2 + cv3 + cv4, -1.0f, 1.0f);
        if(i == size - 1)
        {
            size_t idx = toggle.Pressed() ? 1 : 0;
            hw.WriteCvOut(CV_OUT_2, (out[idx][i] + 1.0f) * 2.5f);
            dsy_gpio_write(&hw.gate_out_1, gate_or);
            dsy_gpio_write(&hw.gate_out_2, gate_and);
        }
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while(1) {}
}
