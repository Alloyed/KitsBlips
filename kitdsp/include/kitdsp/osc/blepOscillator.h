#pragma once

/**
 * bandlimited oscillators, implemented using polybleps
 */
#include <cmath>
#include "kitdsp/osc/oscillatorUtil.h"
#include "kitdsp/osc/polyBlep.h"

namespace kitdsp {
namespace blep {

/**
 * like so:
 *    /|
 *   / |
 *  /  |
 *
 */
class RampUpOscillator : public Phasor {
   public:
    float Process() {
        if (Advance()) {
            mBlep.InsertDiscontinuity(mPhase / mAdvance, 1.0f);
        }
        return mBlep.Process(Wave(mPhase));
    }

    void HardSync() {
        float oldPhase = mPhase;
        Phasor::HardSync();
        mBlep.InsertDiscontinuity(0, Wave(oldPhase) - Wave(0));
    }

   private:
    static constexpr float Wave(float phase) { return phase * 2.0f - 1.0f; }
    polyblep::PolyBlepProcessor<float> mBlep;
};

/**
 * like so:
 * ____
 *     |
 *     |____
 *
 * SetDuty() modulates the duty cycle, can be changed in real time.
 */
class PulseOscillator : public Phasor {
   public:
    void SetDuty(float duty) { mDuty = duty; }
    float Process() {
        float oldPhase = mPhase;
        Advance();

        if (oldPhase > mPhase) {
            // crossed 0
            mBlep.InsertDiscontinuity(mPhase / mAdvance, 1.0f);
        } else if (mPhase > mDuty && oldPhase <= mDuty) {
            // crossed midpoint
            mBlep.InsertDiscontinuity((mPhase - mDuty) / mAdvance, -1.0f);
        }
        return mBlep.Process(Wave(mPhase, mDuty));
    }

    void HardSync() {
        float oldPhase = mPhase;
        Phasor::HardSync();
        float oldSample = Wave(oldPhase, mDuty);
        const float newSample = Wave(0.0f, 0.5f);  // duty cycle doesn't matter, can precompute
        if (oldSample != newSample) {
            mBlep.InsertDiscontinuity(0, newSample - oldSample);
        }
    }

   private:
    static constexpr float Wave(float phase, float duty) { return phase < duty ? 1.0f : -1.0f; }
    float mDuty = 0.5f;
    polyblep::PolyBlepProcessor<float> mBlep;
};

/**
 * like so:
 * \    /
 *  \  /
 *   \/
 * In phase with a cos wave
 */
class TriangleOscillator : public Phasor {
   public:
    float Process() {
        float oldPhase = mPhase;
        Advance();

        // triangle waves are first-order discontinuous, the waveform doesn't suddenly jump, but its slope does.
        // fear not! the InsertIntegratedDiscontinuity() should cover us.
        if (oldPhase > mPhase) {
            // crossed 0, reflect down
            mBlep.InsertIntegratedDiscontinuity(mPhase / mAdvance, -1.0f);
        } else if (mPhase > cMidpoint && oldPhase <= cMidpoint) {
            // crossed midpoint, reflect up
            mBlep.InsertIntegratedDiscontinuity((mPhase - cMidpoint) / mAdvance, 1.0f);
        }

        return mBlep.Process(Wave(mPhase));
    }

    void HardSync() {
        float oldPhase = mPhase;
        Phasor::HardSync();
        float oldSample = Wave(oldPhase);
        float newSample = Wave(0);
        mBlep.InsertDiscontinuity(0, newSample - oldSample);
    }

   private:
    float Wave(float phase) { return fabsf(mPhase - cMidpoint) * 4.0f - 1.0f; }
    polyblep::PolyBlepProcessor<float> mBlep;
    static constexpr float cMidpoint = 0.5f;
};
}  // namespace blep
}  // namespace kitdsp
