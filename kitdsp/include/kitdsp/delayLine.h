#pragma once

#include <etl/span.h>
#include <cstdint>
#include <cstring>
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
/**
 * Implements storage for delay-line based effects. Write() once per sample, and Read() as much as you'd like. read
 * delays are always relative to the write head, eg 0 is the last sample, 1 is the sample before that, etc
 * Note that power-of-two SIZE values will usually be more efficient
 */
template <typename TSample>
class DelayLine {
   public:
    explicit DelayLine(etl::span<TSample> buffer) : mBuffer(buffer) { Reset(); }

    void Reset() {
        std::memset(mBuffer.data(), 0, mBuffer.size_bytes());
        mWriteIndex = 0;
    }

    inline void Write(const TSample sample) {
        mBuffer[mWriteIndex] = sample;
        size_t size = mBuffer.size();
        mWriteIndex = (mWriteIndex - 1 + size) % size;
    }

    inline const TSample Read(int32_t delayIndex) const {
        // non-interpolating read
        size_t size = mBuffer.size();
        // TODO: if size is a power-of-two this could be a cheap & instead
        // (index + delay) & (size − 1)
        return mBuffer[(mWriteIndex + delayIndex) % size];
    }

    inline size_t Size() const { return mBuffer.size(); }

    template <interpolate::InterpolationStrategy strategy>
    inline const TSample Read(float delay) const {
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

    /**
     * This is a shorcut if you're planning to use your delay as part of an allpass filter.
     */
    inline const TSample Allpass(const TSample sample, size_t delay, const TSample coefficient) {
        TSample read = Read(delay);
        TSample write = sample + coefficient * read;
        Write(write);
        return -write * coefficient + read;
    }

   private:
    size_t mWriteIndex;
    etl::span<TSample> mBuffer;
};
}  // namespace kitdsp
