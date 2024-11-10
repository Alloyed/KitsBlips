#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "ScaleQuantizer.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

/**
 * Quantizer module:
 * Inputs:
 * - C5  CV_1: v/oct input (-5, 5)
 * - C4  CV_2: v/oct transpose (-5, 5) (centered to c4 by default)
 * - C3  CV_3: scale
 * - B7  BUTTON : select transpose root
 * Outputs:
 * - B2  OUT_L: v/oct output
 * - B5  GATE_OUT_1: 10ms trigger on note change
 * Improvements:
 * - make v/oct inputs 0-10v instead (passive mixer with -5v ref in?)
 * - use encoder+led array for scale selection?
 * - built in sample+hold? (ala disting)
 * - midi in for custom scales?
 */

DaisyPatchSM hw;
Switch       updateTransposeRoot;
//ScaleQuantizer quantizer(kChromaticScale, DSY_COUNTOF(kChromaticScale));
ScaleQuantizer quantizer(kPentatonic, DSY_COUNTOF(kPentatonic));
//ScaleQuantizer quantizer(kMajorScale, DSY_COUNTOF(kMajorScale));

struct State
{
    // in
    float input;
    float transpose;
    float scale;

    // out
    int   hasNoteChanged;
    float lastNote;

    // state
    float transposeRoot;
};
State state;

void Controls();
void CvCallback()
{
    hw.ProcessAllControls();
    Controls();

    float inputNote = fmap(state.input, 0.0f, 60.0f);
    /*
    float transposeNote = fmap(state.transpose, 0.0f, 60.0f)
                          - fmap(state.transposeRoot, 0.0f, 60.0f);
    */

    float quantizedNote = quantizer.Process(inputNote);
    if(state.lastNote != quantizedNote)
    {
        state.hasNoteChanged++;
        state.lastNote = quantizedNote;
    }
    float noteOut = (fclamp(quantizedNote, 0.f, 60.f) / 60.0f) * 5.0f;
    hw.WriteCvOut(CV_OUT_1, noteOut);
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    //hw.StartDac(CvCallback);
    int32_t triggerActive = -1;
    while(1)
    {
        CvCallback();

        if(state.hasNoteChanged > 0)
        {
            // trigger out
            // note changes are queued in case they happen within the 10ms window
            state.hasNoteChanged--;
            dsy_gpio_write(&hw.gate_out_1, true);
            hw.Delay(9);
            dsy_gpio_write(&hw.gate_out_1, false);
            hw.Delay(1);
        }
    }
}

void Controls()
{
    updateTransposeRoot.Debounce();

    // convert to 0-1
    state.input     = (hw.controls[CV_1].Value() + 1.0f) * 0.5f;
    state.transpose = (hw.controls[CV_2].Value() + 1.0f) * 0.5f;
    if(updateTransposeRoot.RisingEdge())
    {
        state.transposeRoot = state.transpose;
    }
    state.scale = hw.controls[CV_3].Value();
}
