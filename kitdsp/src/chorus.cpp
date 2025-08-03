#include "kitdsp/apps/chorus.h"

namespace kitdsp {

Chorus::Chorus(etl::span<float> buffer, float sampleRate) : mDelayLine(buffer), mSampleRate(sampleRate) {}

void Chorus::Reset() {
    cfg = {};
    mDelayLine.Reset();
    mLfo.Reset();
}

float_2 Chorus::Process(float in) {
    mLfo.SetFrequency(cfg.lfoRateHz, mSampleRate);

    float_2 out{};
    // TODO: multiple taps
    using namespace kitdsp::interpolate;
    float delayMs = cfg.delayBaseMs + (mLfo.Process() * cfg.delayModMs);
    float delaySamples = delayMs * mSampleRate / 1000.0f;
    float tap = mDelayLine.Read<InterpolationStrategy::Linear>(delaySamples);

    out.left += tap;
    out.right += tap;

    // send input in
    float outMono = (out.left + out.right) * 0.5f;
    mDelayLine.Write(in + (outMono * cfg.feedback));

    return out;
}

}  // namespace kitdsp