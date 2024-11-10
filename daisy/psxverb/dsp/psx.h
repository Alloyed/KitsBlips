#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>

static constexpr uint32_t ceilpower2(uint32_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

#define SPU_REV_PRESET_LONGEST_COUNT (0x18040 / 2)

typedef struct
{
    // Port buffers
    float* spu_buffer;
    size_t spu_buffer_count;
    size_t spu_buffer_count_mask;

    uint32_t BufferAddress;

    /* misc things */
    int16_t preset;
    float   rate;

    /* converted reverb parameters */
    uint32_t dAPF1;
    uint32_t dAPF2;
    float    vIIR;
    float    vCOMB1;
    float    vCOMB2;
    float    vCOMB3;
    float    vCOMB4;
    float    vWALL;
    float    vAPF1;
    float    vAPF2;
    uint32_t mLSAME;
    uint32_t mRSAME;
    uint32_t mLCOMB1;
    uint32_t mRCOMB1;
    uint32_t mLCOMB2;
    uint32_t mRCOMB2;
    uint32_t dLSAME;
    uint32_t dRSAME;
    uint32_t mLDIFF;
    uint32_t mRDIFF;
    uint32_t mLCOMB3;
    uint32_t mRCOMB3;
    uint32_t mLCOMB4;
    uint32_t mRCOMB4;
    uint32_t dLDIFF;
    uint32_t dRDIFF;
    uint32_t mLAPF1;
    uint32_t mRAPF1;
    uint32_t mLAPF2;
    uint32_t mRAPF2;
    float    vLIN;
    float    vRIN;
} PsxReverb;

namespace PSX
{
struct Config
{
    int16_t preset;
};

struct Modulations
{
};

static constexpr int32_t kOriginalSampleRate = 22050;
static constexpr int32_t kNumPresets         = 10;

class Model
{
  public:
    Model(int32_t sampleRate, float* buffer, size_t bufferSize);
    void Process(float  inputLeft,
                 float  inputRight,
                 float& outputLeft,
                 float& outputRight);
    void ClearBuffer();

    static constexpr size_t GetBufferDesiredSizeFloats(int32_t sampleRate)
    {
        return ceilpower2((uint32_t)ceil(SPU_REV_PRESET_LONGEST_COUNT
                                         * (sampleRate / kOriginalSampleRate)));
    }

    Config      cfg;
    Modulations mod;

  private:
    PsxReverb rev;
};
} // namespace PSX