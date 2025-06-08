#pragma once

#include "clapeze/effectPlugin.h"
#include "clapeze/ext/parameters.h"

#include <kitdsp/dbMeter.h>
#include <kitdsp/samplerate/resampler.h>
#include <kitdsp/snesEcho.h>
#include <kitdsp/snesEchoFilterPresets.h>

enum class SnechoParams : clap_id {
    Size,
    Feedback,
    FilterPreset,
    SizeRange,
    Mix,
    FreezeEcho,
    EchoDelayMod,
    FilterMix,
    ClearBuffer,
    ResetHead,
    Count
};
using SnechoParamsExt = ParametersExt<SnechoParams>;

class SnechoProcessor : public EffectProcessor<SnechoParamsExt::ProcessParameters> {
   public:
    SnechoProcessor(SnechoParamsExt::ProcessParameters& params) : EffectProcessor(params) {}
    ~SnechoProcessor() = default;

    void ProcessAudio(const StereoAudioBuffer& in,
                      StereoAudioBuffer& out) override;
    void ProcessReset() override;
    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override;

   private:
    static constexpr size_t snesBufferSize = 7680UL * 10000;
    int16_t snesBuffer1[snesBufferSize];
    size_t mLastFilterPreset;
    kitdsp::SNES::Echo snes1{snesBuffer1, snesBufferSize};
    kitdsp::Resampler<float> snesSampler{kitdsp::SNES::kOriginalSampleRate, 41000};
};

class Snecho : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    Snecho(PluginHost& host) : EffectPlugin(host) {}
    ~Snecho() = default;

   protected:
    void Config() override;
};
