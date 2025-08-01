#pragma once

#include "kitdsp/control/lfo.h"
#include "kitdsp/delayLine.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
template <size_t TMaxTaps>
class Chorus {
   public:
    struct Config {
        /// [0, TMaxTaps] number of taps to apply.
        size_t numTaps = TMaxTaps;

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

    Chorus(etl::span<float> buffer, float sampleRate) : mDelayLine(buffer), mSampleRate(sampleRate) {}

    void Reset() {
        cfg = {};
        mDelayLine.Reset();
        mLfo.Reset();
    }

    float_2 Process(float in) {
        mLfo.SetFrequency(cfg.lfoRateHz, mSampleRate);

        float_2 out{};
        // TODO: multiple taps
        using namespace kitdsp::interpolate;
        float delayMs = cfg.delayBaseMs + (mLfo.Process() * cfg.delayModMs);
        float delaySamples = delayMs * mSampleRate / 1000.0f;
        float tap = mDelayLine.Read<InterpolationStrategy::Linear>(delaySamples);

        out.left += tap;
        out.right += tap;

        // send input in
        float outMono = (out.left + out.right) * 0.5f;
        mDelayLine.Write(in + (outMono * cfg.feedback));

        return out;
    }

   private:
    DelayLine<float> mDelayLine;
    lfo::TriangleOscillator mLfo;
    float mSampleRate;
};
}  // namespace kitdsp