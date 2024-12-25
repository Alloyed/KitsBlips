#include "daisy_patch_sm.h"
#include "kitDaisy/controls.h"

using namespace daisy;
using namespace patch_sm;

DaisyPatchSM hw;
Switch button;

using Log = daisy::Logger<LOGGER_RTT>;
FixedCapStr<8> buf1;
FixedCapStr<8> buf2;
const char* Float(float f) {
    buf1.Clear();
    buf1.AppendFloat(f, 3, false, false);
    return buf1.Cstr();
}
const char* Float2(float f) {
    buf2.Clear();
    buf2.AppendFloat(f, 3, false, false);
    return buf2.Cstr();
}

float oneVolt = 1.0f / 5.0f;
float threeVolt = 3.0f / 5.0f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    for (size_t i = 0; i < size; i++) {
        // TODO: calibrate outputs
        OUT_L[i] = -oneVolt;
        OUT_R[i] = -threeVolt;
    }
}

void WaitForButton() {
    do {
        hw.ProcessAllControls();
        button.Debounce();
    } while (!button.FallingEdge());
    System::Delay(10);
}

int main(void) {
    hw.Init();
    Log::StartLog(false);
    kitDaisy::controls::ControlCalibrator calibrator{hw.qspi};
    calibrator.Init();

    hw.SetAudioBlockSize(4);  // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    // hw.StartAudio(AudioCallback);

    // TODO: calibrate knobs

    System::Delay(10);
    Log::PrintLine("Current Calibration:");
    for (int32_t pin = CV_5; pin <= CV_8; ++pin) {
        float offset, scale;
        calibrator.GetPinCalibration(pin, scale, offset);
        Log::PrintLine("CV_%d: {scale: %s, offset: %s }", pin - CV_1 + 1, Float(scale), Float2(offset));
    }

    // calibrate jacks
    float rawOneVolt[ADC_LAST];
    float rawThreeVolt[ADC_LAST];
    for (int32_t pin = CV_5; pin <= CV_8; ++pin) {
        Log::PrintLine("----");
        Log::PrintLine("Calibrating control %d, CV_%d", pin, pin - CV_1 + 1);
        Log::PrintLine("waiting for 1V, press and release button when ready");
        WaitForButton();
        rawOneVolt[pin] = hw.controls[pin].Value();
        Log::PrintLine("error: %s, got it!", Float(rawOneVolt[pin] / oneVolt));
    }

    for (int32_t pin = CV_5; pin <= CV_8; ++pin) {
        Log::PrintLine("----");
        Log::PrintLine("Calibrating control %d, CV_%d", pin, pin - CV_1 + 1);
        Log::PrintLine("waiting for 3V, press and release button when ready");
        WaitForButton();
        rawThreeVolt[pin] = hw.controls[pin].Value();
        Log::PrintLine("error: %s, got it!", Float(rawThreeVolt[pin] / threeVolt));
    }

    for (int32_t pin = CV_5; pin <= CV_8; ++pin) {
        calibrator.StorePinCalibration(pin, rawOneVolt[pin], rawThreeVolt[pin]);
        float offset, scale;
        calibrator.GetPinCalibration(pin, scale, offset);
        Log::PrintLine("CV_%d: {scale: %s, offset: %s }", pin - CV_1 + 1, Float(scale), Float2(offset));
    }

    // TODO: calibrate outputs

    calibrator.Save();
    Log::PrintLine("---------");
    Log::PrintLine("Calibration saved! should be automatically loaded by other patches.");
}
