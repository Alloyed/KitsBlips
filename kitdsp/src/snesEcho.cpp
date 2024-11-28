#include "kitdsp/snesEcho.h"
#include "kitdsp/math/util.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace kitdsp {
/*
  This is a rough transcription of the rules that govern the SNES's reverb/echo
pathway, references below:

Thank you especially msx @heckscaper for providing extra info/examples
https://sneslab.net/wiki/FIR_Filter
https://problemkaputt.de/fullsnes.htm#snesaudioprocessingunitapu
https://wiki.superfamicom.org/spc700-reference#dsp-echo-function-1511
https://www.youtube.com/watch?v=JC0PywZvKeg

DSP Mixer/Reverb Block Diagram (c=channel, L/R)
  c0 --->| ADD    |                      |MVOLc| Master Volume   |     |
  c1 --->| Output |--------------------->| MUL |---------------->|     |
  c2 --->| Mixing |                      |_____|                 |     |
  c3 --->|        |                       _____                  | ADD |--> c
  c4 --->|        |                      |EVOLc| Echo Volume     |     |
  c5 --->|        |   Feedback   .------>| MUL |---------------->|     |
  c6 --->|        |   Volume     |       |_____|                 |_____|
  c7 --->|________|    _____     |                      _________________
                      | EFB |    |                     |                 |
     EON  ________    | MUL |<---+---------------------|   Add FIR Sum   |
  c0 -:->|        |   |_____|                          |_________________|
  c1 -:->|        |      |                              _|_|_|_|_|_|_|_|_
  c2 -:->|        |      |                             |   MUL FIR7..0   |
  c3 -:->|        |      |         ESA=Addr, EDL=Len   |_7_6_5_4_3_2_1_0_|
  c4 -:->| ADD    |    __V__  FLG   _______________     _|_|_|_|_|_|_|_|_
  c5 -:->| Echo   |   |     | ECEN | Echo Buffer c |   | FIR Buffer c    |
  c6 -:->| Mixing |-->| ADD |--:-->|   RAM         |-->| (Hardware regs) |
  c7 -:->|________|   |_____|      |_______________|   |_________________|
                                   Newest --> Oldest    Newest --> Oldest
*/

int16_t SNES::Model::ProcessFIR(uint8_t filterSetting, int16_t inSample) {
    // update FIR buffer: first is oldest, last is newest
    for (size_t i = 0; i < 7; ++i) {
        mFIRBuffer[i] = mFIRBuffer[i + 1];
    }
    mFIRBuffer[7] = inSample;

    // update FIR coeffs
    size_t idx = filterSetting * 8;

    // apply first 7 taps
    int16_t S = (static_cast<int8_t>(mFIRCoeff[idx + 0]) * mFIRBuffer[0] >> 6) +
                (static_cast<int8_t>(mFIRCoeff[idx + 1]) * mFIRBuffer[1] >> 6) +
                (static_cast<int8_t>(mFIRCoeff[idx + 2]) * mFIRBuffer[2] >> 6) +
                (static_cast<int8_t>(mFIRCoeff[idx + 3]) * mFIRBuffer[3] >> 6) +
                (static_cast<int8_t>(mFIRCoeff[idx + 4]) * mFIRBuffer[4] >> 6) +
                (static_cast<int8_t>(mFIRCoeff[idx + 5]) * mFIRBuffer[5] >> 6) +
                (static_cast<int8_t>(mFIRCoeff[idx + 6]) * mFIRBuffer[6] >> 6);
    // Clip
    S &= 0xFFFF;
    // Apply last tap
    S += (static_cast<int8_t>(mFIRCoeff[idx + 7]) * mFIRBuffer[7] >> 6);

    return S;
}

SNES::Model::Model(int32_t _sampleRate,
                   int16_t* _echoBuffer,
                   size_t _echoBufferCapacity)
    : mEchoBuffer(_echoBuffer), mEchoBufferCapacity(_echoBufferCapacity) {
    // assert(_sampleRate == kOriginalSampleRate); // TODO: other sample rates
    // assert(_echoBufferCapacity == GetBufferDesiredSizeInt16s(_sampleRate));
    ClearBuffer();
}

void SNES::Model::ClearBuffer() {
    memset(mFIRBuffer, 0, kFIRTaps * sizeof(mFIRBuffer[0]));
    memset(mEchoBuffer, 0, mEchoBufferCapacity * sizeof(mEchoBuffer[0]));
}

void SNES::Model::ResetHead() {
    mBufferIndex = 0;
}

float SNES::Model::Process(float input) {
    float targetSize =
        clamp(cfg.echoBufferSize + mod.echoBufferSize, 0.0f, 1.0f);
    float delayMod = clamp(cfg.echoDelayMod + mod.echoDelayMod, -1.0f, 1.0f);
    float feedback = clamp(cfg.echoFeedback + mod.echoFeedback, -1.0f, 1.0f);
    uint8_t filterSetting = cfg.filterSetting % kNumFilterSettings;
    float filterMix = clamp(cfg.filterMix + mod.filterMix, 0.0f, 1.0f);
    bool freeze = cfg.freezeEcho || mod.freezeEcho;

    // on press
    if (mod.clearBuffer && !mClearBufferPressed) {
        ClearBuffer();
    }
    mClearBufferPressed = mod.clearBuffer;

    // TODO: hysteresis
    size_t targetSizeSamples = clamp<size_t>(
        roundTo(targetSize * mEchoBufferCapacity, kEchoIncrementSamples),
        kEchoIncrementSamples, mEchoBufferCapacity);
    mBufferIndex = mEchoBufferSize ? (mBufferIndex + 1) % mEchoBufferSize : 0;

    // on press
    if (mod.resetHead && !mResetHeadPressed) {
        ResetHead();
    }
    mResetHeadPressed = mod.resetHead;

    if (targetSizeSamples != mEchoBufferSize && mBufferIndex == 0) {
        // only resize buffer at the end of of the last delay
        // based on this line in docs
        // > *** This is because the echo hardware doesn't actually read the
        // buffer designation values until it reaches the END of the old buffer!
        mEchoBufferSize = targetSizeSamples;
    }

    int16_t inputNorm = static_cast<int16_t>(input * kHeadRoom * INT16_MAX);

    size_t delayModSamples = static_cast<size_t>(delayMod * mEchoBufferSize);

    // shift echo buffer from 15-bit -> 16-bit
    size_t delayIndex = (mBufferIndex - delayModSamples) % mEchoBufferSize;
    viz.readHeadLocation = delayIndex / static_cast<float>(mEchoBufferCapacity);
    int16_t delayedSample = mEchoBuffer[delayIndex] << 1;
    int16_t filteredSample = ProcessFIR(filterSetting, delayedSample);
    // lerp
    int8_t filterMixInt = static_cast<int8_t>(filterMix * INT8_MAX);
    int16_t mixedSample = (delayedSample * (128 - filterMixInt) / 128) +
                          (filteredSample * filterMixInt / 128);

    // store current state in echo buffer /w feedback
    int8_t feedbackInt =
        static_cast<int8_t>(feedback * INT8_MAX);  // emulates EFB register
    if (!freeze) {
        // echo buffer is 15-bit, so we remove one bit at the end to emulate
        // that
        mEchoBuffer[mBufferIndex] =
            (inputNorm + (mixedSample * feedbackInt / 128)) >> 1;
        viz.writeHeadLocation =
            mBufferIndex / static_cast<float>(mEchoBufferCapacity);
    }

    float echoFloat = static_cast<float>(mixedSample) / INT16_MAX;

    return echoFloat / kHeadRoom;
}

size_t SNES::Model::GetDelayLengthSamples(float delay) const {
    // TODO: hysteresis
    return clamp<size_t>(
        roundTo(delay * mEchoBufferCapacity, kEchoIncrementSamples),
        kEchoIncrementSamples, mEchoBufferCapacity);
}

size_t SNES::Model::GetDelayModLengthSamples(float delayMod) const {
    // TODO: hysteresis
    return static_cast<size_t>(delayMod * mEchoBufferSize);
}
void SNES::Model::Reset() {
    ClearBuffer();
    ResetHead();
    mod = {};
    mEchoBufferSize = 0;
}
}