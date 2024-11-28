#pragma once

namespace kitdsp {
// https://www.dsprelated.com/freebooks/filters/DC_Blocker.html
class DcBlocker {
   public:
    inline float Process(float in) {
        float out = in - mLastIn + 0.995f * mLastOut;
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
};
}  // namespace kitdsp