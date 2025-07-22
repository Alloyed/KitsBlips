#pragma once

#include <cstdint>
#include <cstdio>
#include "kitdsp/math/vector.h"

// convenience wrapper for writing wav files using cstdio (mostly for debugging)

namespace kitdsp {
template <size_t NUM_CHANNELS>
class WavFile {
   public:
    WavFile(float sampleRate, FILE* fp) : mFp(fp), mSampleRate(static_cast<uint32_t>(sampleRate)) {}

    void Start() {
        // implied
        constexpr uint16_t bitsPerByte = 8;
        // https://en.wikipedia.org/wiki/WAV#WAV_file_header

        fputs("RIFF", mFp);
        mTellFileSize = ftell(mFp);
        uint32_t fileSize = 0;  // fill in later
        fwrite(&fileSize, sizeof(fileSize), 1, mFp);
        fputs("WAVE", mFp);

        // format
        fputs("fmt ", mFp);
        uint32_t fmtDataSize = 16;
        fwrite(&fmtDataSize, sizeof(fmtDataSize), 1, mFp);
        uint16_t audioFormat = 3;  // float
        fwrite(&audioFormat, sizeof(audioFormat), 1, mFp);
        uint16_t numChannels = NUM_CHANNELS;
        fwrite(&numChannels, sizeof(numChannels), 1, mFp);
        uint32_t sampleRate = mSampleRate;
        fwrite(&sampleRate, sizeof(sampleRate), 1, mFp);
        uint32_t bytesPerSecond = mBytesPerSample * NUM_CHANNELS * mSampleRate;
        fwrite(&bytesPerSecond, sizeof(bytesPerSecond), 1, mFp);
        uint16_t bytesPerSampleAllChannels = mBytesPerSample * NUM_CHANNELS;
        fwrite(&bytesPerSampleAllChannels, sizeof(bytesPerSampleAllChannels), 1, mFp);
        uint16_t bitsPerSample = mBytesPerSample * bitsPerByte;
        fwrite(&bitsPerSample, sizeof(bitsPerSample), 1, mFp);

        // data
        fputs("data", mFp);
        mTellWavDataSize = ftell(mFp);
        uint32_t wavDataSize = 0;  // fill in later
        fwrite(&wavDataSize, sizeof(wavDataSize), 1, mFp);
    }

    void Add(float in) { mNumSamples += fwrite(&in, mBytesPerSample, 1, mFp); }

    void Add(Vector<float, NUM_CHANNELS> in) {
        for (size_t i = 0; i < NUM_CHANNELS; ++i) {
            Add(in.data[i]);
        }
    }

    void Finish() {
        size_t finalSize = ftell(mFp);

        fseek(mFp, mTellFileSize, SEEK_SET);
        uint32_t fileSize = finalSize - 8;  // remove "riff" and fileSize itself
        fwrite(&fileSize, sizeof(fileSize), 1, mFp);

        fseek(mFp, mTellWavDataSize, SEEK_SET);
        uint32_t wavDataSize = mNumSamples * NUM_CHANNELS * mBytesPerSample;
        fwrite(&wavDataSize, sizeof(wavDataSize), 1, mFp);
    }

    uint32_t GetSampleRate() { return mSampleRate; }

   private:
    // configuration
    uint32_t mSampleRate = 0;
    static constexpr size_t mBytesPerSample = sizeof(float);

    // file state
    FILE* mFp;
    size_t mNumSamples = 0;
    size_t mTellFileSize;
    size_t mTellWavDataSize;
};

}  // namespace kitdsp