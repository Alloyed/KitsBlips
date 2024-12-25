#pragma once

#include <cstddef>
#include <cstdint>
#include "kitdsp/math/util.h"
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
template<typename SAMPLE>
class Resampler {
    // TODO: we should filter the inputs to avoid aliasing
    // https://dspguru.com/dsp/faqs/multirate/decimation/
   public:
    Resampler(float sourceRate, float targetRate)
        : mPeriod(targetRate / sourceRate) {}

    template <interpolate::InterpolationStrategy strategy, typename F>
    SAMPLE Process(SAMPLE in, F&& callback) {
        mSampleCounter += 1.0f;
        if (mSampleCounter > mPeriod * 10000.0f) {
            // prevent extreme overrun scenarios
            mSampleCounter = mPeriod;
        }

        while (mSampleCounter > mPeriod) {
            // shift
            mOutput[0] = mOutput[1];
            mOutput[1] = mOutput[2];
            mOutput[2] = mOutput[3];

            callback(in, mOutput[3]);
            mSampleCounter -= mPeriod;
        }
        float t = mSampleCounter / mPeriod;

        // Linear interpolation
        using namespace interpolate;
        switch (strategy) {
            case InterpolationStrategy::None: {
                return mOutput[3];
            };
            case InterpolationStrategy::Linear: {
                return linear(mOutput[2], mOutput[3], t);
            };
            case InterpolationStrategy::Hermite: {
                return hermite4pt3oX(mOutput[0], mOutput[1], mOutput[2], mOutput[3], t);
            };
            case InterpolationStrategy::Cubic: {
                return cubic(mOutput[0], mOutput[1], mOutput[2], mOutput[3], t);
            };
        }
    }

   private:
    SAMPLE mOutput[4]{};
    float mSampleCounter{};
    float mPeriod{};
};
}  // namespace kitdsp