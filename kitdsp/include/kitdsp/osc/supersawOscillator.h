#include "kitdsp/filters/biquad.h"
#include "kitdsp/osc/oscillatorUtil.h"
#include "kitdsp/samplerate/oversampler.h"

namespace kitdsp {
/**
 * Floating-point reimplementation of the supersaw. TODO: fixed-point "authentic" version
 */
class SuperSawOscillator {
   public:
    void SetFrequency(float frequencyHz, float detune, float sampleRate) {
        for (size_t i = 0; i < kNumSaws; ++i) {
            mSaws[i].SetFrequency(frequencyHz + (kDetuneOffsets[i] * detune), sampleRate);
        }
        mAntiAliasingFilter.SetFrequency(frequencyHz * kFilterCutoffCoef, sampleRate);
    }
    void SetSpread(float spread) { mSpreadAmount = spread; }
    float Process() {
        // center oscillator
        float mixed = mSaws[0].Process();
        // side oscillators
        for (size_t i = 1; i < kNumSaws; ++i) {
            mixed += mSaws[i].Process() * mSpreadAmount;
        }
        return mAntiAliasingFilter.Process(mixed);
    }
    void HardSync() {
        for (size_t i = 0; i < kNumSaws; ++i) {
            mSaws[i].HardSync();
        }
    }
    void Reset() {
        mOversampler.Reset();
        for (size_t i = 0; i < kNumSaws; ++i) {
            mSaws[i].Reset();
        }
    }

   private:
    static constexpr size_t kNumSaws = 7;
    static constexpr float kDetuneOffsets[] = {0.0f,        -0.11002313f, -0.06288439f, -0.01952356f,
                                               0.01991221f, 0.06216538f,  0.10745242f};
    // Since each filter has its own slope, and I'm using an existing off-the-shelf filter from my toolkit, the
    // coefficient is _not_ accurate to the original, but instead tweaked to taste.
    static constexpr float kFilterCutoffCoef = 0.9f;
    float mDetuneAmount{1.0f};
    float mSpreadAmount{1.0f};
    Oversampler2x<float> mOversampler;
    Phasor mSaws[kNumSaws]{};
    rbj::BiquadFilter<rbj::BiquadFilterMode::Highpass> mAntiAliasingFilter;
};
}  // namespace kitdsp

#if 0
/*
 * check out
 * https://youtu.be/XM_q5T7wTpQ?t=1741
 * https://www.adamszabo.com/internet/adam_szabo_how_to_emulate_the_super_saw.pdf
 * important detail this code doesn't capture: this should run 88.2KHz, and the high pass filter should be placed just below the fundamental (to reduce aliasing noise)
 */
int24_t saw[7] = {0};

const int24_t detune_table[7] = { 0, 128, - 128, 816, -824, 1408, 1440 };

int24_t next(int24_t pitch, int24_t spread, int24_t detune) {
    int24_t sum = 0 ;

    for (int i = 0; i < 7; i++) {
        int24_t voice_detune = (detune_table[i] * (pitch * detune)) >> 7;
        saw[i] += pitch + voice_detune;

        if ( i == 0)
            sum += saw[i];
        else
            sum += saw[i] * spread;
    }

    return high_pass(sum);
}
#endif
