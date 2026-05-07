#pragma once

#include "etl/vector.h"
#include "kitdsp/delayLine.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
class Disperser {
    static constexpr size_t kMaxFilters = 32;
    using Sample = float;

   public:
    explicit Disperser(etl::span<Sample> buffer) : mDelaySize(buffer.size() / kMaxFilters), mDelayBuffer(buffer) {}
    void Reset();

    void SetParams(float frequencyHz, float sampleRate, float feedback, size_t numFilters);

    Sample Process(Sample in);
    void ProcessInPlace(etl::span<Sample> in);

    float GetLowestFrequency(float sampleRate) const {
        float maxPeriod = narrow_cast<float>(mDelaySize - 2);
        return sampleRate / maxPeriod;
    }
    static constexpr size_t GetMaxFilters() { return kMaxFilters; }

   private:
    // allpass coefficients
    float mC1 = 0.0f;
    float mC2 = 0.0f;
    float mC3 = 0.0f;
    float mDelay = 1;
    size_t mDelaySize;

    etl::vector<DelayLine<Sample>, kMaxFilters> mDelayLines;
    etl::span<Sample> mDelayBuffer;
};
}  // namespace kitdsp
