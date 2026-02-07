#pragma once

#include <etl/span.h>
#include <cstddef>
#include <cstdint>
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
template <typename SAMPLE, interpolate::InterpolationStrategy STRATEGY, bool SHOULD_LOOP>
class Sampler1D {
   public:
    explicit Sampler1D(etl::span<SAMPLE> buf) : mBuffer(buf) {}

    SAMPLE Read(int32_t index) const {
        size_t size = mBuffer.size();
        if (SHOULD_LOOP) {
            return mBuffer[index % size];
        } else {
            return index < 0 || index >= size ? SAMPLE(0) : mBuffer[index];
        }
    }

    SAMPLE Read(float sampleIndex) const {
        // assumption: sample is exactly one period long
        size_t idx = static_cast<size_t>(sampleIndex);
        float frac = sampleIndex - static_cast<float>(idx);

        using namespace interpolate;
        switch (STRATEGY) {
            case InterpolationStrategy::None: {
                return Read(idx);
            };
            case InterpolationStrategy::Linear: {
                return linear(Read(idx), Read(idx + 1), frac);
            };
            case InterpolationStrategy::Hermite: {
                return hermite4pt3oX(Read(idx - 1), Read(idx), Read(idx + 1), Read(idx + 2), frac);
            };
            case InterpolationStrategy::Cubic: {
                return cubic(Read(idx - 1), Read(idx), Read(idx + 1), Read(idx + 2), frac);
            };
        }
    }
    size_t size() const { return mBuffer.size(); }

   private:
    etl::span<SAMPLE> mBuffer;
};

}  // namespace kitdsp
