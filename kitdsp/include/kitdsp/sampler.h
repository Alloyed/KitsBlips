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
    explicit Sampler1D(etl::span<SAMPLE> buf) : mBuffer(buf) {}

    template <bool SHOULD_LOOP>
    SAMPLE Read(int32_t index) const {
        size_t size = mBuffer.size();
        if (SHOULD_LOOP) {
            return mBuffer[index % size];
        } else {
            return index < 0 || index >= narrow_cast<int32_t>(size) ? SAMPLE(0) : mBuffer[index];
        }
    }

    template <bool SHOULD_LOOP, interpolate::InterpolationStrategy STRATEGY>
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
                return linear(Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx + 1), frac);
            };
            case InterpolationStrategy::Hermite: {
                return hermite4pt3oX(Read<SHOULD_LOOP>(idx - 1), Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx + 1),
                                     Read<SHOULD_LOOP>(idx + 2), frac);
            };
            case InterpolationStrategy::Cubic: {
                return cubic(Read<SHOULD_LOOP>(idx - 1), Read<SHOULD_LOOP>(idx), Read<SHOULD_LOOP>(idx + 1),
                             Read<SHOULD_LOOP>(idx + 2), frac);
            };
        }
    }
    size_t size() const { return mBuffer.size(); }

   private:
    etl::span<SAMPLE> mBuffer;
};

}  // namespace kitdsp
