#include "daisy_patch_sm.h"
#include "daisysp.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM    hw;
VoctCalibration calibrator;
Switch          button;

float onevolt   = 1.0f / 5.0f;
float threevolt = 3.0f / 5.0f;

struct AdcCalibrationSetting
{
    float scale  = 1.0f;
    float offset = 0.0f;
};

struct KitCalibrationSettings
{
    AdcCalibrationSetting adcValues[ADC_LAST];

    bool operator==(KitCalibrationSettings const &right) const
    {
        return std::memcmp(this, &right, sizeof(*this)) == 0;
    };
    bool operator!=(KitCalibrationSettings const &right) const
    {
        return std::memcmp(this, &right, sizeof(*this)) != 0;
    };
};

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();
    for(size_t i = 0; i < size; i++)
    {
        // TODO: calibrate outputs
        OUT_L[i] = onevolt;
        OUT_R[i] = threevolt;
    }
}

int main(void)
{
    hw.Init();
    hw.StartLog(true);

    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.StartAudio(AudioCallback);

    PersistentStorage<KitCalibrationSettings> storage(hw.qspi);
    storage.Init({}, 0); // stored at very start
    KitCalibrationSettings settings = storage.GetSettings();

    for(int32_t pin = CV_5; pin <= CV_8; ++pin)
    {
        hw.PrintLine("----");
        hw.PrintLine("Calibrating control %d, CV_%d", pin, pin - CV_1 + 1);
        float raw[2];
        for(int32_t rawIndex = 0; rawIndex <= 1; ++rawIndex)
        {
            hw.PrintLine("waiting for %s, press and release button when ready",
                         rawIndex ? "3V" : "1V");
            while(!button.FallingEdge())
            {
                hw.ProcessAllControls();
                button.Debounce();
            }
            float saved   = hw.controls[pin].Value();
            raw[rawIndex] = saved;
            hw.PrintLine("%s: expected %f, got %f",
                         rawIndex ? "3V" : "1V",
                         rawIndex ? threevolt : onevolt,
                         saved);
        }
        calibrator.Record(raw[0], raw[1]);
        calibrator.GetData(settings.adcValues[pin].scale,
                           settings.adcValues[pin].offset);
        hw.PrintLine("scale: %f, offset: %f",
                     settings.adcValues[pin].scale,
                     settings.adcValues[pin].offset);
    }
    storage.Save();
    hw.PrintLine("---------");
    hw.PrintLine(
        "Calibration saved! should be automatically loaded by other patches.");
}
