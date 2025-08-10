#pragma once

#include "kitdsp/hilbertTransform.h"
#include "kitdsp/osc/oscillatorUtil.h"

namespace kitdsp {
class FrequencyShifter {
   public:
    FrequencyShifter(float sampleRate);
    void Reset();

    float Process(float in);

   private:
    HilbertTransformMonoFloat mHilbert;
    Phasor mPhasor;
    float mSampleRate;
};
}  // namespace kitdsp
