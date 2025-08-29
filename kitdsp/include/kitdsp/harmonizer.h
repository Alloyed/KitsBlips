#pragma once

#include "kitdsp/control/lfo.h"
#include "kitdsp/delayLine.h"
#include "kitdsp/filters/biquad.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
class Harmonizer {
   public:
    Harmonizer(etl::span<float> buffer, float sampleRate);
    void Reset();

    void SetParams(float pitchRatio, float grainSizeMs = 30.0f, float baseDelayMs = 0.0f, float feedback = 0.0f);

    float Process(float in);

    size_t GetMaxGrainSize() const;

    size_t GetEffectiveDelay() const;

   private:
    DelayLine<float> mDelayLine;
    lfo::Phasor mGrainPhasor;
    float mSampleRate;
    float mGrainSizeSamples;
    float mBaseDelaySamples;
    bool mSlowing;
    float mFeedback;
    rbj::BiquadFilter<rbj::BiquadFilterMode::LowPass> mFilterOut;
};
}  // namespace kitdsp
