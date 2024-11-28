/*
  Copyright 2006-2016 David Robillard <d@drobilla.net>
  Copyright 2006 Steve Harris <steve@plugin.org.uk>
  Copyright 2023 Michael Panzlaff <michael.panlaff@fau.de>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/**
 * This code is primarily copied from:
 * https://github.com/ipatix/lv2-psx-reverb/tree/master
 * changes primarily made to use C++ features and style conformance
 */

#include "kitdsp/psxReverb.h"
#include "kitdsp/psxReverbPresets.h"
#include "kitdsp/util.h"

/** Include standard C headers */
#include <cassert>
#include <cmath>
#include <cstdbool>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
float q16ToFloat(int16_t v) {
    return (float)(v) / 32768.0f;
}

/* convert iir filter constant to center frequency */
float alpha2fc(float alpha, float sampleRate) {
    const double dt = 1.0 / sampleRate;
    const double fc_inv = cTwoPi * (dt / alpha - dt);
    return (float)(1.0 / fc_inv);
}

/* convert center frequency to iir filter constant */
float fc2alpha(float fc, float sampleRate) {
    const double dt = 1.0 / sampleRate;
    const double rc = 1.0 / (cTwoPi * fc);
    return (float)(dt / (rc + dt));
}

uint32_t u16ToDelayBufferIndex(uint32_t raw, float sampleRate) {
    float stretchFactor = sampleRate / kitdsp::PSX::kOriginalSampleRate;
    return static_cast<uint32_t>((raw << 2) * stretchFactor);
}
}  // namespace

namespace kitdsp {

PSX::Reverb::Reverb(int32_t sampleRate, float* buffer, size_t bufferSize) {
    assert(bufferSize == GetBufferDesiredSizeFloats(sampleRate));
    rate = (float)sampleRate;

    /* alloc reverb buffer */
    spu_buffer_count = bufferSize;
    spu_buffer_count_mask =
        spu_buffer_count -
        1;  // <-- we can use this for quick circular buffer access
    spu_buffer = buffer;

    LoadPreset(cPresets[0].data.chunk, rate);
    ClearBuffer();
}

void PSX::Reverb::Process(float inputLeft,
                          float inputRight,
                          float& outputLeft,
                          float& outputRight) {
    if (cfg.preset != preset) {
        preset = cfg.preset;
        LoadPreset(cPresets[preset].data.chunk, rate);
    }

#define mem(idx) \
    (spu_buffer[(unsigned)((idx) + BufferAddress) & spu_buffer_count_mask])
    const float LeftInput = inputLeft;
    const float RightInput = inputRight;

    const float Lin = vLIN * LeftInput;
    const float Rin = vRIN * RightInput;

    // same side reflection
    mem(mLSAME) =
        (Lin + mem(dLSAME) * vWALL - mem(mLSAME - 1)) * vIIR + mem(mLSAME - 1);
    mem(mRSAME) =
        (Rin + mem(dRSAME) * vWALL - mem(mRSAME - 1)) * vIIR + mem(mRSAME - 1);

    // different side reflection
    mem(mLDIFF) =
        (Lin + mem(dRDIFF) * vWALL - mem(mLDIFF - 1)) * vIIR + mem(mLDIFF - 1);
    mem(mRDIFF) =
        (Rin + mem(dLDIFF) * vWALL - mem(mRDIFF - 1)) * vIIR + mem(mRDIFF - 1);

    // early echo
    float Lout = vCOMB1 * mem(mLCOMB1) + vCOMB2 * mem(mLCOMB2) +
                 vCOMB3 * mem(mLCOMB3) + vCOMB4 * mem(mLCOMB4);
    float Rout = vCOMB1 * mem(mRCOMB1) + vCOMB2 * mem(mRCOMB2) +
                 vCOMB3 * mem(mRCOMB3) + vCOMB4 * mem(mRCOMB4);

    // late reverb APF1
    Lout -= vAPF1 * mem(mLAPF1 - dAPF1);
    mem(mLAPF1) = Lout;
    Lout = Lout * vAPF1 + mem(mLAPF1 - dAPF1);
    Rout -= vAPF1 * mem(mRAPF1 - dAPF1);
    mem(mRAPF1) = Rout;
    Rout = Rout * vAPF1 + mem(mRAPF1 - dAPF1);

    // late reverb APF2
    Lout -= vAPF2 * mem(mLAPF2 - dAPF2);
    mem(mLAPF2) = Lout;
    Lout = Lout * vAPF2 + mem(mLAPF2 - dAPF2);
    Rout -= vAPF2 * mem(mRAPF2 - dAPF2);
    mem(mRAPF2) = Rout;
    Rout = Rout * vAPF2 + mem(mRAPF2 - dAPF2);
#undef mem

    // output to mixer
    outputLeft = Lout;
    outputRight = Rout;

    BufferAddress = ((BufferAddress + 1) & spu_buffer_count_mask);
}

void PSX::Reverb::ClearBuffer() {
    BufferAddress = 0;
    memset(spu_buffer, 0, spu_buffer_count * sizeof(spu_buffer[0]));
}

void kitdsp::PSX::Reverb::LoadPreset(const PresetBinaryChunk& presetData,
                                     float sampleRate) {
    const kitdsp::PSX::PresetBinaryChunk* preset = &presetData;

    dAPF1 = u16ToDelayBufferIndex(preset->dAPF1, sampleRate);
    dAPF2 = u16ToDelayBufferIndex(preset->dAPF2, sampleRate);
    // correct 22050 Hz IIR alpha to our actual sampleRate
    vIIR =
        fc2alpha(alpha2fc(q16ToFloat(preset->vIIR), PSX::kOriginalSampleRate),
                 sampleRate);
    vCOMB1 = q16ToFloat(preset->vCOMB1);
    vCOMB2 = q16ToFloat(preset->vCOMB2);
    vCOMB3 = q16ToFloat(preset->vCOMB3);
    vCOMB4 = q16ToFloat(preset->vCOMB4);
    vWALL = q16ToFloat(preset->vWALL);
    vAPF1 = q16ToFloat(preset->vAPF1);
    vAPF2 = q16ToFloat(preset->vAPF2);
    mLSAME = u16ToDelayBufferIndex(preset->mLSAME, sampleRate);
    mRSAME = u16ToDelayBufferIndex(preset->mRSAME, sampleRate);
    mLCOMB1 = u16ToDelayBufferIndex(preset->mLCOMB1, sampleRate);
    mRCOMB1 = u16ToDelayBufferIndex(preset->mRCOMB1, sampleRate);
    mLCOMB2 = u16ToDelayBufferIndex(preset->mLCOMB2, sampleRate);
    mRCOMB2 = u16ToDelayBufferIndex(preset->mRCOMB2, sampleRate);
    dLSAME = u16ToDelayBufferIndex(preset->dLSAME, sampleRate);
    dRSAME = u16ToDelayBufferIndex(preset->dRSAME, sampleRate);
    mLDIFF = u16ToDelayBufferIndex(preset->mLDIFF, sampleRate);
    mRDIFF = u16ToDelayBufferIndex(preset->mRDIFF, sampleRate);
    mLCOMB3 = u16ToDelayBufferIndex(preset->mLCOMB3, sampleRate);
    mRCOMB3 = u16ToDelayBufferIndex(preset->mRCOMB3, sampleRate);
    mLCOMB4 = u16ToDelayBufferIndex(preset->mLCOMB4, sampleRate);
    mRCOMB4 = u16ToDelayBufferIndex(preset->mRCOMB4, sampleRate);
    dLDIFF = u16ToDelayBufferIndex(preset->dLDIFF, sampleRate);
    dRDIFF = u16ToDelayBufferIndex(preset->dRDIFF, sampleRate);
    mLAPF1 = u16ToDelayBufferIndex(preset->mLAPF1, sampleRate);
    mRAPF1 = u16ToDelayBufferIndex(preset->mRAPF1, sampleRate);
    mLAPF2 = u16ToDelayBufferIndex(preset->mLAPF2, sampleRate);
    mRAPF2 = u16ToDelayBufferIndex(preset->mRAPF2, sampleRate);
    vLIN = q16ToFloat(preset->vLIN);
    vRIN = q16ToFloat(preset->vRIN);

    memset(spu_buffer, 0, spu_buffer_count * sizeof(spu_buffer[0]));
}

}  // namespace kitdsp