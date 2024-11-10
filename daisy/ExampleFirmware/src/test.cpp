#include "daisy_patch_sm.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM hw;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{

    // Assign Output Buffers
    float *out_left, *out_right;
    out_left  = out[0];
    out_right = out[1];

    hw.ProcessDigitalControls();
    hw.ProcessAnalogControls();

    // Synthesis.
    for(size_t i = 0; i < size; i++)
    {
        // Output
        out_left[i]  = 0.0f;
        out_right[i] = 0.0f;
    }
}

int main(void)
{
    // Init everything.
    float samplerate;
    hw.Init();
    samplerate = hw.AudioSampleRate();

    // Start the ADC and Audio Peripherals on the Hardware
    hw.StartAudio(AudioCallback);

    // Declare a variable to store the state we want to set for the LED.
    bool led_state;
    led_state = true;

    // Loop forever
    for(;;)
    {
        // Set the onboard LED
        hw.SetLed(led_state);

        // Toggle the LED state for the next time around.
        led_state = !led_state;

        // Wait 500ms
        System::Delay(500);
    }
}
