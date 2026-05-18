#pragma once

#include <cstddef>
#include "kitdsp/filters/halfband.h"

namespace kitdsp {
/**
 * optimized filters for 0.5x undersampling
 */
template <typename SAMPLE>
class Undersampler2x {
   public:
    void Reset() {
        mFilterUp.Reset();
        mFilterDown.Reset();
    }

    template <typename F>
    SAMPLE Process(F&& callback) {
        float out = 0.0f;
        counter++;

        while (counter >= FACTOR) {
            callback(out);
            counter -= FACTOR;
        }

        // upsample + filter
        // if the processor doesn't run this frame, out is 0. the filter will see this as high frequency content that
        // will get removed anyways
        out = mFilterUp.Process(out);

        return out;
    }

    template <typename F>
    SAMPLE Process(SAMPLE nativeIn, F&& callback) {
        float out = 0.0f;
        counter++;

        float filtered = mFilterDown.Process(nativeIn);
        while (counter >= FACTOR) {
            out = callback(filtered);
            counter -= FACTOR;
        }

        // upsample + filter
        // if the processor doesn't run this frame, out is 0. the filter will see this as high frequency content that
        // will get removed anyways
        out = mFilterUp.Process(out);

        return out;
    }

   private:
    static constexpr size_t FACTOR = 2;
    size_t counter = 0;
    HalfbandFilter<1> mFilterUp;
    HalfbandFilter<1> mFilterDown;
};
}  // namespace kitdsp