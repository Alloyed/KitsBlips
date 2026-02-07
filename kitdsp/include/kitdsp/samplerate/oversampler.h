#pragma once

#include <cstddef>
#include "kitdsp/filters/biquad.h"
#include "kitdsp/filters/halfband.h"

namespace kitdsp {
template <typename SAMPLE, size_t FACTOR>
class OversamplerGeneric {
   public:
    explicit OversamplerGeneric(float sourceRate) { SetSampleRate(sourceRate); }

    void Reset() {
        mFilterUp.Reset();
        mFilterDown.Reset();
        SetSampleRate(48000.0f);
    }

    void SetSampleRate(float sourceRate) {
        if (sourceRate != mSampleRate) {
            mSampleRate = sourceRate;
            // sticking to a fixed filter of 20khz
            mFilterUp.SetFrequency(20000.0f, sourceRate * FACTOR);
            mFilterDown.SetFrequency(20000.0f, sourceRate * FACTOR);
        }
    }

    float GetTargetSampleRate() const { return mSampleRate * FACTOR; }

    template <typename F>
    SAMPLE Process(F&& callback) {
        SAMPLE out[FACTOR];
        for (size_t i = 0; i < FACTOR; ++i) {
            callback(out[i]);
        }

        // filter + decimate. only the last sample will be included in the final signal.
        SAMPLE filtered;
        for (size_t i = 0; i < FACTOR; ++i) {
            filtered = mFilterDown.Process(out[i]);
        }

        return filtered;
    }

    template <typename F>
    SAMPLE Process(SAMPLE nativeIn, F&& callback) {
        SAMPLE in[FACTOR];
        SAMPLE out[FACTOR];

        // upsample + filter
        // fill every sample but the first with 0. the filter will see this as high frequency content that will get
        // removed anyways
        for (size_t i = 0; i < FACTOR; ++i) {
            in[i] = mFilterUp.Process(i == 0 ? nativeIn : 0);
        }

        for (size_t i = 0; i < FACTOR; ++i) {
            callback(in[i], out[i]);
        }

        // filter + decimate. only the last sample will be included in the final signal.
        SAMPLE filtered;
        for (size_t i = 0; i < FACTOR; ++i) {
            filtered = mFilterDown.Process(out[i]);
        }

        return filtered;
    }

   private:
    float mSampleRate{};
    // TODO: smarter filters than these should be used
    // These are IIR filters with a relatively shallow slope, which means to cut off most high frequency content they
    // need to cut off some audible content as well.
    // Ideally we could make a dedicated HalfBandFilter type
    rbj::BiquadFilter<rbj::BiquadFilterMode::LowPass> mFilterUp;
    rbj::BiquadFilter<rbj::BiquadFilterMode::LowPass> mFilterDown;
};

/**
 * optimized filters for 2x oversampling
 */
template <typename SAMPLE>
class Oversampler2x {
   public:
    void Reset() {
        mFilterUp.Reset();
        mFilterDown.Reset();
    }

    template <typename F>
    SAMPLE Process(F&& callback) {
        SAMPLE out[FACTOR];
        for (size_t i = 0; i < FACTOR; ++i) {
            callback(out[i]);
        }

        // filter + decimate. only the last sample will be included in the final signal.
        SAMPLE filtered;
        for (size_t i = 0; i < FACTOR; ++i) {
            filtered = mFilterDown.Process(out[i]);
        }

        return filtered;
    }

    template <typename F>
    SAMPLE Process(SAMPLE nativeIn, F&& callback) {
        SAMPLE in[FACTOR];
        SAMPLE out[FACTOR];

        // upsample + filter
        // fill every sample but the first with 0. the filter will see this as high frequency content that will get
        // removed anyways
        for (size_t i = 0; i < FACTOR; ++i) {
            in[i] = mFilterUp.Process(i == 0 ? nativeIn : 0);
        }

        for (size_t i = 0; i < FACTOR; ++i) {
            callback(in[i], out[i]);
        }

        // filter + decimate. only the last sample will be included in the final signal.
        SAMPLE filtered;
        for (size_t i = 0; i < FACTOR; ++i) {
            filtered = mFilterDown.Process(out[i]);
        }

        return filtered;
    }

   private:
    static constexpr size_t FACTOR = 2;
    HalfbandFilter<1> mFilterUp;
    HalfbandFilter<1> mFilterDown;
};
}  // namespace kitdsp