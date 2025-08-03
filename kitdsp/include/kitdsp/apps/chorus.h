#pragma once

#include "kitdsp/control/lfo.h"
#include "kitdsp/delayLine.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
class Chorus {
   public:
    struct Config {
        /// [0, TMaxTaps] number of taps to apply.
        size_t numTaps = 1;

        /// [0, 1] triangle->saw? maybe
        float lfoShape = 0.0f;
        /// [unbounded] lfo rate
        float lfoRateHz = 1.0f;

        /// [0.0f, GetMaxDelay()] base delay. the LFO will modulate above and below this value
        float delayBaseMs;
        /// [0.0f, GetMaxDelay()] delay amount. will be clamped if delayBase + mod goes over a limit.
        float delayModMs = 1.0f;

        /// [0, 1] feedback level. 1 = 100%
        float feedback = 0.0f;
        /// [0, 1] applies BBD "analog" emulation to the delay line.
        float grunge = 0.5f;
        /// [0, 1] dry/wet mix
        float mix = 0.5f;
    };

    Config cfg;

    Chorus(etl::span<float> buffer, float sampleRate);

    void Reset();

    float_2 Process(float in);

   private:
    DelayLine<float> mDelayLine;
    lfo::TriangleOscillator mLfo;
    float mSampleRate;
};
}  // namespace kitdsp