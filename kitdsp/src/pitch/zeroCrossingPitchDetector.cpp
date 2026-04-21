#include "kitdsp/pitch/zeroCrossingPitchDetector.h"

namespace kitdsp {
namespace pitch {
ZeroCrossingPitchDetector::ZeroCrossingPitchDetector(float sampleRate) : mSampleRate(sampleRate) {
    mSmoothedPeriod.SetHalfLife(0.001, mSampleRate);
    Reset();
}

void ZeroCrossingPitchDetector::Reset() {
    mSmoothedPeriod.Reset();
}

void ZeroCrossingPitchDetector::Process(etl::span<float> input) {
    for (float f : input) {
        Process(f);
    }
}

void ZeroCrossingPitchDetector::Process(float input) {
    if (mLastSample < 0.0f && input >= 0.0f) {
        mSmoothedPeriod.target = mNextPeriod;
        mNextPeriod = 0;
    }

    mLastSample = input;
    mNextPeriod++;
    mSmoothedPeriod.Process();
}

float ZeroCrossingPitchDetector::GetFrequency() const {
    return mSmoothedPeriod.current == 0.0f ? 0.0f : mSampleRate / mSmoothedPeriod.current;
}
float ZeroCrossingPitchDetector::GetPeriod() const {
    return mSmoothedPeriod.current;
}
}  // namespace pitch
}  // namespace kitdsp