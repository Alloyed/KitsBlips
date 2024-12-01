#include "daisy_patch_sm.h"

#include <kitdsp/resampler.h>
#include <kitdsp/snesEcho.h>
#include <kitdsp/math/util.h>

#include "kitDaisy/controls.h"

using namespace daisy;
using namespace kitDaisy::controls;
using namespace patch_sm;
using namespace kitdsp;

DaisyPatchSM hw;
Switch button, toggle;
LinearControl wetDryMix{hw.controls[CV_4], &hw.controls[CV_8]};

// constexpr size_t snesBufferSize =
// SNES::Echo::GetBufferDesiredSizeInt16s(SNES::kOriginalSampleRate);
constexpr size_t snesBufferSize = 7680UL;
int16_t DSY_SDRAM_BSS snesBuffer[snesBufferSize];
SNES::Echo snes(snesBuffer, snesBufferSize);
Resampler<float> snesSampler(SNES::kOriginalSampleRate, SNES::kOriginalSampleRate);

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    snes.cfg.echoBufferSize = GetKnob(hw.controls[CV_1]);
    snes.mod.echoBufferSize = GetJack(hw.controls[CV_5]);
    snes.cfg.echoFeedback = GetKnob(hw.controls[CV_2]);
    snes.mod.echoFeedback = GetJack(hw.controls[CV_6]);
    // unused
    //GetKnob(hw.controls[CV_3]);
    //GetJack(hw.controls[CV_7]);

    float wetDry = wetDryMix.Get();
    hw.WriteCvOut(2, 2.5 * wetDry);

    snes.mod.clearBuffer = button.RisingEdge() || hw.gate_in_1.Trig();

    if (toggle.Pressed() || hw.gate_in_2.State()) {
        snes.mod.freezeEcho = 1.0f;
    } else {
        snes.mod.freezeEcho = 0.0f;
    }

    for (size_t i = 0; i < size; i++) {
        // signals are scaled to get a more appropriate clipping level for
        // eurorack's (often very loud) signal values
        float delayed =
            snesSampler
                .Process<InterpolationStrategy::Linear>(
                    IN_L[i] * 0.5f,
                    [](float in, float& out) { out = snes.Process(in); });

        OUT_L[i] = lerpf(IN_L[i], delayed * 2.0f, wetDry);
        OUT_R[i] = -OUT_L[i];
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(8);  // number of samples handled per callback
    hw.SetAudioSampleRate(SNES::kOriginalSampleRate);
    snesSampler = {SNES::kOriginalSampleRate, hw.AudioSampleRate()};

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());

    hw.StartAudio(AudioCallback);
    while (1) {
    }
}
