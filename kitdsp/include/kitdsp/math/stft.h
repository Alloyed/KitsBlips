#pragma once

#include "kitdsp/chunkifier.h"
#include "kitdsp/lookupTables/sineLut.h"
#include "kitdsp/math/shy_fft.h"

namespace {
float hammingWindow(float t) {
    return 0.54f - (0.46f * kitdsp::cos2pif_lut(t));
}

float hannWindow(float t) {
    return 0.5f * (1.0f - kitdsp::cos2pif_lut(t));
}

float rectangleWindow(float t) {
    return 1;
}
}  // namespace

namespace kitdsp {
template <size_t CHUNK_SIZE>
class STFT {
   public:
    static constexpr size_t GetDesiredBufferSize() { return CHUNK_SIZE * 3; }
    static constexpr size_t GetDesiredOverlap() { return CHUNK_SIZE / 2; }

    explicit STFT(etl::span<float> buffer)
        : mChunkifier({buffer.data(), CHUNK_SIZE * 2}, CHUNK_SIZE, GetDesiredOverlap()),
          mOut(buffer.data() + CHUNK_SIZE * 2, CHUNK_SIZE),
          mFFT() {
        mFFT.Init();
    }

    void Process(float in) {
        mChunkifier.Process(in, [this](etl::span<float> chunk) {
            for (size_t i = 0; i < chunk.size(); ++i) {
                chunk[i] = chunk[i] * hammingWindow(static_cast<float>(i) / static_cast<float>(chunk.size()));
            }
            mFFT.Direct(chunk.data(), mOut.data());
        });
    }

    etl::span<float> mOut;

   private:
    Chunkifier<float> mChunkifier;
    ShyFFT<float, CHUNK_SIZE> mFFT;
};
}  // namespace kitdsp