#pragma once

#include "kitdsp/control/lfo.h"
#include "kitdsp/delayLine.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
class Chorus {
   public:
    struct Config {
        /// [unbounded] lfo rate
        float lfoRateHz = 1.0f;

        /// [0.0f, GetMaxDelay()] base delay. the LFO will modulate above and below this value
        float delayBaseMs = 8.0f;
        /// [0.0f, GetMaxDelay()] delay amount. will be clamped if delayBase + mod goes over a limit.
        float delayModMs = 2.0f;

        /// [0, 1] feedback level. 1 = 100%
        float feedback = 0.0f;
        /// [0, 1] dry/wet mix
        float mix = 0.5f;
    };

    Config cfg;

    Chorus(etl::span<float> buffer, float sampleRate);

    void Reset();

    float_2 Process(float in);
    float_2 Process(float_2 in);

    float GetMaxDelayMs() const;

   private:
    DelayLine<float> mDelayLine;
    lfo::TriangleOscillator mLfo;
    float mSampleRate;
};
}  // namespace kitdsp
