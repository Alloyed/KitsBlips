#pragma once

/**
 * non-bandlimited oscillators
 */
#include <cmath>
#include "kitdsp/osc/oscillatorUtil.h"

namespace kitdsp {
class RampUpOscillator : public Phasor {
    float Process() {
        Advance();
        return mPhase * 2.0f - 1.0f;
    }
};

class RampDownOscillator : public Phasor {
    float Process() {
        Advance();
        return 1.0f - (mPhase * 2.0f);
    }
};

class PulseOscillator : public Phasor {
   public:
    void SetDuty(float mDuty) { mHalfDuty = mDuty * 0.5f; }
    float Process() {
        Advance();
        float phase1 = WrapPhase(mPhase + mHalfDuty);
        float phase2 = WrapPhase(mPhase - mHalfDuty);

        float ramp1 = phase1 * 2.0f - 1.0f;
        float ramp2 = 1.0f - (phase2 * 2.0f);

        return ramp1 + ramp2;
    }

   private:
    float mHalfDuty;
};

class TriangleOscillator : public Phasor {
   public:
    float Process() {
        Advance();
        // in phase with a cos wave
        return fabsf(mPhase - 0.5f) * 4.0f - 1.0f;
    }
};
}  // namespace kitdsp
