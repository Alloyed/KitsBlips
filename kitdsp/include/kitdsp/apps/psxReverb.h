#pragma once

#include <cstddef>
#include <cstdint>
#include "kitdsp/math/util.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {

namespace PSX {
struct PresetBinaryChunk;

struct Config {
    int16_t preset;
};

struct Modulations {};

static constexpr int32_t kOriginalSampleRate = 22050;
static constexpr size_t SPU_REV_PRESET_LONGEST_COUNT = 0x18040 / 2;

/**
 * Implements the reverb algorithm featured on the Playstation's SPU chip, which is documented here:
 *    https://www.problemkaputt.de/psx-spx.htm#spureverbformula
 * This code is primarily adapted from:
 *    https://github.com/ipatix/lv2-psx-reverb/tree/master
 * with several modifications for modern C++ and to allow for tweakability
 */
class Reverb {
   public:
    Reverb(int32_t sampleRate, float* buffer, size_t bufferSize);
    void ClearBuffer();
    void Reset();
    float_2 Process(float_2 in);

    void LoadPreset(const PresetBinaryChunk& preset, float sampleRate);

    static size_t GetBufferDesiredSizeFloats(float sampleRate) {
        return ceilToPowerOf2(SPU_REV_PRESET_LONGEST_COUNT * (sampleRate / kOriginalSampleRate));
    }

    Config cfg;
    Modulations mod;

   private:
    // circle buffer
    // TODO: the original algorithm used q16s instead of floats! so for full accuracy we should switch to that
    float* mBuffer;
    size_t mBufferSize;
    size_t mBufferWrapMask{};
    size_t mBufferHeadIndex{};
    float_2 Get(size_2 index);
    void Set(size_2 index, float_2 sample);

    /* misc things */
    int16_t mPreset{};
    float mSampleRate{};

    /*
     * original PSX register values:
     * http://www.problemkaputt.de/psx-spx.htm#spureverbregisters
     * names preserved, order changed for cache benefits
     */
    // input gain
    float_2 vIN{};

    // reflections
    float vWALL{};
    float vIIR{};
    size_2 dSAME{};
    size_2 mSAME{};
    size_2 dDIFF{};
    size_2 mDIFF{};

    // early echo (implemented as comb filter)
    size_2 mCOMB1{};
    float vCOMB1{};
    size_2 mCOMB2{};
    float vCOMB2{};
    size_2 mCOMB3{};
    float vCOMB3{};
    size_2 mCOMB4{};
    float vCOMB4{};

    // late reverb (all pass filter, 1)
    uint32_t dAPF1{};
    size_2 mAPF1{};
    float vAPF1{};

    // late reverb (all pass filter, 2)
    uint32_t dAPF2{};
    size_2 mAPF2{};
    float vAPF2{};
};
}  // namespace PSX
}  // namespace kitdsp