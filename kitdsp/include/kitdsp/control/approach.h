#pragma once

#include <cmath>
#include "kitdsp/math/util.h"

namespace kitdsp {
/**
 * Quick abstraction over an exponential approach algorithm, also known as the "lerp trick" in gamedev circles:
 * https://www.youtube.com/watch?v=LSNQuFEDOyQ
 */
class Approach {
   public:
    Approach() {}
    ~Approach() = default;
    inline void Reset() {
        target = 0.0f;
        current = 0.0f;
    }
    inline void SetHalfLife(float halfLifeMs, float sampleRate) {
        if (halfLifeMs == 0.0f || sampleRate == 0.0f) {
            // instant approach
            mH = 0.0f;
        } else {
            // https://mastodon.social/@acegikmo/111931613710775864

            // doing some algebra to:
            // float dt = 1 / sampleRate;
            // float h = halfLifeMs / 1000.0f;
            // mH = std::exp2(-dt / h);
            mH = 1.0f - std::exp2(-1000.0f / (sampleRate * halfLifeMs));
        }
    }
    /**
     * Sets the approach factor by time until IsChanging() == false. For this to be precise, we need to know the initial
     * time, because the approach is always proportional to that.
     */
    inline void SetSettleTime(float settleTimeMs, float sampleRate, float desiredChange = 1.0f) {
        return SetHalfLife(-settleTimeMs / std::log2f(cSettlePrecision / desiredChange), sampleRate);
    }
    inline float Process() {
        current = lerpf(current, target, mH);
        return current;
    }
    inline bool IsChanging() const { return fabsf(target - current) > cSettlePrecision; }
    float target = 0.0f;
    float current = 0.0f;

   private:
    static constexpr float cSettlePrecision = 0.0001f;
    float mH = 1.0f;
};
}  // namespace kitdsp