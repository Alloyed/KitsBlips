#pragma once

#include "etl/vector.h"
#include "kitdsp/delayLine.h"

namespace kitdsp {
class Disperser {
    static constexpr size_t kMaxFilters = 32;

   public:
    explicit Disperser(etl::span<float> buffer) : mDelaySize(buffer.size() / kMaxFilters), mDelayBuffer(buffer) {}
    void Reset();

    void SetParams(float frequencyHz, float sampleRate, float feedback, size_t numFilters);

    float Process(float in);
    void ProcessInPlace(etl::span<float> in);

    float GetLowestFrequency(float sampleRate) const {
        float maxPeriod = narrow_cast<float>(mDelaySize - 2);
        return sampleRate / maxPeriod;
    }
    size_t GetMaxFilters() const { return kMaxFilters; }

   private:
    // allpass coefficients
    float mC1 = 0.0f;
    float mC2 = 0.0f;
    float mC3 = 0.0f;
    float mDelay = 1;
    size_t mDelaySize;

    etl::vector<DelayLine<float>, kMaxFilters> mDelayLines;
    etl::span<float> mDelayBuffer;
};
}  // namespace kitdsp
