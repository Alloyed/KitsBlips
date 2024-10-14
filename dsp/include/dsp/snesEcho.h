#pragma once

#include <cstdint>
#include <cstddef>

namespace SNES
{
  struct Config
  {
    // [0, 1]: rounded up to nearest 16 ms,  down to exactly 16ms and up to kMaxEchoMs. corresponds to the EDL register
    float echoBufferSize;
    // [-1, 1]: feedback. 0 is no feedback, 1 is 100% (watch out!), negative values invert the signal. corresponds to EFB register
    float echoFeedback;
    // [-1, 1]: moves the read head up/down up to 16 ms. can be used for chorus-y effects. (extension, not available on
    // original hardware)
    float echoDelayMod;
    // [0, kNumFilterSettings]: select from a number of FIR filter presets harvested from actual games. corresponds to FIRx register
    uint8_t filterSetting;
    // [0, 1]: FIR mix setting. 0 is no filter, 1 is full filter (as existed on the hardware)
    float filterMix;
    // corresponds to the ECEN register value. when true, writing to the delay buffer is disabled, but reading isn't, freezing its "state"
    bool freezeEcho;
  };

  struct Modulations
  {
    // added to the equivalent config variables
    float echoBufferSize;
    float echoFeedback;
    float echoDelayMod;
    float filterMix;
    bool freezeEcho;
    bool clearBuffer;
    bool resetHead;
  };

  struct Visualizations
  {
    // [0, 1]
    float writeHeadLocation;
    // [0, 1]
    float readHeadLocation;
  };

  // low sample rate for OG crunchiness
  static constexpr int32_t kOriginalSampleRate = 32000;
  // max echo depth. TODO: choose between these at runtime
  static constexpr int32_t kMaxEchoMs = 240;          // the original algorithm length
  static constexpr int32_t kMaxEchoModdedMs = 496;    // the length if you didn't want to store any samples on the chip
  static constexpr int32_t kMaxEchoFantasyMs = 10000; // just a big number
  // snap to 16ms increments, 16 * 32000 / 1000
  static constexpr int32_t kEchoIncrementSamples = 512;
  // hardcoded into the snes. not sure how sample rate affects this
  static constexpr size_t kFIRTaps = 8;
  static constexpr uint8_t kNumFilterSettings = 4;
  // how much should the input be attenuated by to avoid clipping?
  static constexpr float kHeadRoom = 0.1;

  class Model
  {
  public:
    Model(int32_t sampleRate, int16_t *echoBuffer, size_t echoBufferCapacity);
    void Process(float inputLeft,
                 float inputRight,
                 float &outputLeft,
                 float &outputRight);

    static constexpr size_t GetBufferDesiredSizeInt16s(int32_t sampleRate)
    {
      return kMaxEchoMs * (sampleRate / 1000);
    }

    size_t GetDelayLengthSamples(float delay) const;
    size_t GetDelayModLengthSamples(float delayMod) const;

    Config cfg;
    Modulations mod;
    Visualizations viz;

  private:
    int16_t ProcessFIR(uint8_t filterSetting, int16_t in);
    void ClearBuffer();
    void ResetHead();

    int16_t *mEchoBuffer;
    size_t mEchoBufferCapacity;
    size_t mEchoBufferSize;
    size_t mBufferIndex = 0;
    bool mClearBufferPressed = false;
    bool mResetHeadPressed = false;
    int16_t mFIRBuffer[kFIRTaps];
    // any coeffecients are allowed, but historically accurate coeffecients can be found here:
    // https://sneslab.net/wiki/FIR_Filter#Examples
    uint8_t mFIRCoeff[kFIRTaps * kNumFilterSettings] = {
        // LPF 5khz, -25db
        0x0C,
        0x21,
        0x2B,
        0x2B,
        0x13,
        0xFE,
        0xF3,
        0xF9,
        // HPF 3khz, -40db
        0x58,
        0xBF,
        0xDB,
        0xF0,
        0xFE,
        0x07,
        0x0C,
        0x0C,
        // BPF, 1.5khz to 8.5khz
        0x34,
        0x33,
        0x00,
        0xD9,
        0xE5,
        0x01,
        0xFC,
        0xEB,
        // band-stop, 6khz to 12khz
        0x0C,
        0x21,
        0x2B,
        0x2B,
        0xF3,
        0xFE,
        0xF3,
        0xF9};
  };
} // namespace SNES