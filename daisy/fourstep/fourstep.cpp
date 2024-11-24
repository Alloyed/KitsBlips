#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/util.h>
#include <kitdsp/ScaleQuantizer.h>

/**
 * Patch.init() starting fourstep
 */

using namespace daisy;
using namespace patch_sm;

DaisyPatchSM hw;
Switch       button, toggle;
float sequence[4] = {0.0f};
size_t indexInternal = 0;
bool hasWrapped = false;
kitdsp::ScaleQuantizer quantizer(kitdsp::kChromaticScale, 12);

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

    sequence[0] = lerpf(-1.0f, 1.0f, knobValue(CV_1));
    sequence[1] = lerpf(-1.0f, 1.0f, knobValue(CV_2));
    sequence[2] = lerpf(-1.0f, 1.0f, knobValue(CV_3));
    sequence[3] = lerpf(-1.0f, 1.0f, knobValue(CV_4));

    if (hw.gate_in_1.Trig())
    {
        size_t lastIndex = indexInternal;
        indexInternal = indexInternal + 1 % 4;
        hasWrapped = indexInternal < lastIndex;
    }

    if (hw.gate_in_2.Trig())
    {
        indexInternal = 0;
    }

    size_t index = (indexInternal + static_cast<size_t>(jackValue(CV_5) * 4.0f + 0.5f)) % 4;

    // hehe
    float outBi = sequence[index];
    float outUni = (sequence[index] + 1.0f) / 2.0f;

    for(size_t i = 0; i < size; i++)
    {
        // unipolar
        OUT_L[i] = -outUni;
        // bipolar
        OUT_R[i] = -outBi;
    }

    float noteIn = lerpf(0.0f, 60.0f, outUni);
    float noteOut = (clampf(quantizer.Process(noteIn), 0.f, 60.f) / 60.0f) * 5.0f;
    hw.WriteCvOut(CV_OUT_1, noteOut);
    hw.WriteCvOut(CV_OUT_2, outUni * 5.0f);
    hw.gate_out_1.Write(outBi > 0.0f);
    hw.gate_out_2.Write(hasWrapped);
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    hw.StartAudio(AudioCallback);

    for (;;) {}
}
