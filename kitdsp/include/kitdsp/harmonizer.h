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

    void SetParams(float transpose, size_t grainSize);

    float Process(float in);

    size_t GetMaxGrainSize() const;

    size_t GetEffectiveDelay() const;

   private:
    DelayLine<float> mDelayLine;
    lfo::Phasor mGrainPhasor;
    float mSampleRate;
    rbj::BiquadFilter<rbj::BiquadFilterMode::LowPass> mFilterOut;
};
}  // namespace kitdsp
