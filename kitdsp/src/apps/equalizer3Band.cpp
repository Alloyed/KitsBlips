#include "kitdsp/apps/equalizer3Band.h"
#include "kitdsp/filters/biquad.h"
#include "kitdsp/math/units.h"

namespace kitdsp {
void Equalizer3Band::Reset() {
    mLowShelf.Reset();
    mHighShelf.Reset();
}
float Equalizer3Band::Process(float in) {
    float allGain = dbToRatio(cfg.midGainDb);
    float lowDb = cfg.lowGainDb - cfg.midGainDb;
    float highDb = cfg.highGainDb - cfg.midGainDb;

    mLowShelf.SetFrequency<rbj::BiquadFilterMode::LowShelf>(cfg.lowFreq, cfg.sampleRate);
    mLowShelf.SetShelf<rbj::BiquadFilterMode::LowShelf>(1.0f, lowDb);

    mHighShelf.SetFrequency<rbj::BiquadFilterMode::HighShelf>(cfg.highFreq, cfg.sampleRate);
    mHighShelf.SetShelf<rbj::BiquadFilterMode::HighShelf>(1.0f, highDb);

    float low = mLowShelf.Process(in);
    float hi = mHighShelf.Process(low);

    return hi * allGain;
}
}  // namespace kitdsp