#pragma once

#include <cmath>
namespace kitdsp {
/**
 * Quick abstraction over an exponential approach algorithm, also known as the "lerp trick" in gamedev circles:
 * https://www.youtube.com/watch?v=LSNQuFEDOyQ
 */
class Approach {
    public:
    Approach() {}
    ~Approach() = default;
    inline void Reset() { mState = 0.0f; }
    inline void SetHalfLife(float halfLifeMs, float sampleRate) {
        if (halfLifeMs == 0.0f || sampleRate == 0.0f) {
            // instant approach
            mH = 1.0f;
        } else {
            // https://mastodon.social/@acegikmo/111931613710775864
            
            // doing some algebra to:
            // float dt = 1 / sampleRate;
            // float h = halfLifeMs / 1000.0f;
            // mH = std::exp2(-dt / h);
            mH = std::exp2(-1000.0f / (sampleRate * halfLifeMs));
        }
    }
    inline float Process(float in) {
        mState = in + (mState - in) * mH;
        return mState;
    }
    private:
    float mState = 0.0f;
    float mH = 1.0f;
};
}  // namespace kitdsp