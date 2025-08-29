#include "kitdsp/harmonizer.h"

namespace kitdsp {
Harmonizer::Harmonizer(etl::span<float> buffer, float sampleRate) : mDelayLine(buffer), mSampleRate(sampleRate) {
    // from the H910 manual, which doesn't expose grain size but mentions that in pitch mode that the delay is variable around 30ms
    SetParams(2.0f, 30.0f, 0.0f, 0.0f);
}
void Harmonizer::Reset() {}

void Harmonizer::SetParams(float pitchRatio, float grainSizeMs, float baseDelayMs, float feedback) {
    mBaseDelaySamples = baseDelayMs * mSampleRate / 1000.0f;
    mGrainSizeSamples = grainSizeMs * mSampleRate / 1000.0f;
    mSlowing = pitchRatio < 1.0f;
    mFeedback = feedback;

    mGrainPhasor.SetPeriod(pitchRatio == 1.0f ? 0.0f : fabsf(grainSizeMs / (pitchRatio - 1.0f)), mSampleRate);
}

float Harmonizer::Process(float in) {
    using namespace kitdsp::interpolate;

    float phase1 = mGrainPhasor.Process();
    float phase2 = lfo::Phasor::WrapPhase(phase1 + 0.5f);

    float grain1Delay = mBaseDelaySamples + ((mSlowing ? phase1 : 1.0f - phase1) * mGrainSizeSamples);
    float grain2Delay = mBaseDelaySamples + ((mSlowing ? phase2 : 1.0f - phase2) * mGrainSizeSamples);
    float grain1 = mDelayLine.Read<InterpolationStrategy::Cubic>(grain1Delay);
    float grain2 = mDelayLine.Read<InterpolationStrategy::Cubic>(grain2Delay);

    // using a triangle wave here to mimic original H910 harmonizer
    float tri = fabsf(phase1 - 0.5f) * 2.0f;
    mFilterOut.SetFrequency(12000.0f, mSampleRate);
    mFilterOut.SetQ(1.0f);

    float out = clamp<float>(mFilterOut.Process(fade(grain1, grain2, tri)), -1.0f, 1.0f);

    mDelayLine.Write(in + (out * mFeedback));

    return out;
}

size_t Harmonizer::GetMaxGrainSize() const {
    return mDelayLine.Size();
}

size_t Harmonizer::GetEffectiveDelay() const {
    return 0;  // idk man
}
}  // namespace kitdsp
