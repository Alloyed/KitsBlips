#pragma once

#include <cmath>
#include "kitdsp/math/util.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
/**
 * A low-complexity, no-configuration "safety" limiter, derived from:
 * https://github.com/pichenettes/stmlib/blob/master/dsp/limiter.h
 */
template <typename TSample>
class SafetyLimiter {
   public:
    void Reset() { mPeak = 0.5f; }

    inline TSample Process(TSample in) {
        // slew rectified input
        float difference = std::abs(in) - mPeak;
        float slope = difference > 0 ? kSlopeUp : kSlopeDown;
        mPeak += difference * slope;

        // reduce gain if peak is above 1.0
        float gain = mPeak <= 1.0f ? 1.0f : 1.0f / mPeak;

        return clamp(in * gain * kPostGain, TSample(-1), TSample(1));
    }

   private:
    float mPeak = 0.5f;
    // slope constants. these are sample-rate dependent, but for this case it doesn't matter much.
    static constexpr float kSlopeUp = 0.05f;
    static constexpr float kSlopeDown = 0.00002f;
    // provides a bit of extra headroom just in case
    static constexpr float kPostGain = 0.8f;
};

template <>
inline float_2 SafetyLimiter<float_2>::Process(float_2 in) {
    // slew rectified input
    float largestIn = in.left;
    if(std::abs(in.left) < std::abs(in.right)) {
        largestIn = in.right;
    }
    float difference = std::abs(largestIn) - mPeak;
    float slope = difference > 0 ? kSlopeUp : kSlopeDown;
    mPeak += difference * slope;

    // reduce gain if peak is above 1.0
    float gain = mPeak <= 1.0f ? 1.0f : 1.0f / mPeak;

    if(gain < 1.0f) {
        // TODO: debug break
        printf("SAFETY LIMITER ACTIVATED\n");
    }

    return (in * gain * kPostGain).map([](float i){return clamp(i, -1.0f, 1.0f);});
}
}  // namespace kitdsp