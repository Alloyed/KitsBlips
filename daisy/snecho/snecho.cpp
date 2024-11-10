#include "daisy_patch_sm.h"

#include "dsp/snes.h"
#include "dsp/util.h"
#include "dsp/resampler.h"

using namespace daisy;
using namespace patch_sm;


DaisyPatchSM hw;
Switch       button, toggle;

constexpr size_t snesBufferSize
    = SNES::Model::GetBufferDesiredSizeInt16s(SNES::kOriginalSampleRate);
int16_t     snesBuffer[snesBufferSize];
SNES::Model snes(SNES::kOriginalSampleRate, snesBuffer, snesBufferSize);
Resampler   snesSampler(SNES::kOriginalSampleRate, SNES::kOriginalSampleRate);

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

    snes.cfg.echoBufferSize = knobValue(CV_1);
    snes.mod.echoBufferSize = jackValue(CV_5);
    snes.cfg.echoFeedback   = knobValue(CV_2);
    snes.mod.echoFeedback   = jackValue(CV_6);
    snes.cfg.filterMix      = knobValue(CV_3);
    snes.mod.filterMix      = jackValue(CV_7);

    // TODO
    snes.cfg.echoDelayMod  = 1.0f;
    snes.mod.echoDelayMod  = 1.0f;
    snes.cfg.filterSetting = 0;
    snes.mod.freezeEcho    = 0.0f;

    float wetDry = clampf(knobValue(CV_4) + jackValue(CV_8), 0.0f, 1.0f);
    hw.WriteCvOut(2, 2.5 * wetDry);

    if(button.RisingEdge() || hw.gate_in_1.Trig())
    {
        snes.ClearBuffer();
    }

    if(toggle.Pressed() || hw.gate_in_2.State())
    {
        snes.mod.freezeEcho = 1.0f;
    }
    else
    {
        snes.mod.freezeEcho = 0.0f;
    }

    for(size_t i = 0; i < size; i++)
    {
        // signals are scaled to get a more appropriate clipping level for eurorack's (often very loud) signal values
        float snesLeft, snesRight;
        snesSampler.Process(
            IN_L[i] * 0.5f,
            IN_R[i] * 0.5f,
            snesLeft,
            snesRight,
            [](float inLeft, float inRight, float &outLeft, float &outRight)
            { snes.Process(inLeft, inRight, outLeft, outRight); });

        OUT_L[i] = lerpf(IN_L[i], snesLeft * 2.0f, wetDry);
        OUT_R[i] = lerpf(IN_R[i], snesRight * 2.0f, wetDry);
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(8); // number of samples handled per callback
    hw.SetAudioSampleRate(SNES::kOriginalSampleRate);
    snesSampler = {SNES::kOriginalSampleRate, hw.AudioSampleRate()};

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while(1) {}
}
