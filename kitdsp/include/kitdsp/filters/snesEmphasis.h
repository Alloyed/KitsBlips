#pragma once
#include <array>
#include "kitdsp/math/util.h"
#include "kitdsp/util/macros.h"

namespace kitdsp {
/**
 * This is a preprocessing filter used by SNES sample encoders to emphasize the frequencies that gaussian interpolation will roll off on the other end.
 * source: https://github.com/tildearrow/furnace/blob/47c02e971f34415010a614429f9aebe60fb622fa/src/engine/brrUtils.c#L200
 */
class SnesEmphasisFilter {
   public:
    inline float Process(float in) {
        int16_t inputNorm = static_cast<int16_t>(in * INT16_MAX);
        mEmphasisBuf[0] = mEmphasisBuf[1];
        mEmphasisBuf[1] = mEmphasisBuf[2];
        mEmphasisBuf[2] = inputNorm;

        inputNorm = narrow_cast<int16_t>(clamp<int32_t>(
            ((mEmphasisBuf[1] << 11) - mEmphasisBuf[0] * 370 - mEmphasisBuf[2] * 374) / 1305, INT16_MIN, INT16_MAX));
        return narrow_cast<float>(inputNorm) / INT16_MAX;
    }

    inline void Reset() {
        mEmphasisBuf = {};
    }

   private:
    std::array<int16_t, 3> mEmphasisBuf{};
};
}  // namespace kitdsp