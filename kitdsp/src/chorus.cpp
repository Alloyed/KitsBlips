#include "kitdsp/apps/chorus.h"
#include "kitdsp/math/interpolate.h"

namespace kitdsp {

Chorus::Chorus(etl::span<float> buffer, float sampleRate) : mDelayLine(buffer), mSampleRate(sampleRate) {}

void Chorus::Reset() {
    cfg = {};
    mDelayLine.Reset();
    mLfo.Reset();
}

float_2 Chorus::Process(float in) {
    return Process({in, in});
}

float_2 Chorus::Process(float_2 in) {
    mLfo.SetFrequency(cfg.lfoRateHz, mSampleRate);

    float_2 out{};

    float lfoMs = (mLfo.Process() * cfg.delayModMs);
    auto toSamples = [&](float offsetMs) {
        return clamp<float>((cfg.delayBaseMs + offsetMs) * mSampleRate / 1000.0f, 0.0f,
                            static_cast<float>(mDelayLine.Size()));
    };

    using namespace kitdsp::interpolate;
    out.left += mDelayLine.Read<InterpolationStrategy::Linear>(toSamples(lfoMs));
    out.right += mDelayLine.Read<InterpolationStrategy::Linear>(toSamples(-lfoMs));

    // send input in
    float inMono = (in.left + in.right) * 0.5f;
    float outMono = (out.left + out.right) * 0.5f;
    mDelayLine.Write(inMono + (outMono * cfg.feedback));

    // lerp
    out = fade(in, out, cfg.mix);

    out.left = clamp<float>(out.left, -1.0f, 1.0f);
    out.right = clamp<float>(out.right, -1.0f, 1.0f);

    return out;
}

float Chorus::GetMaxDelayMs() const {
    return static_cast<float>(mDelayLine.Size()) * 1000.0f / mSampleRate;
}

}  // namespace kitdsp
