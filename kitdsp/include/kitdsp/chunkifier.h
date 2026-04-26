#pragma once

#include <etl/span.h>
#include <cassert>
#include "kitdsp/delayLine.h"
#include "kitdsp/macros.h"

namespace kitdsp {
/**
 * Turns a continuous signal into equally sized chunks (for STFT or other chunk based algorithms).
 */
template <typename TSample>
class Chunkifier {
   public:
    static constexpr size_t GetDesiredBufferSize(size_t chunkLength) { return chunkLength * 2; }
    /**
     * @param buffer should be sized to fit GetDesiredBufferSize()
     * @param chunkLength should be the number of samples to include in a chunk
     * @param overlapSamples should be the number of samples that should be included in the last chunk and the current
     * chunk. 0 means no overlap.
     */
    Chunkifier(etl::span<TSample> buffer, size_t chunkLength, size_t overlapSamples)
        : mBuf({buffer.data(), chunkLength}),
          mChunk(buffer.data() + chunkLength, chunkLength),
          mHopSize(chunkLength - overlapSamples) {
        assert(buffer.size() == GetDesiredBufferSize(chunkLength));
        assert(overlapSamples <= chunkLength);
    }
    void Reset() {
        mBuf.Reset();
        mHopIdx = 0;
    }
    template <typename TFn>
    void Process(TSample input, TFn fn) {
        mBuf.Write(input);
        mHopIdx++;
        if (mHopIdx > mHopSize) {
            // mChunk is necessary here because the delay line might not be continuous
            mBuf.ReadChunk(mChunk.size() - 1, mChunk);
            fn(mChunk);
            mHopIdx -= mHopSize;
        }
    }

   private:
    DelayLine<TSample> mBuf;
    etl::span<TSample> mChunk;
    size_t mHopIdx = 0;
    size_t mHopSize;
};
}  // namespace kitdsp