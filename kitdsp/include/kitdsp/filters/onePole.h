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
        OnePole()
        {
            // pick arbitrary frequency
            SetFrequency(1200.0f, 48000.0f);
            Reset();
        }
        ~OnePole() {}

        inline void Reset() { mState = 0.0f; }

        inline void SetFrequency(float frequencyHz, float sampleRate)
        {
            float ratio = frequencyHz / sampleRate;

            // this is FREQUENCY_EXACT in the og implementation
            // Clip coefficient to about 100
            ratio = minf(ratio, 0.497);
            mG = tanf(cPi * ratio);

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
