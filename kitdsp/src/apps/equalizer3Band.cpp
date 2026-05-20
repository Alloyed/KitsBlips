#include "kitdsp/apps/equalizer3Band.h"
#include "kitdsp/filters/biquad.h"
#include "kitdsp/math/units.h"

namespace kitdsp {
Equalizer3Band::Equalizer3Band(float sampleRate) : mSampleRate(sampleRate) {}
void Equalizer3Band::Reset() {
    mLowShelf.Reset();
    mHighShelf.Reset();
}
float Equalizer3Band::Process(float in) {
    float allGain = dbToRatio(cfg.midGainDb);
    float lowDb = cfg.lowGainDb - cfg.midGainDb;
    float highDb = cfg.highGainDb - cfg.midGainDb;

    mLowShelf.SetFrequency<rbj::BiquadFilterMode::LowShelf>(cfg.lowFreq, mSampleRate);
    mLowShelf.SetShelf<rbj::BiquadFilterMode::LowShelf>(1.0f, lowDb);

    mHighShelf.SetFrequency<rbj::BiquadFilterMode::HighShelf>(cfg.highFreq, mSampleRate);
    mHighShelf.SetShelf<rbj::BiquadFilterMode::HighShelf>(1.0f, highDb);

    float low = mLowShelf.Process(in);
    float hi = mHighShelf.Process(low);

    return hi * allGain;
}
}  // namespace kitdsp