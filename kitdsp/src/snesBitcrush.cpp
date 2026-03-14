#include "kitdsp/apps/snesBitcrush.h"
#include "kitdsp/apps/snesEcho.h"
#include "kitdsp/macros.h"

namespace kitdsp::SNES {
Bitcrush::Bitcrush(float sampleRate) : mResampler(kOriginalSampleRate, sampleRate) {}
/*
 * The rules for the gaussian filter are documented here!
 * https://problemkaputt.de/fullsnes.htm#snesapudspbrrpitch
 */
float Bitcrush::Process(float input) {
    return mResampler.Process<interpolate::InterpolationStrategy::None>(input, [this](float input, float& outf) {
        // The input of this filter is usually a sample and phase, but this implementation is intended to be used with
        // arbitrary audio streams, so we'll have to fake that part.
        int16_t inputNorm = static_cast<int16_t>(input * kHeadRoom * INT16_MAX);

        // emphasis filter yoinked from
        // https://github.com/tildearrow/furnace/blob/47c02e971f34415010a614429f9aebe60fb622fa/src/engine/brrUtils.c#L200
        if (mUseEmphasis) {
            mEmphasisBuf[0] = mEmphasisBuf[1];
            mEmphasisBuf[1] = mEmphasisBuf[2];
            mEmphasisBuf[2] = inputNorm;

            inputNorm = narrow_cast<int16_t>(
                clamp<int32_t>(((mEmphasisBuf[1] << 11) - mEmphasisBuf[0] * 370 - mEmphasisBuf[2] * 374) / 1305,
                               INT16_MIN, INT16_MAX));
        }

        mGaussBuf[0] = mGaussBuf[1];
        mGaussBuf[1] = mGaussBuf[2];
        mGaussBuf[2] = mGaussBuf[3];
        mGaussBuf[3] = inputNorm;

        //  Interpolation is applied on the 4 most recent 15bit BRR samples (new,old,older,oldest), using bit4-11 of the
        //  pitch counter as interpolation index (i=00h..FFh):
        mPhasor.SetFrequency(1000.0f, 32000.0f);
        uint8_t i = narrow_cast<uint8_t>(mPhasor.Process() * 0xFF);
        int32_t out = ((kGaussCoefs[0x0FF - i] * mGaussBuf[0]) >> 10);  // initial 16bit value
        out = out + ((kGaussCoefs[0x1FF - i] * mGaussBuf[1]) >> 10);    // no 16bit overflow handling
        out = out + ((kGaussCoefs[0x100 + i] * mGaussBuf[2]) >> 10);    // no 16bit overflow handling
        out = out + ((kGaussCoefs[0x000 + i] * mGaussBuf[3]) >> 10);    // with 16bit overflow handling
        out = out >> 1;                                                 // convert 16bit result to 15bit

        // back to float!
        outf = static_cast<float>(out) / INT16_MAX / kHeadRoom;
    });
}

void Bitcrush::Reset() {
    std::fill(mGaussBuf.begin(), mGaussBuf.end(), 0);
    std::fill(mEmphasisBuf.begin(), mEmphasisBuf.end(), 0);
    mPhasor.Reset();
}

}  // namespace kitdsp::SNES