#pragma once

#include "clapApi/effectPlugin.h"
#include "clapApi/ext/parameters.h"

#include <kitdsp/dbMeter.h>
#include <kitdsp/samplerate/resampler.h>
#include <kitdsp/snesEcho.h>
#include <kitdsp/snesEchoFilterPresets.h>

class Snecho : public EffectPlugin
{
    public:
    static const PluginEntry Entry;
    Snecho(PluginHost& host): EffectPlugin(host) {}
    void Config() override;
    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out, ParametersExt::AudioParameters& params) override;

    private:
    static constexpr size_t snesBufferSize = 7680UL * 10000;
    int16_t snesBuffer1[snesBufferSize];
    size_t mLastFilterPreset;
    kitdsp::SNES::Echo snes1{snesBuffer1, snesBufferSize};
    kitdsp::Resampler<float> snesSampler{kitdsp::SNES::kOriginalSampleRate, 41000};
};
