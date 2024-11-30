#pragma once

#include <cstddef>
#include <cstdint>

namespace kitdsp {
namespace SNES {

// low sample rate for OG crunchiness
static constexpr int32_t kOriginalSampleRate = 32000;
static constexpr size_t MsToSamples(int32_t ms) {
    return ms * kOriginalSampleRate * 0.001f;
}
static constexpr size_t GetBufferDesiredCapacityInt16s(int32_t desiredMaxEchoMs) {
    return MsToSamples(desiredMaxEchoMs);
}
// 240ms, the original algorithm echo length
static constexpr int32_t kOriginalMaxEchoSamples = 240 * kOriginalSampleRate / 1000;
// 496ms, the echo length if you re-used all the available ram _just_ for echo processing.
static constexpr int32_t kExtremeMaxEchoSamples = 496 * kOriginalSampleRate / 1000;
// snap to 16ms increments, 16 * 32000 / 1000
static constexpr int32_t kOriginalEchoIncrementSamples = 16 * kOriginalSampleRate / 1000;
// hardcoded into the snes.
static constexpr size_t kFIRTaps = 8;
// how much should the input be attenuated by to avoid clipping?
static constexpr float kHeadRoom = 1.f;

// settings that were not available on original hardware marked as (extension)
struct Config {
    // [0, 1]: The delay length, snapped to increment values. corresponds to the EDL register
    float echoBufferSize = 1.0f;
    // [0, capacity]: (extension). pick what size == 0 means. values less than the initial increment will be rounded up.
    size_t echoBufferRangeMinSamples = kOriginalEchoIncrementSamples;
    // [0, capacity]: (extension). pick what size == 1 means. values greater than the capacity will be rounded down.
    size_t echoBufferRangeMaxSamples = kOriginalMaxEchoSamples;
    // [0, capacity]: (extension). pick the snapping increment. useful for clock sync
    size_t echoBufferIncrementSamples = kOriginalEchoIncrementSamples;
    // [-1, 1]: feedback. 0 is no feedback, 1 is 100% (watch out!), negative
    // values invert the signal. corresponds to EFB register
    float echoFeedback = 0.0f;
    // [-1, 1]: (Extension) moves the read head up/down up to 16 ms. can be used for
    // chorus-y effects.
    float echoDelayMod = 0.0f;
    // Raw FIR filter coefficients. Historical examples can be found here:
    // https://sneslab.net/wiki/FIR_Filter#Examples
    uint8_t filterCoefficients[kFIRTaps] = {0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    // (Extension) final gain to apply to filter. useful for filters that produce louder signals than the dry
    float filterGain = 1.0f;
    // [0, 1]: (Extension) FIR mix setting. 0 is no filter, 1 is full filter.
    float filterMix = 1.0f;
    // corresponds to the ECEN register value. when true, writing to the delay
    // buffer is disabled, but reading isn't, freezing its "state"
    bool freezeEcho = false;
};

struct Modulations {
    // added to the equivalent config variables
    float echoBufferSize;
    float echoFeedback;
    float echoDelayMod;
    float filterMix;
    bool freezeEcho;
    bool clearBuffer;
    bool resetHead;
};

struct Visualizations {};

class Echo {
   public:
    Echo(int16_t* echoBuffer, size_t echoBufferCapacity);
    float Process(float in);

    size_t GetDelaySamples(float delay) const;
    size_t GetDelayModSamples(float delayMod) const;

    void Reset();

    Config cfg;
    Modulations mod;
    Visualizations viz;

   private:
    int16_t ProcessFIR(int16_t in);
    void ClearBuffer();
    void ResetHead();

    int16_t* mEchoBuffer;
    size_t mEchoBufferCapacity;
    size_t mEchoBufferSize;
    size_t mBufferIndex = 0;
    bool mClearBufferPressed = false;
    bool mResetHeadPressed = false;
    int16_t mFIRBuffer[kFIRTaps];
};
}  // namespace SNES
}  // namespace kitdsp