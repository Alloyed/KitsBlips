#include "kitdsp/pitch/zeroCrossingPitchDetector.h"

namespace kitdsp {
namespace pitch {

ZeroCrossingPitchDetector::ZeroCrossingPitchDetector() = default;

void ZeroCrossingPitchDetector::Reset() {}

void ZeroCrossingPitchDetector::Process(etl::span<float> input) {}

void ZeroCrossingPitchDetector::Process(float input) {}

float ZeroCrossingPitchDetector::GetFrequency() const {
    return 0.0f;
}
}  // namespace pitch
}  // namespace kitdsp