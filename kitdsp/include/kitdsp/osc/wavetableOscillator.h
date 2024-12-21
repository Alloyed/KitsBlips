#pragma once

#include <cmath>
#include "kitdsp/osc/oscillatorUtil.h"
#include "kitdsp/math/interpolate.h"

namespace kitdsp {
template <typename SAMPLE, size_t SIZE, interpolate::InterpolationStrategy STRATEGY>
class WavetableOscillator : public Phasor {
   public:
    WavetableOscillator(SAMPLE* buffer) : mBuffer(buffer) {}
    float Process() {
        Advance();
        // assumption: sample is exactly one period long
        float phase = mPhase * SIZE;
        size_t idx = static_cast<size_t>(phase);
        float frac = phase - static_cast<float>(idx);

        switch (STRATEGY) {
            case InterpolationStrategy::None: {
                return Read(idx);
            };
            case InterpolationStrategy::Linear: {
                return interpolate::linear(Read(idx), Read(idx+1), frac);
            };
            case InterpolationStrategy::Hermite: {
                return interpolate::hermite4pt3oX(Read(idx - 1), Read(idx), Read(idx + 1), Read(idx + 2), frac);
            };
            case InterpolationStrategy::Cubic: {
                return interpolate::cubic(Read(idx - 1), Read(idx), Read(idx + 1), Read(idx + 2), frac);
            };
        }
    }

   private:
    SAMPLE Read(int32_t index) { return mBuffer[index % SIZE]; }
    SAMPLE* mBuffer;
};

}  // namespace kitdsp
