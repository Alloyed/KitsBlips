#include "daisy_patch_sm.h"

#include "kitdsp/psxReverb.h"
#include "kitdsp/util.h"
#include "kitdsp/resampler.h"

using namespace daisy;
using namespace patch_sm;

DaisyPatchSM hw;
Switch button, toggle;

constexpr size_t psxBufferSize = 65536; // PSX::GetBufferDesiredSizeFloats(PSX::kOriginalSampleRate);
float DSY_SDRAM_BSS psxBuffer[psxBufferSize];
PSX::Model psx(PSX::kOriginalSampleRate, psxBuffer, psxBufferSize);
Resampler psxSampler(PSX::kOriginalSampleRate, PSX::kOriginalSampleRate);

float knobValue(int32_t cvEnum)
{
    return clampf(hw.controls[cvEnum].Value(), 0.0f, 1.0f);
}

float jackValue(int32_t cvEnum)
{
    return clampf(hw.controls[cvEnum].Value(), -1.0f, 1.0f);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    // PSX has no parameters yet D:

    float wetDry = clampf(knobValue(CV_4) + jackValue(CV_8), 0.0f, 1.0f);
    hw.WriteCvOut(2, 2.5 * wetDry);

    for (size_t i = 0; i < size; i++)
    {
        float psxLeft, psxRight;
        // signals scaled to compensate for eurorack's (often loud) signal levels
        psxSampler.Process(
            IN_L[i] * 0.5f,
            IN_R[i] * 0.5f,
            psxLeft,
            psxRight,
            Resampler::InterpolationStrategy::Linear,
            [](float inLeft, float inRight, float &outLeft, float &outRight)
            { psx.Process(inLeft, inRight, outLeft, outRight); });

        float left = psxLeft * 2.0f;
        float right = psxRight * 2.0f;

        OUT_L[i] = lerpf(IN_L[i], left, wetDry);
        OUT_R[i] = lerpf(IN_R[i], right, wetDry);
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(8); // number of samples handled per callback
    hw.SetAudioSampleRate(PSX::kOriginalSampleRate);
    psxSampler = {PSX::kOriginalSampleRate, hw.AudioSampleRate()};

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while (1)
    {
    }
}
