#pragma once

#include "kitdsp/osc/oscillatorUtil.h"
#include "kitdsp/sampler.h"
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
template <typename SAMPLE, size_t SIZE, interpolate::InterpolationStrategy STRATEGY>
class WavetableOscillator : public Phasor {
   public:
    WavetableOscillator(SAMPLE* buffer) : mPlayer(buffer, SIZE) {}
    float Process() {
        Advance();
        // assumption: sample is exactly one period long
        float sampleIndex = mPhase * SIZE;
        return mPlayer.Read(sampleIndex);
    }

   private:
    Sampler1D<SAMPLE, STRATEGY, true> mPlayer;
};
}  // namespace kitdsp