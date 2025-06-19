#pragma once

#include "kitdsp/samplerate/oversampler.h"
/**
 * base implementation of common oscillator utils
 */

namespace kitdsp {
class Phasor {
   public:
    void SetFrequency(float frequencyHz, float sampleRate) { mAdvance = frequencyHz / sampleRate; }
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
        // true if wrapped, false if not
        return mPhase != nextPhase;
    }
    bool WillWrapNext() { return mPhase + mAdvance > 1.0f; }
    float mPhase = 0.0f;
    float mAdvance = 0.0f;
};

template <typename OSC, size_t FACTOR>
class OversampledOscillator {
   public:
    void SetFrequency(float frequencyHz, float sampleRate) {
        mSampler.SetSampleRate(sampleRate);
        mOsc.SetFrequency(frequencyHz, mSampler.GetTargetSampleRate());
    }
    void HardSync() { mOsc.HardSync(); }
    float GetPhase() const { return mOsc.GetPhase(); }
    void Reset() {
        mOsc.Reset();
        mSampler.Reset();
    }
    float Process() {
        return mSampler.Process([&](float& out) { out = mOsc.Process(); });
    }
    OSC& GetOscillator() { return mOsc; }

   private:
    OSC mOsc;
    Oversampler<float, FACTOR> mSampler{48000.0f};
};

}  // namespace kitdsp
