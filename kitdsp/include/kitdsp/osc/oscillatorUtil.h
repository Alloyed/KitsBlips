#pragma once

#include "kitdsp/util.h"
/**
 * base implementation of common oscillator utils
 */

namespace kitdsp {
    class Phasor
    {
    public:
        void SetFrequency(float frequencyHz, float sampleRate)
        {
            mAdvance = frequencyHz / sampleRate;
        }
        void HardSync()
        {
            mPhase = 0.0f;
        }
        float GetPhase()
        {
            return mPhase;
        }

        /** take an arbitrary value and wrap into 0-1 (phasor range) */
        static inline float WrapPhase(float phase) {
            return phase - floorf(phase);
        }

    protected:
        void Advance()
        {
            mPhase = WrapPhase(mPhase + mAdvance);
        }
        float mPhase;
        float mAdvance;
    };

}