#include <daisy.h>
#include <daisy_patch_sm.h>
#include <cstdint>

#include <kitdsp/approx.h>
#include <kitdsp/dbMeter.h>
#include <kitdsp/dcBlocker.h>
#include <kitdsp/onePole.h>
#include <kitdsp/util.h>

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

Algorithm algorithm = Algorithm::Tanh;

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
        return tanhf(in);
    }
    case Algorithm::Fold:
    {
        // input: [-.5, .5] out [-1, 1]
        return sinf(in * 0.5 * cTwoPi);
    }
    case Algorithm::Rectify:
    {
        return fabsf(in) * 2.0f - 1.0f;
    }
    case Algorithm::Count:
    {
        return in;
    }
    }
    return in;
}

// lazy tone control
// TODO: this was too lazy, time to find a nice shelf filter
class ToneFilter
{
public:
    ToneFilter(float sampleRate)
    {
        mPole1.SetFrequency(1600.0f, sampleRate);
    }
    float Process(float in, float tone)
    {
        float lowpass = mPole1.Process(in);
        return lerpf(lowpass, in, tone);
    }
    kitdsp::OnePole mPole1;
};

DaisyPatchSM hw;
Switch button, toggle;

constexpr float cSampleRate = 96000.0f;
constexpr auto cSamplerateEnum = SaiHandle::Config::SampleRate::SAI_96KHZ;
kitdsp::DcBlocker dcLeft;
kitdsp::DcBlocker dcRight;
ToneFilter tonePreLeft(cSampleRate);
ToneFilter tonePreRight(cSampleRate);
ToneFilter tonePostLeft(cSampleRate);
ToneFilter tonePostRight(cSampleRate);

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

    float gain = kitdsp::dbToRatio(lerpf(0.f, 6.0f, knobValue(CV_1)));
    float tone = lerpf(.0f, 1.0f, knobValue(CV_2));
    float makeup = kitdsp::dbToRatio(lerpf(-2.f, 4.f, knobValue(CV_3)));
    float mix = lerpf(.0f, 1.0f, knobValue(CV_4));

    if (button.RisingEdge())
    {
        algorithm = static_cast<Algorithm>((static_cast<int32_t>(algorithm) + 1) % static_cast<int32_t>(Algorithm::Count));
    }

    float dbfs;
    for (size_t i = 0; i < size; i++)
    {
        float left = IN_L[i];
        float right = IN_R[i];
        float &outleft = OUT_L[i];
        float &outright = OUT_R[i];
        if (toggle.Pressed())
        {
            // bypass
            outleft = left;
            outright = right;
            continue;
        }

        float leftCrunched = dcLeft.Process(crunch(left * gain));
        float rightCrunched = dcRight.Process(crunch(right * gain));

        outleft = lerpf(left, leftCrunched, mix) * makeup;
        outright = lerpf(right, rightCrunched, mix) * makeup;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(cSamplerateEnum);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    hw.StartAudio(AudioCallback);

    const int32_t unit = 150;
    for (;;)
    {
        // visualize algorithm
        const int32_t num = static_cast<int32_t>(algorithm) + 1;
        for (int i = 0; i < num; ++i)
        {
            hw.WriteCvOut(CV_OUT_2, 1.8f);
            hw.Delay(unit);
            hw.WriteCvOut(CV_OUT_2, 0.0f);
            hw.Delay(unit);
        }

        // wait space length of time
        hw.WriteCvOut(CV_OUT_2, 0.0f);
        hw.Delay(7 * unit);
    }
}
