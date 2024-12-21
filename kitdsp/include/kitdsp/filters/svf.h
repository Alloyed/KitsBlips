/**
 *   from pichenettes/stmlib/dsp/filter.h
 */
#pragma once

#include <cmath>
#include <algorithm>
#include "kitdsp/math/util.h"

namespace kitdsp {

enum class FilterMode {
  LowPass,
  BandPass,
  Highpass
};

/**
 * This is an adaptation of the svf class in stmlib, the most commonly used filter in mutable instruments modules.
 */
class EmileSvf {
   public:
    EmileSvf() {
        SetFrequency(1000.0f, 48000.0f, 100.0f);
        Reset();
    }
    ~EmileSvf() {}

    void Reset() { mState1 = mState2 = 0.0f; }

    /**
     * Set frequency + resonance.
     * @param frequencyHz in hertz
     * @param sampleRate should be the rate Process() is called at
     * @param resonance should be 0.5 (least resonance) to infinity (self-oscillate)
     */
    inline void SetFrequency(float frequencyHz, float sampleRate, float resonance) {
        float ratio = frequencyHz / sampleRate;

        // this is FREQUENCY_EXACT in the og implementation
        // Clip coefficient to about 100
        ratio = min(ratio, 0.497f);
        mG = tanf(kPi * ratio);
        mR = 1.0f / resonance;
        mH = 1.0f / (1.0f + mR * mG + mG * mG);
    }

    template <FilterMode mode>
    inline float Process(float in) {
        float hp, bp, lp;
        ProcessAll(in, hp, bp, lp);

        switch (mode) {
            case FilterMode::LowPass:
                return lp;
            case FilterMode::BandPass:
                return bp;
            case FilterMode::Highpass:
                return hp;
        }
    }

    inline float Process(float in, FilterMode mode) {
        float hp, bp, lp;
        ProcessAll(in, hp, bp, lp);

        switch (mode) {
            case FilterMode::LowPass:
                return lp;
            case FilterMode::BandPass:
                return bp;
            case FilterMode::Highpass:
                return hp;
        }
    }

    inline void ProcessAll(float in, float& hp, float& bp, float& lp) {
        hp = (in - mR * mState1 - mG * mState1 - mState2) * mH;
        bp = mG * hp + mState1;
        mState1 = mG * hp + bp;
        lp = mG * bp + mState2;
        mState2 = mG * bp + lp;
    }

   private:
    float mG;
    float mR;
    float mH;

    float mState1;
    float mState2;
};
}  // namespace kitdsp