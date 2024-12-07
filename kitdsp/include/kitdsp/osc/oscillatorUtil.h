#pragma once

#include "kitdsp/math/util.h"
#include "kitdsp/oversampler.h"
/**
 * base implementation of common oscillator utils
 */

namespace kitdsp {
class Phasor {
   public:
    void SetFrequency(float frequencyHz, float sampleRate) {
        mAdvance = frequencyHz / sampleRate;
    }
    void HardSync() { mPhase = 0.0f; }
    float GetPhase() { return mPhase; }
    void Reset() { mPhase = 0.0f; mAdvance = 0.0f; }
    static float WrapPhase(float phase) { return phase - floor(phase); }

   protected:
    bool Advance() {
        float nextPhase = mPhase + mAdvance;
        mPhase = WrapPhase(nextPhase);
        return mPhase != nextPhase;
    }
    /** take an arbitrary value and wrap into 0-1 (phasor range) */
    float mPhase = 0.0f;
    float mAdvance = 0.0f;
};

template<typename OSC, size_t FACTOR>
class OversampledOscillator {
    public:
    void SetFrequency(float frequencyHz, float sampleRate) {
        mSampler.SetSampleRate(sampleRate);
        mOsc.SetFrequency(frequencyHz, mSampler.GetTargetSampleRate());
    }
    void HardSync() { mOsc.HardSync(); }
    float GetPhase() { return mOsc.GetPhase(); }
    void Reset() { mOsc.Reset(); mSampler.Reset(); }
    float Process() {
         return mSampler.Process([&](float& out) { out = mOsc.Process(); });
    }
    private:
    OSC mOsc;
    Oversampler<float, FACTOR> mSampler{48000.0f};
};

}  // namespace kitdsp