#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include "kitdsp/math/vector.h"

namespace kitdsp {
    
static inline uint32_t ceilpower2(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

namespace PSX {
struct PresetBinaryChunk;

struct Config {
    int16_t preset;
};

struct Modulations {};

static constexpr int32_t kOriginalSampleRate = 22050;
static constexpr size_t SPU_REV_PRESET_LONGEST_COUNT = 0x18040 / 2;

class Reverb {
   public:
    Reverb(int32_t sampleRate, float* buffer, size_t bufferSize);
    float_2 Process(float_2 in);
    void ClearBuffer();
    
    void LoadPreset(const PresetBinaryChunk& preset, float sampleRate);

    static size_t GetBufferDesiredSizeFloats(int32_t sampleRate) {
        return ceilpower2((uint32_t)ceil(SPU_REV_PRESET_LONGEST_COUNT *
                                         (sampleRate / kOriginalSampleRate)));
    }

    Config cfg;
    Modulations mod;

   private:
    // Port buffers
    float* spu_buffer;
    size_t spu_buffer_count;
    size_t spu_buffer_count_mask;

    uint32_t BufferAddress;

    /* misc things */
    int16_t preset;
    float rate;

    /* converted reverb parameters */
    uint32_t dAPF1;
    uint32_t dAPF2;
    float vIIR;
    float vCOMB1;
    float vCOMB2;
    float vCOMB3;
    float vCOMB4;
    float vWALL;
    float vAPF1;
    float vAPF2;
    size_2 mSAME;
    size_2 mCOMB1;
    size_2 mCOMB2;
    size_2 dSAME;
    size_2 mDIFF;
    size_2 mCOMB3;
    size_2 mCOMB4;
    size_2 dDIFF;
    size_2 mAPF1;
    size_2 mAPF2;
    float_2 vIN;
};
}  // namespace PSX
}