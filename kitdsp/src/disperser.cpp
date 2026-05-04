#include "kitdsp/apps/disperser.h"
#include "kitdsp/macros.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/util.h"

namespace kitdsp {

void Disperser::Reset() {
    for (auto& delayLine : mDelayLines) {
        delayLine.Reset();
    }
}

void Disperser::SetParams(float frequencyHz, float sampleRate, float feedback, size_t numFilters) {
    /**
     * the phase response (phi(w), where w is the frequency we're analyzing) of
     * this filter is
     *
     *     atan((g^2-1) * sin(delay w), (g^2+1) * cos(delay w) - 2g)
     *
     */

    mDelay = kitdsp::clamp(sampleRate / frequencyHz, 1.0f, narrow_cast<float>(mDelaySize) - 3.0f);
    numFilters = kitdsp::clamp<size_t>(numFilters, 0, kMaxFilters);

    float g = kitdsp::clamp(feedback, -0.95f, 0.95f);
    if (g == 0) {
        mC1 = 0.0f;
        mC2 = 0.0f;
        mC3 = -1.0f;
    } else {
        mC1 = g;
        mC3 = 1.0f / g;
        mC2 = mC3 - g;
    }
    if (numFilters != mDelayLines.size()) {
        for (int32_t i = narrow_cast<int32_t>(mDelayLines.size()) - 1; i >= narrow_cast<int32_t>(numFilters); --i) {
            mDelayLines.pop_back();
        }
        for (size_t i = mDelayLines.size(); i < numFilters; ++i) {
            mDelayLines.emplace_back(etl::span<float>({&mDelayBuffer[i * mDelaySize], mDelaySize}));
            mDelayLines.back().Reset();
            // TODO: can we copy the state of the previous filter instead of resetting?
        }
    }
}

float Disperser::Process(float startingInput) {
    float out = startingInput;
    for (auto& mid : mDelayLines) {
        float in = out;
        float newMid = in + mC1 * mid.Read<interpolate::InterpolationStrategy::Linear>(mDelay);
        mid.Write(newMid);
        out = (mC2 * newMid) - (mC3 * in);
    }
    return out;
}

void Disperser::ProcessInPlace(etl::span<float> startingInput) {
    for (auto& out : startingInput) {
        for (auto& mid : mDelayLines) {
            float in = out;
            float newMid = in + mC1 * mid.Read<interpolate::InterpolationStrategy::Linear>(mDelay);
            mid.Write(newMid);
            out = (mC2 * newMid) - (mC3 * in);
        }
    }
}
}  // namespace kitdsp