#pragma once

#include "kitdsp/math/interpolate.h"
#include "kitdsp/osc/oscillatorUtil.h"
#include "kitdsp/sampler.h"

namespace kitdsp {
template <typename SAMPLE, interpolate::InterpolationStrategy STRATEGY>
class WavetableOscillator : public Phasor {
   public:
    explicit WavetableOscillator(etl::span<SAMPLE> buffer) : mPlayer(buffer) {}
    float Process() {
        Advance();
        // assumption: sample is exactly one period long
        float sampleIndex = mPhase * mPlayer.size();
        return mPlayer.Read(sampleIndex);
    }

   private:
    Sampler1D<SAMPLE, STRATEGY, true> mPlayer;
};
}  // namespace kitdsp