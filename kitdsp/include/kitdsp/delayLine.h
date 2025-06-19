#pragma once

#include <cstdint>
#include <cstring>
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
/**
 * Implements storage for delay-line based effects. Write() once per sample, and Read() as much as you'd like. read
 * delays are always relative to the write head.
 * Note that power-of-two SIZE values will usually be more efficient
 */
template <typename SAMPLE, size_t SIZE>
class DelayLine {
   public:
    DelayLine(SAMPLE* buffer) : mBuffer(buffer) { Reset(); }
    ~DelayLine() {}

    void Reset() {
        std::memset(mBuffer, 0, SIZE * sizeof(mBuffer[0]));
        mWriteIndex = 0;
    }

    inline void Write(const SAMPLE sample) {
        mBuffer[mWriteIndex] = sample;
        mWriteIndex = (mWriteIndex - 1 + SIZE) % SIZE;
    }

    inline const SAMPLE Read(int32_t delayIndex) const {
        // non-interpolating read
        return mBuffer[(mWriteIndex + delayIndex) % SIZE];
    }

    template <interpolate::InterpolationStrategy strategy>
    inline const SAMPLE Read(float delay) const {
        // read with interpolation
        int32_t idx = static_cast<int32_t>(delay);
        float frac = delay - static_cast<float>(idx);

        using namespace interpolate;
        switch (strategy) {
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

    inline const SAMPLE Allpass(const SAMPLE sample, size_t delay, const SAMPLE coefficient) {
        SAMPLE read = Read(delay);
        SAMPLE write = sample + coefficient * read;
        Write(write);
        return -write * coefficient + read;
    }

   private:
    size_t mWriteIndex;
    SAMPLE* mBuffer;
};
}  // namespace kitdsp