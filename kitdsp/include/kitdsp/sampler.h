#pragma once

#include <etl/span.h>
#include <cstddef>
#include <cstdint>
#include "kitdsp/macros.h"
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
template <typename SAMPLE>
class Sampler1D {
   public:
    explicit Sampler1D(etl::span<SAMPLE> buf, float sampleRate) : mBuffer(buf), mBufSampleRate(sampleRate) {}

    template <bool SHOULD_LOOP = false>
    SAMPLE Read(int32_t index) const {
        size_t size = mBuffer.size();
        if (size == 0) {
            return SAMPLE(0);
        } else if (SHOULD_LOOP) {
            return mBuffer[index % size];
        } else {
            return index < 0 || index >= narrow_cast<int32_t>(size) ? SAMPLE(0) : mBuffer[index];
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY, bool SHOULD_LOOP, bool LOOP_BACKWARDS>
    SAMPLE Read(float sampleIndex) const {
        // assumption: sample is exactly one period long
        int32_t idx = narrow_cast<int32_t>(sampleIndex);
        float frac = sampleIndex - narrow_cast<float>(idx);

        using namespace interpolate;
        switch (STRATEGY) {
            case InterpolationStrategy::None: {
                return Read<SHOULD_LOOP>(idx);
            };
            case InterpolationStrategy::Linear: {
                if (LOOP_BACKWARDS) {
                    return linear(Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx - 1), frac);
                } else {
                    return linear(Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx + 1), frac);
                }
            };
            case InterpolationStrategy::Hermite: {
                if (LOOP_BACKWARDS) {
                    return hermite4pt3oX(Read<SHOULD_LOOP>(idx + 1), Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx - 1),
                                         Read<SHOULD_LOOP>(idx - 2), frac);
                } else {
                    return hermite4pt3oX(Read<SHOULD_LOOP>(idx - 1), Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx + 1),
                                         Read<SHOULD_LOOP>(idx + 2), frac);
                }
            };
            case InterpolationStrategy::Cubic: {
                if (LOOP_BACKWARDS) {
                    return cubic(Read<SHOULD_LOOP>(idx + 1), Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx - 1),
                                 Read<SHOULD_LOOP>(idx - 2), frac);
                } else {
                    return cubic(Read<SHOULD_LOOP>(idx - 1), Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx + 1),
                                 Read<SHOULD_LOOP>(idx + 2), frac);
                }
            };
        }

        return {};
    }
    size_t GetNumSamples() const { return mBuffer.size(); }
    float GetSampleRate() const { return mBufSampleRate; }

   private:
    etl::span<SAMPLE> mBuffer;
    float mBufSampleRate;
};
}  // namespace kitdsp
