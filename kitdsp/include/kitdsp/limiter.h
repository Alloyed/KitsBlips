#pragma once

#include <cmath>
#include "kitdsp/math/util.h"

namespace kitdsp {
/**
 * A low-complexity, no-configuration "safety" limiter, derived from:
 * https://github.com/pichenettes/stmlib/blob/master/dsp/limiter.h
 */
template <typename SAMPLE>
class Limiter {
   public:
    Limiter() {}
    ~Limiter() {}

    void Reset() { mPeak = 0.5f; }

    SAMPLE Process(SAMPLE in) {
        // slew rectified input
        float difference = std::abs(in) - mPeak;
        float slope = difference > 0 ? kSlopeUp : kSlopeDown;
        mPeak += difference * slope;

        // reduce gain if peak is above 1.0
        float gain = mPeak <= 1.0f ? 1.0f : 1.0f / mPeak;

        return clamp(in * gain * kPostGain, SAMPLE(-1), SAMPLE(1));
    }

   private:
    float mPeak = 0.5f;
    // slope constants. these are sample-rate dependent, but for this case it doesn't matter much.
    static constexpr float kSlopeUp = 0.05f;
    static constexpr float kSlopeDown = 0.00002f;
    // provides a bit of extra headroom just in case
    static constexpr float kPostGain = 0.8f;
};
}  // namespace kitdsp