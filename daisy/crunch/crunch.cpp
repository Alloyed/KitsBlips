#include <daisy.h>
#include <daisy_patch_sm.h>
#include <kitdsp/util.h>
#include <kitdsp/approx.h>
#include <kitdsp/onePole.h>
#include <kitdsp/dbMeter.h>
#include <kitdsp/resampler.h>

/**
 * multi-algorithm distortion
 */

using namespace daisy;
using namespace patch_sm;

enum class Algorithm
{
    // base algorithms (similar to renoise's built-in distortion)
    HardClip,
    Tanh,
    Fold,
    Rectify,
    // not an algorithm :)
    Count
};

Algorithm algorithm;

float crunch(float in)
{
    switch (algorithm)
    {
    case Algorithm::HardClip:
    {
        return clampf(in, -1.0, 1.0);
    }
    case Algorithm::Tanh:
    {
        return kitdsp::approx::tanhf(in);
    }
    case Algorithm::Fold:
    {
        // input: [-.5, .5] out [-1, 1]
        return kitdsp::approx::sin2pif_nasty(in * 0.5);
    }
    case Algorithm::Rectify:
    {
        return fabsf(in) * 2.0f - 1.0f;
    }
    case Algorithm::Count:
    {
        break;
    }
    }
    return in;
}

// lazy tone control
class ToneFilter : public kitdsp::OnePole {
    public:
    ToneFilter() {
        SetFrequency(1200.0f);
    }
    float Process(float in, float tone)
    {
        float lowpass = OnePole::Process(in);
        float highpass = in - lowpass;
        return lerpf(lowpass, highpass, tone);
    }
};

DaisyPatchSM hw;
Switch       button, toggle;

constexpr float cSampleRate = 48000.0f;
constexpr auto cSamplerateEnum = SaiHandle::Config::SampleRate::SAI_48KHZ;
// 8x resampling to avoid aliasing
kitdsp::Resampler resampler(cSampleRate, cSampleRate * 8);
ToneFilter toneLeft;
ToneFilter toneRight;
kitdsp::DbMeter meter;

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
    
    float gain = kitdsp::dbToRatio(lerpf(-6.f, 20.0f, knobValue(CV_1) + jackValue(CV_5)));
    float tone = lerpf(.0f, 1.0f, knobValue(CV_2)+ jackValue(CV_6));
    float makeup = kitdsp::dbToRatio(lerpf(-12.f, 12.f, knobValue(CV_3) + jackValue(CV_7)));
    float mix = lerpf(.0f, 1.0f, knobValue(CV_4) + jackValue(CV_8));

    if(button.RisingEdge()) {
        algorithm = static_cast<Algorithm>(static_cast<int8_t>(algorithm) + 1 % static_cast<int8_t>(Algorithm::Count));
    }

    float dbfs;
    for(size_t i = 0; i < size; i++)
    {
        float left = IN_L[i];
        float right = IN_R[i];
        float leftToned = toneLeft.Process(left, tone);
        float rightToned = toneRight.Process(right, tone);
        float leftCrunched = crunch(leftToned * gain) / gain;
        float rightCrunched = crunch(rightToned * gain) / gain;
        
        float diffLeft = left - leftCrunched;
        dbfs = meter.Process(diffLeft);

        OUT_L[i] = lerpf(left, leftCrunched, mix) * makeup;
        OUT_R[i] = lerpf(right, rightCrunched, mix) * makeup;
    }

    // visualize db
    float mindb = -40.0f;
    float maxdb = 3.0f;
    float led = (clampf(dbfs, mindb, maxdb) - mindb ) / (maxdb - mindb);
    hw.WriteCvOut(CV_OUT_2, led * 5.0f);
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(cSamplerateEnum);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    hw.StartAudio(AudioCallback);

    for (;;) {}
}
