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
        float sampleRate = 44100.0f;
    };

    Config cfg;

    Equalizer3Band() = default;

    void Reset();

    float Process(float in);

    float GetMaxDelayMs() const;

   private:
    rbj::BiquadFilter mLowShelf;
    rbj::BiquadFilter mHighShelf;
};
}  // namespace kitdsp
