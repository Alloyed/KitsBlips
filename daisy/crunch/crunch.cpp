#include <daisy.h>
#include <daisy_patch_sm.h>
#include <cstdint>

#include <kitdsp/dbMeter.h>
#include <kitdsp/filters/dcBlocker.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/math/approx.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>

#include "kitDaisy/controls.h"

/**
 * multi-algorithm distortion
 */

using namespace kitdsp;
using namespace daisy;
using namespace patch_sm;

enum class Algorithm {
    // base algorithms (similar to renoise's built-in distortion)
    HardClip,
    Tanh,
    Fold,
    Rectify,
    // not an algorithm :)
    Count
};

Algorithm algorithm = Algorithm::Tanh;

float crunch(float in) {
    switch (algorithm) {
        case Algorithm::HardClip: {
            return clamp(in, -1.0f, 1.0f);
        }
        case Algorithm::Tanh: {
            return tanhf(in);
        }
        case Algorithm::Fold: {
            return sinf(in * kPi);
        }
        case Algorithm::Rectify: {
            return fabsf(in) * 2.0f - 1.0f;
        }
        case Algorithm::Count: {
            return in;
        }
    }
    return in;
}

// lazy tone control
// TODO: this was too lazy, time to find a nice shelf filter
class ToneFilter {
   public:
    ToneFilter(float sampleRate) { mPole1.SetFrequency(1200.0f, sampleRate); }
    float Process(float in, float tone) {
        float lowpass = mPole1.Process(in);
        float highpass = in - lowpass;

        // needs to be 2 at 0.5 and 1 at 0, 1, in between is a matter of taste
        float gain = sinf(tone * kPi) + 1.0f;

        return lerp(lowpass, highpass, tone) * gain;
    }
    kitdsp::OnePole mPole1;
};

DaisyPatchSM hw;
Switch button, toggle;
kitDaisy::controls::LinearControl gainDb{hw.controls[CV_1], &hw.controls[CV_5], 0.0f, 16.0f};
kitDaisy::controls::LinearControl tone{hw.controls[CV_2], &hw.controls[CV_6], 0.0f, 1.0f};
kitDaisy::controls::LinearControl makeupGainDb{hw.controls[CV_3], &hw.controls[CV_7], -3.0f, 3.0f};
kitDaisy::controls::LinearControl mix{hw.controls[CV_4], &hw.controls[CV_8], 0.0f, 1.0f};

constexpr float cSampleRate = 96000.0f;
constexpr auto cSamplerateEnum = SaiHandle::Config::SampleRate::SAI_96KHZ;
kitdsp::DcBlocker dcLeft;
kitdsp::DcBlocker dcRight;
ToneFilter tonePreLeft(cSampleRate);
ToneFilter tonePreRight(cSampleRate);
ToneFilter tonePostLeft(cSampleRate);
ToneFilter tonePostRight(cSampleRate);

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    float gain = kitdsp::dbToRatio(gainDb.Get());
    float tonef = tone.Get();
    float makeup = kitdsp::dbToRatio(makeupGainDb.Get());
    float mixf = mix.Get();

    if (button.RisingEdge()) {
        algorithm =
            static_cast<Algorithm>((static_cast<int32_t>(algorithm) + 1) % static_cast<int32_t>(Algorithm::Count));
    }

    float dbfs;
    for (size_t i = 0; i < size; i++) {
        float left = IN_L[i];
        float right = IN_R[i];
        float& outleft = OUT_L[i];
        float& outright = OUT_R[i];
        if (toggle.Pressed()) {
            // bypass
            outleft = left;
            outright = right;
            continue;
        }

        // strong pre tone
        float preTone = lerp(0.1f, 0.9f, tonef);
        float leftPre = tonePreLeft.Process(left, preTone);
        float rightPre = tonePreRight.Process(right, preTone);

        // TODO: different gain curves for different algorithms?
        float crunchedLeft = crunch(leftPre * gain);
        float crunchedRight = crunch(rightPre * gain);

        // weak post tone
        float postTone = lerp(0.4f, 0.6f, tonef);
        float processedLeft = dcLeft.Process(tonePostLeft.Process(crunchedLeft, postTone)) * makeup;
        float processedRight = dcRight.Process(tonePostRight.Process(crunchedRight, postTone)) * makeup;

        outleft = lerp(left, processedLeft, mixf);
        outright = lerp(right, processedRight, mixf);
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(cSamplerateEnum);
    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    hw.StartAudio(AudioCallback);

    const int32_t unit = 150;
    for (;;) {
        // visualize algorithm
        const int32_t num = static_cast<int32_t>(algorithm) + 1;
        for (int i = 0; i < num; ++i) {
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
