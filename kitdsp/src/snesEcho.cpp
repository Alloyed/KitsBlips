#include "kitdsp/snesEcho.h"
#include "kitdsp/math/util.h"

#include <cassert>
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

namespace {
inline int8_t q1_7(uint8_t byte) {
    union U {
        uint8_t byte;
        int8_t q;
    };
    U u;
    u.byte = byte;
    return u.q;
}
}  // namespace
int16_t SNES::Echo::ProcessFIR(int16_t inSample) {
    // update FIR buffer: first is oldest, last is newest
    for (size_t i = 0; i < 7; ++i) {
        mFIRBuffer[i] = mFIRBuffer[i + 1];
    }
    mFIRBuffer[7] = inSample;

    // clang-format off
    // apply first 7 taps
    int16_t S = (q1_7(cfg.filterCoefficients[0]) * mFIRBuffer[0] >> 7) +
                (q1_7(cfg.filterCoefficients[1]) * mFIRBuffer[1] >> 7) +
                (q1_7(cfg.filterCoefficients[2]) * mFIRBuffer[2] >> 7) +
                (q1_7(cfg.filterCoefficients[3]) * mFIRBuffer[3] >> 7) +
                (q1_7(cfg.filterCoefficients[4]) * mFIRBuffer[4] >> 7) +
                (q1_7(cfg.filterCoefficients[5]) * mFIRBuffer[5] >> 7) +
                (q1_7(cfg.filterCoefficients[6]) * mFIRBuffer[6] >> 7);
    // Clip
    S &= 0xFFFF;
    // Apply last tap
    S += (q1_7(cfg.filterCoefficients[7]) * mFIRBuffer[7] >> 7);
    S *= cfg.filterGain;
    // clang-format on

    return S;
}

SNES::Echo::Echo(int16_t* _echoBuffer, size_t _echoBufferCapacity)
    : mEchoBuffer(_echoBuffer), mEchoBufferCapacity(_echoBufferCapacity) {
    ClearBuffer();
}

void SNES::Echo::ClearBuffer() {
    memset(mFIRBuffer, 0, kFIRTaps * sizeof(mFIRBuffer[0]));
    memset(mEchoBuffer, 0, mEchoBufferCapacity * sizeof(mEchoBuffer[0]));
}

void SNES::Echo::ResetHead() {
    mBufferIndex = 0;
    mEchoBufferSize = 0;  // will be updated next process loop
}

void SNES::Echo::Reset() {
    ClearBuffer();
    ResetHead();
    mod = {};
}

float SNES::Echo::Process(float input) {
    // mod
    float targetSize = clamp(cfg.echoBufferSize + mod.echoBufferSize, 0.0f, 1.0f);
    float delayMod = clamp(cfg.echoDelayMod + mod.echoDelayMod, -1.0f, 1.0f);
    float feedback = clamp(cfg.echoFeedback + mod.echoFeedback, -1.0f, 1.0f);
    float filterMix = clamp(cfg.filterMix + mod.filterMix, 0.0f, 1.0f);
    bool freeze = cfg.freezeEcho || mod.freezeEcho;

    // on press events
    if (mod.clearBuffer && !mClearBufferPressed) {
        ClearBuffer();
    }
    mClearBufferPressed = mod.clearBuffer;
    if (mod.resetHead && !mResetHeadPressed) {
        ResetHead();
    }
    mResetHeadPressed = mod.resetHead;

    mBufferIndex = mEchoBufferSize ? (mBufferIndex + 1) % mEchoBufferSize : 0;
    size_t targetSizeSamples = GetDelaySamples(targetSize);
    if (targetSizeSamples != mEchoBufferSize && mBufferIndex == 0) {
        // only resize buffer at the end of of the last delay
        // based on this line in docs
        // > This is because the echo hardware doesn't actually read the
        // > buffer designation values until it reaches the END of the old
        // > buffer!
        mEchoBufferSize = targetSizeSamples;
    }

    int16_t inputNorm = static_cast<int16_t>(input * kHeadRoom * INT16_MAX);

    size_t delayModSamples = GetDelayModSamples(delayMod);

    size_t delayIndex = (mBufferIndex + delayModSamples) % mEchoBufferSize;

    // shift echo buffer from 15-bit -> 16-bit
    int16_t delayedSample = mEchoBuffer[delayIndex] << 1;
    int16_t filteredSample = ProcessFIR(delayedSample);
    int16_t filterMixedSample = lerpf(delayedSample, filteredSample, filterMix);

    // bit-reduced to emulate EFB register
    int8_t feedbackInt = static_cast<int8_t>(feedback * INT8_MAX);
    if (!freeze) {
        // store current state in echo buffer /w feedback
        // echo buffer is 15-bit, so we remove one bit at the end to emulate that
        mEchoBuffer[mBufferIndex] = (inputNorm + (filterMixedSample * feedbackInt / 128)) >> 1;
    }

    float echoFloat = static_cast<float>(filterMixedSample) / INT16_MAX;

    return echoFloat / kHeadRoom;
}

size_t SNES::Echo::GetDelaySamples(float delay) const {
    // TODO: hysteresis
    size_t increment = cfg.echoBufferIncrementSamples;
    size_t minSamples = max(increment, cfg.echoBufferRangeMinSamples);
    size_t maxSamples = min(mEchoBufferCapacity, cfg.echoBufferRangeMaxSamples);
    return clamp<size_t>(roundTo(lerpf(minSamples, maxSamples, delay), increment), minSamples, maxSamples);
}

size_t SNES::Echo::GetDelayModSamples(float delayMod) const {
    return static_cast<size_t>(delayMod * cfg.echoBufferIncrementSamples);
}
}  // namespace kitdsp