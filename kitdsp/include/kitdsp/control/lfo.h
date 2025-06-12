#pragma once

#include "kitdsp/math/approx.h"

namespace kitdsp {
namespace lfo {
class Phasor {
   public:
    void SetPeriod(float periodMs, float sampleRate) {
        if (periodMs == 0.0f || sampleRate == 0.0f) {
            // hard locked
            mAdvance = 1000.0f / (sampleRate * periodMs);
        } else {
            mAdvance = 1000.0f / (sampleRate * periodMs);
        }
    }
    void HardSync() { mPhase = 0.0f; }
    float GetPhase() const { return mPhase; }
    void Reset() {
        mPhase = 0.0f;
        mAdvance = 0.0f;
    }
    /** take an arbitrary value and wrap into 0-1 (phasor range) */
    static float WrapPhase(float phase) { return phase - floor(phase); }

   protected:
    bool Advance() {
        float nextPhase = mPhase + mAdvance;
        mPhase = WrapPhase(nextPhase);
        return mPhase != nextPhase;
    }
    float mPhase = 0.0f;
    float mAdvance = 0.0f;
};

class SineOscillator : public Phasor {
   public:
    float Process() {
        Advance();
        return approx::cos2pif_nasty(mPhase);
    }
};

class TriangleOscillator : public Phasor {
   public:
    float Process() {
        Advance();
        // in phase with a cos wave
        return fabsf(mPhase - 0.5f) * 4.0f - 1.0f;
    }
};
}  // namespace lfo
}  // namespace kitdsp