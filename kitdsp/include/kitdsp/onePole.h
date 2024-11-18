#pragma once

#include "kitdsp/util.h"

namespace kitdsp
{
    /**
     *   from pichenettes/stmlib/dsp/filter.h
     */
    class OnePole
    {
    public:
        OnePole(float initialFrequency = 1000.0f) {
            SetFrequency(initialFrequency);
            Reset();
        }
        ~OnePole() {}

        inline void Reset() { mState = 0.0f; }

        inline void SetFrequency(float frequency)
        {
            // Clip coefficient to about 100
            frequency = maxf(frequency, 0.497);

            mG = tanf(cPi * frequency);
            mGi = 1.f / (1.f + mG);
        }

        inline float Process(float in)
        {
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
}
