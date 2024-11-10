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
 */

#include "dsp/psx.h"

/** Include standard C headers */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static const uint16_t presets[PSX::kNumPresets][0x20] = {
    {
        /* Name: Room, SPU mem required: 0x26C0 */
        0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80,
        0x5800, 0x5300, 0x04D6, 0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF,
        0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000,
    },
    {
        /* Name: Studio Small, SPU mem required: 0x1F40 */
        0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00,
        0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266,
        0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7,
        0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000,
    },
    {
        /* Name: Studio Medium, SPU mem required: 0x4840 */
        0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0,
        0x5280, 0x4EC0, 0x0904, 0x076B, 0x0824, 0x065F, 0x07A2, 0x0616,
        0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305, 0x0462, 0x02B7,
        0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, 0x8000, 0x8000,
    },
    {
        /* Name: Studio Large, SPU mem required: 0x6FE0*/
        0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680,
        0x5680, 0x52C0, 0x0DFB, 0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973,
        0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0, 0x06EF, 0x03D2,
        0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000,
    },
    {
        /* Name: Hall, SPU mem required: 0xADE0 */
        0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000,
        0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1,
        0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD,
        0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000,
    },
    {
        /* Name: Half Echo, SPU mem required: 0x3C00 */
        0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500,
        0x5F80, 0x54C0, 0x0371, 0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7,
        0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1, 0x011F, 0x0059,
        0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000,
    },
    {
        /* Name: Space Echo, SPU mem required: 0xF6C0 */
        0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000,
        0x6000, 0x5400, 0x1ED6, 0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2,
        0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D, 0x11F6, 0x0C5D,
        0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, 0x8000, 0x8000,
    },
    {
        /* Name: Chaos Echo, SPU mem required: 0x18040 */
        0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100,
        0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
    },
    {
        /* Name: Delay, SPU mem required: 0x18040 */
        0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
    },
    {
        /* Name: Off, SPU mem required: 0x10 */
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000,
    },
};

static float s2f(int16_t v)
{
    return (float)(v) / 32768.0f;
}

/* convert iir filter constant to center frequency */
static float alpha2fc(float alpha, float samplerate)
{
    const double dt     = 1.0 / samplerate;
    const double fc_inv = 2.0 * M_PI * (dt / alpha - dt);
    return (float)(1.0 / fc_inv);
}

/* convert center frequency to iir filter constant */
static float fc2alpha(float fc, float samplerate)
{
    const double dt = 1.0 / samplerate;
    const double rc = 1.0 / (2.0 * M_PI * fc);
    return (float)(dt / (rc + dt));
}

/** Define a macro for converting a gain in dB to a coefficient. */
#define DB_CO(g) ((g) > -90.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)

/* My own stuff. PSX standard presets used in most games can be found here */

typedef struct PsxReverbPreset
{
    uint16_t dAPF1;
    uint16_t dAPF2;
    int16_t  vIIR;
    int16_t  vCOMB1;
    int16_t  vCOMB2;
    int16_t  vCOMB3;
    int16_t  vCOMB4;
    int16_t  vWALL;
    int16_t  vAPF1;
    int16_t  vAPF2;
    uint16_t mLSAME;
    uint16_t mRSAME;
    uint16_t mLCOMB1;
    uint16_t mRCOMB1;
    uint16_t mLCOMB2;
    uint16_t mRCOMB2;
    uint16_t dLSAME;
    uint16_t dRSAME;
    uint16_t mLDIFF;
    uint16_t mRDIFF;
    uint16_t mLCOMB3;
    uint16_t mRCOMB3;
    uint16_t mLCOMB4;
    uint16_t mRCOMB4;
    uint16_t dLDIFF;
    uint16_t dRDIFF;
    uint16_t mLAPF1;
    uint16_t mRAPF1;
    uint16_t mLAPF2;
    uint16_t mRAPF2;
    int16_t  vLIN;
    int16_t  vRIN;
} PsxReverbPreset;

static void preset_load(PsxReverb& psx_rev, int preset_index)
{
    // even if loading preset fails, set the ID regardless so we don't get log spam
    psx_rev.preset = preset_index;

    float stretch_factor = psx_rev.rate / PSX::kOriginalSampleRate;

    PsxReverbPreset* preset = (PsxReverbPreset*)&presets[psx_rev.preset];

    psx_rev.dAPF1 = (uint32_t)((preset->dAPF1 << 2) * stretch_factor);
    psx_rev.dAPF2 = (uint32_t)((preset->dAPF2 << 2) * stretch_factor);
    // correct 22050 Hz IIR alpha to our actual rate
    psx_rev.vIIR = fc2alpha(
        alpha2fc(s2f(preset->vIIR), PSX::kOriginalSampleRate), psx_rev.rate);
    psx_rev.vCOMB1  = s2f(preset->vCOMB1);
    psx_rev.vCOMB2  = s2f(preset->vCOMB2);
    psx_rev.vCOMB3  = s2f(preset->vCOMB3);
    psx_rev.vCOMB4  = s2f(preset->vCOMB4);
    psx_rev.vWALL   = s2f(preset->vWALL);
    psx_rev.vAPF1   = s2f(preset->vAPF1);
    psx_rev.vAPF2   = s2f(preset->vAPF2);
    psx_rev.mLSAME  = (uint32_t)((preset->mLSAME << 2) * stretch_factor);
    psx_rev.mRSAME  = (uint32_t)((preset->mRSAME << 2) * stretch_factor);
    psx_rev.mLCOMB1 = (uint32_t)((preset->mLCOMB1 << 2) * stretch_factor);
    psx_rev.mRCOMB1 = (uint32_t)((preset->mRCOMB1 << 2) * stretch_factor);
    psx_rev.mLCOMB2 = (uint32_t)((preset->mLCOMB2 << 2) * stretch_factor);
    psx_rev.mRCOMB2 = (uint32_t)((preset->mRCOMB2 << 2) * stretch_factor);
    psx_rev.dLSAME  = (uint32_t)((preset->dLSAME << 2) * stretch_factor);
    psx_rev.dRSAME  = (uint32_t)((preset->dRSAME << 2) * stretch_factor);
    psx_rev.mLDIFF  = (uint32_t)((preset->mLDIFF << 2) * stretch_factor);
    psx_rev.mRDIFF  = (uint32_t)((preset->mRDIFF << 2) * stretch_factor);
    psx_rev.mLCOMB3 = (uint32_t)((preset->mLCOMB3 << 2) * stretch_factor);
    psx_rev.mRCOMB3 = (uint32_t)((preset->mRCOMB3 << 2) * stretch_factor);
    psx_rev.mLCOMB4 = (uint32_t)((preset->mLCOMB4 << 2) * stretch_factor);
    psx_rev.mRCOMB4 = (uint32_t)((preset->mRCOMB4 << 2) * stretch_factor);
    psx_rev.dLDIFF  = (uint32_t)((preset->dLDIFF << 2) * stretch_factor);
    psx_rev.dRDIFF  = (uint32_t)((preset->dRDIFF << 2) * stretch_factor);
    psx_rev.mLAPF1  = (uint32_t)((preset->mLAPF1 << 2) * stretch_factor);
    psx_rev.mRAPF1  = (uint32_t)((preset->mRAPF1 << 2) * stretch_factor);
    psx_rev.mLAPF2  = (uint32_t)((preset->mLAPF2 << 2) * stretch_factor);
    psx_rev.mRAPF2  = (uint32_t)((preset->mRAPF2 << 2) * stretch_factor);
    psx_rev.vLIN    = s2f(preset->vLIN);
    psx_rev.vRIN    = s2f(preset->vRIN);

    memset(psx_rev.spu_buffer,
           0,
           psx_rev.spu_buffer_count * sizeof(psx_rev.spu_buffer[0]));
}

PSX::Model::Model(int32_t sampleRate, float* buffer, size_t bufferSize)
{
    assert(bufferSize == GetBufferDesiredSizeFloats(sampleRate));
    rev.rate = (float)sampleRate;

    /* alloc reverb buffer */
    rev.spu_buffer_count = bufferSize;
    rev.spu_buffer_count_mask
        = rev.spu_buffer_count
          - 1; // <-- we can use this for quick circular buffer access
    rev.spu_buffer = buffer;

    preset_load(rev, rev.preset);
    ClearBuffer();
}

void PSX::Model::Process(float  inputLeft,
                         float  inputRight,
                         float& outputLeft,
                         float& outputRight)
{
    if(cfg.preset != rev.preset)
    {
        preset_load(rev, cfg.preset);
    }

#define mem(idx)                                          \
    (rev.spu_buffer[(unsigned)((idx) + rev.BufferAddress) \
                    & rev.spu_buffer_count_mask])
    const float LeftInput  = inputLeft;
    const float RightInput = inputRight;

    const float Lin = rev.vLIN * LeftInput;
    const float Rin = rev.vRIN * RightInput;

    // same side reflection
    mem(rev.mLSAME)
        = (Lin + mem(rev.dLSAME) * rev.vWALL - mem(rev.mLSAME - 1)) * rev.vIIR
          + mem(rev.mLSAME - 1);
    mem(rev.mRSAME)
        = (Rin + mem(rev.dRSAME) * rev.vWALL - mem(rev.mRSAME - 1)) * rev.vIIR
          + mem(rev.mRSAME - 1);

    // different side reflection
    mem(rev.mLDIFF)
        = (Lin + mem(rev.dRDIFF) * rev.vWALL - mem(rev.mLDIFF - 1)) * rev.vIIR
          + mem(rev.mLDIFF - 1);
    mem(rev.mRDIFF)
        = (Rin + mem(rev.dLDIFF) * rev.vWALL - mem(rev.mRDIFF - 1)) * rev.vIIR
          + mem(rev.mRDIFF - 1);

    // early echo
    float Lout = rev.vCOMB1 * mem(rev.mLCOMB1) + rev.vCOMB2 * mem(rev.mLCOMB2)
                 + rev.vCOMB3 * mem(rev.mLCOMB3)
                 + rev.vCOMB4 * mem(rev.mLCOMB4);
    float Rout = rev.vCOMB1 * mem(rev.mRCOMB1) + rev.vCOMB2 * mem(rev.mRCOMB2)
                 + rev.vCOMB3 * mem(rev.mRCOMB3)
                 + rev.vCOMB4 * mem(rev.mRCOMB4);

    // late reverb APF1
    Lout -= rev.vAPF1 * mem(rev.mLAPF1 - rev.dAPF1);
    mem(rev.mLAPF1) = Lout;
    Lout            = Lout * rev.vAPF1 + mem(rev.mLAPF1 - rev.dAPF1);
    Rout -= rev.vAPF1 * mem(rev.mRAPF1 - rev.dAPF1);
    mem(rev.mRAPF1) = Rout;
    Rout            = Rout * rev.vAPF1 + mem(rev.mRAPF1 - rev.dAPF1);

    // late reverb APF2
    Lout -= rev.vAPF2 * mem(rev.mLAPF2 - rev.dAPF2);
    mem(rev.mLAPF2) = Lout;
    Lout            = Lout * rev.vAPF2 + mem(rev.mLAPF2 - rev.dAPF2);
    Rout -= rev.vAPF2 * mem(rev.mRAPF2 - rev.dAPF2);
    mem(rev.mRAPF2) = Rout;
    Rout            = Rout * rev.vAPF2 + mem(rev.mRAPF2 - rev.dAPF2);
#undef mem

    // output to mixer
    outputLeft  = Lout;
    outputRight = Rout;

    rev.BufferAddress = ((rev.BufferAddress + 1) & rev.spu_buffer_count_mask);
}

void PSX::Model::ClearBuffer()
{
    rev.BufferAddress = 0;
    memset(rev.spu_buffer, 0, rev.spu_buffer_count * sizeof(rev.spu_buffer[0]));
}
