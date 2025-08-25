#include "kitdsp/harmonizer.h"

namespace kitdsp {
Harmonizer::Harmonizer(etl::span<float> buffer, float sampleRate) : mDelayLine(buffer), mSampleRate(sampleRate) {}
void Harmonizer::Reset() {}

void Harmonizer::SetParams(float transpose, size_t grainSize) {}

float Harmonizer::Process(float in) {
    float grainSizeMs = 80.f;
    float grainSizeSamples = grainSizeMs * mSampleRate / 1000.0f;

    float shiftRatio = 1.5f;
    float freq = (shiftRatio - 1.0f);  // why -20?
    bool slowing = freq < 0.0f;

    mGrainPhasor.SetPeriod(fabsf(grainSizeMs / freq), mSampleRate);

    mDelayLine.Write(in);
    using namespace kitdsp::interpolate;

    float phase1 = mGrainPhasor.Process();
    float phase2 = lfo::Phasor::WrapPhase(phase1 + 0.5f);

    float grain1Delay = ((slowing ? phase1 : 1.0f - phase1) * grainSizeSamples);
    float grain2Delay = ((slowing ? phase2 : 1.0f - phase2) * grainSizeSamples);
    float grain1 = mDelayLine.Read<InterpolationStrategy::None>(grain1Delay);
    float grain2 = mDelayLine.Read<InterpolationStrategy::None>(grain2Delay);

    // using a triangle wave here to mimic original H910 harmonizer
    float tri = fabsf(phase1 - 0.5f) * 2.0f;
    mFilterOut.SetFrequency(12000.0f, mSampleRate);
    mFilterOut.SetQ(1.0f);
    return clamp<float>(mFilterOut.Process(fade(grain1, grain2, tri)), -1.0f, 1.0f);
}

size_t Harmonizer::GetMaxGrainSize() const {
    return mDelayLine.Size();
}

size_t Harmonizer::GetEffectiveDelay() const {
    return 0;  // idk man
}
}  // namespace kitdsp
