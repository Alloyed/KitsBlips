#pragma once
#include <array>
#include "kitdsp/math/util.h"
#include "kitdsp/util/macros.h"

namespace kitdsp {
/**
 * An FIR filter is a kind of filter that doesn't need any feeback path, and is instead implemented as a series of coefficients over the last N samples.
 * They are bad for musical control, but good as a workhorse for things that need specifically tuned mathematical filters
 * https://ccrma.stanford.edu/~jos/filters/FIR_Digital_Filters.html
 */
template<typename TSample, size_t N>
class FirFilter {
   public:
    inline void SetCoefs(etl::span<TSample> coefs) {
        mCoefs = coefs;
    }
    inline TSample Process(TSample in) {
        for(size_t idx = 0; idx < N - 1; ++idx) {
            mBuf[idx] = mBuf[idx+1];
        }
        mBuf[N - 1] = in;

        TSample out {};
        for(size_t idx = 0; idx < N; ++idx) {
            out += mBuf[idx] * mCoefs[idx];
        }
        return out;
    }

    inline void Process(etl::span<TSample> inoutBuf) {
        for(TSample& inout : inOutBuf) {
            for(size_t idx = 0; idx < N - 1; ++idx) {
                mBuf[idx] = mBuf[idx+1];
            }
            mBuf[N - 1] = inout;

            TSample out {};
            for(size_t idx = 0; idx < N; ++idx) {
                out += mBuf[idx] * mCoefs[idx];
            }
            inout = out;
        }
    }

    inline void Reset() {
        mBuf = {};
    }

   private:
    std::array<TSample, N> mBuf{};
    std::array<TSample, N> mCoefs;
};
}  // namespace kitdsp