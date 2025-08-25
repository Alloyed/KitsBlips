#pragma once

#include "kitdsp/hilbertTransform.h"
#include "kitdsp/osc/oscillatorUtil.h"

namespace kitdsp {
class FrequencyShifter {
   public:
    explicit FrequencyShifter(float sampleRate);
    void Reset();

    void SetFrequencyOffset(float frequencyHz, float sampleRate);
    float Process(float in);

   private:
    HilbertTransformMonoFloat mHilbert;
    Phasor mPhasor;
};
}  // namespace kitdsp
