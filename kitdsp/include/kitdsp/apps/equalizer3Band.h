#pragma once

#include "kitdsp/filters/biquad.h"

namespace kitdsp {
class Equalizer3Band {
   public:
    struct Config {
        float lowGainDb = 0.0f;
        float lowFreq = 200.0f;
        float highGainDb = 0.0f;
        float highFreq = 1000.0f;
        float midGainDb = 0.0f;
    };

    Config cfg;

    explicit Equalizer3Band(float sampleRate);

    void Reset();

    float Process(float in);

    float GetMaxDelayMs() const;

   private:
    float mSampleRate;
    rbj::BiquadFilter mLowShelf;
    rbj::BiquadFilter mHighShelf;
};
}  // namespace kitdsp
