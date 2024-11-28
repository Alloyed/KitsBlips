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

float_2 PSX::Reverb::Process(float_2 in) {
    if (cfg.preset != preset) {
        preset = cfg.preset;
        LoadPreset(cPresets[preset].data.chunk, rate);
    }

#define mem(idx) \
    (spu_buffer[(unsigned)((idx) + BufferAddress) & spu_buffer_count_mask])
    
    const auto get = [&](size_2 index) {
        return float_2{mem(index.l), mem(index.r)};
    };

    const auto set = [&](size_2 index, float_2 sample) {
        mem(index.l) = sample.l;
        mem(index.r) = sample.r;
    };

    // apply input gain
    in = vIN * in;

    // same side reflection
    set(mSAME,
        (in + get(dSAME) * vWALL - get(mSAME - 1)) * vIIR + get(mSAME - 1));

    // different side reflection
    set(mDIFF,
        (in + get(dDIFF) * vWALL - get(mDIFF - 1)) * vIIR + get(mDIFF - 1));

    // early echo
    float_2 out = get(mCOMB1) * vCOMB1 + get(mCOMB2) * vCOMB2 +
                  get(mCOMB3) * vCOMB3 + get(mCOMB4) * vCOMB4;

    // late reverb APF1
    out = out - get(mAPF1 - dAPF1) * vAPF1;
    set(mAPF1, out);
    out = out * vAPF1 + get(mAPF1 - dAPF1);

    // late reverb APF2
    out = out - get(mAPF2 - dAPF2) * vAPF2;
    set(mAPF2, out);
    out = out * vAPF2 + get(mAPF2 - dAPF2);
#undef mem

    BufferAddress = ((BufferAddress + 1) & spu_buffer_count_mask);
    
    // output to mixer
    return out;
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
    mSAME.l = u16ToDelayBufferIndex(preset->mLSAME, sampleRate);
    mSAME.r = u16ToDelayBufferIndex(preset->mRSAME, sampleRate);
    mCOMB1.l = u16ToDelayBufferIndex(preset->mLCOMB1, sampleRate);
    mCOMB1.r = u16ToDelayBufferIndex(preset->mRCOMB1, sampleRate);
    mCOMB2.l = u16ToDelayBufferIndex(preset->mLCOMB2, sampleRate);
    mCOMB2.r = u16ToDelayBufferIndex(preset->mRCOMB2, sampleRate);
    dSAME.l = u16ToDelayBufferIndex(preset->dLSAME, sampleRate);
    dSAME.r = u16ToDelayBufferIndex(preset->dRSAME, sampleRate);
    mDIFF.l = u16ToDelayBufferIndex(preset->mLDIFF, sampleRate);
    mDIFF.r = u16ToDelayBufferIndex(preset->mRDIFF, sampleRate);
    mCOMB3.l = u16ToDelayBufferIndex(preset->mLCOMB3, sampleRate);
    mCOMB3.r = u16ToDelayBufferIndex(preset->mRCOMB3, sampleRate);
    mCOMB4.l = u16ToDelayBufferIndex(preset->mLCOMB4, sampleRate);
    mCOMB4.r = u16ToDelayBufferIndex(preset->mRCOMB4, sampleRate);
    dDIFF.l = u16ToDelayBufferIndex(preset->dLDIFF, sampleRate);
    dDIFF.r = u16ToDelayBufferIndex(preset->dRDIFF, sampleRate);
    mAPF1.l = u16ToDelayBufferIndex(preset->mLAPF1, sampleRate);
    mAPF1.r = u16ToDelayBufferIndex(preset->mRAPF1, sampleRate);
    mAPF2.l = u16ToDelayBufferIndex(preset->mLAPF2, sampleRate);
    mAPF2.r = u16ToDelayBufferIndex(preset->mRAPF2, sampleRate);
    vIN.l = q16ToFloat(preset->vLIN);
    vIN.r = q16ToFloat(preset->vRIN);

    memset(spu_buffer, 0, spu_buffer_count * sizeof(spu_buffer[0]));
}

}  // namespace kitdsp