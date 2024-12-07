#pragma once

#include <array>
#include "kitdsp/math/util.h"

namespace kitdsp {
/**
 *   from pichenettes/stmlib/dsp/filter.h
 */
class OnePole {
   public:
    OnePole() {
        // pick arbitrary frequency
        SetFrequency(1200.0f, 48000.0f);
        Reset();
    }
    ~OnePole() {}

    inline void Reset() { mState = 0.0f; }

    inline void SetFrequency(float frequencyHz, float sampleRate) {
        float ratio = frequencyHz / sampleRate;

        // this is FREQUENCY_EXACT in the og implementation
        // Clip coefficient to about 100
        ratio = min(ratio, 0.497f);
        mG = tanf(kPi * ratio);

        mGi = 1.f / (1.f + mG);
    }

    inline float Process(float in) {
        float lowpass;
        lowpass = (mG * in + mState) * mGi;
        mState = mG * (in - lowpass) + lowpass;
        return lowpass;
    }

   private:
    float mG;
    float mGi;
    float mState;
};

// multiple OnePole filters in series
template <size_t NUM_POLES>
class OnePoleSeries {
   public:
    inline void Reset() {
        for (auto& pole : mPoles) {
            pole.Reset();
        }
    }

    inline void SetFrequency(float frequencyHz, float sampleRate) {
        for (auto& pole : mPoles) {
            pole.SetFrequency(frequencyHz, sampleRate);
        }
    }

    inline float Process(float in) {
        float out = in;
        for (auto& pole : mPoles) {
            out = pole.Process(out);
        }
        return out;
    }

   private:
    std::array<OnePole, NUM_POLES> mPoles;
};
}  // namespace kitdsp
