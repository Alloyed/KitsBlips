#include "daisy_patch_sm.h"

#include "kitdsp/math/vector.h"
#include "kitdsp/psxReverb.h"
#include "kitdsp/resampler.h"
#include "kitdsp/util.h"

using namespace daisy;
using namespace patch_sm;
using namespace kitdsp;

DaisyPatchSM hw;
Switch button, toggle;

constexpr size_t psxBufferSize =
    65536;  // PSX::GetBufferDesiredSizeFloats(PSX::kOriginalSampleRate);
float DSY_SDRAM_BSS psxBuffer[psxBufferSize];
PSX::Reverb psx(PSX::kOriginalSampleRate, psxBuffer, psxBufferSize);
Resampler<float_2> psxSampler(PSX::kOriginalSampleRate,
                             PSX::kOriginalSampleRate);

float knobValue(int32_t cvEnum) {
    return clampf(hw.controls[cvEnum].Value(), 0.0f, 1.0f);
}

float jackValue(int32_t cvEnum) {
    return clampf(hw.controls[cvEnum].Value(), -1.0f, 1.0f);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    // PSX has no parameters yet D:

    float wetDry = clampf(knobValue(CV_4) + jackValue(CV_8), 0.0f, 1.0f);
    hw.WriteCvOut(2, 2.5 * wetDry);

    for (size_t i = 0; i < size; i++) {
        float psxLeft, psxRight;
        // signals scaled to compensate for eurorack's (often loud) signal
        // levels
        float_2 dry = float_2{IN_L[i] * 0.5f, IN_R[i] * 0.5f};
        float_2 wet =
            psxSampler
                .Process<kitdsp::InterpolationStrategy::Linear>(
                    dry,
                    [](float_2 in, float_2& out) { out = psx.Process(in); });

        float left = psxLeft * 2.0f;
        float right = psxRight * 2.0f;

        OUT_L[i] = lerpf(IN_L[i], left, wetDry);
        OUT_R[i] = lerpf(IN_R[i], right, wetDry);
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(8);  // number of samples handled per callback
    hw.SetAudioSampleRate(kitdsp::PSX::kOriginalSampleRate);
    psxSampler = {kitdsp::PSX::kOriginalSampleRate, hw.AudioSampleRate()};

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while (1) {
    }
}
