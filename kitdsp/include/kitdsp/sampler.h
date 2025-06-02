#pragma once

#include "kitdsp/math/interpolate.h"
#include "kitdsp/osc/oscillatorUtil.h"

namespace kitdsp {
template <typename SAMPLE, interpolate::InterpolationStrategy STRATEGY, bool SHOULD_LOOP>
class Sampler1D {
   public:
    Sampler1D(SAMPLE* buffer, size_t size) : mBuffer(buffer), mSize(size) {}

    SAMPLE Read(int32_t index) const {
        if (SHOULD_LOOP) {
            return mBuffer[index % mSize];
        } else {
            return index < 0 || index >= mSize ? SAMPLE(0) : mBuffer[index];
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
    size_t size() const { return mSize; }

   private:
    SAMPLE* mBuffer;
    size_t mSize;
};

}  // namespace kitdsp
