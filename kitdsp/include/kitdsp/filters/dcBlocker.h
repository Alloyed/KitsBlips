#pragma once

namespace kitdsp {
/**
 * This is a tiny, one-pole filter that blocks out very low frequencies. if the input signal has a DC offset, this will filter that out.
 * from: https://www.dsprelated.com/freebooks/filters/DC_Blocker.html
 */
class DcBlocker {
   public:
    inline float Process(float in) {
        float out = in - mLastIn + coef * mLastOut;
        mLastIn = in;
        mLastOut = out;
        return out;
    }

    inline void Reset() {
        mLastIn = 0.0f;
        mLastOut = 0.0f;
    }

   private:
    float mLastIn{};
    float mLastOut{};
    static constexpr float coef = 0.995f; // called R in the source
};
}  // namespace kitdsp