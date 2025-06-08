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

class Snecho : public EffectPlugin<SnechoParamsExt> {
   public:
    static const PluginEntry Entry;
    Snecho(PluginHost& host) : EffectPlugin(host) {}
    ~Snecho() = default;
    void Config() override;
    void ProcessAudio(const StereoAudioBuffer& in,
                      StereoAudioBuffer& out,
                      SnechoParamsExt::AudioParameters& params) override;
    bool Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) override;
    void Reset() override;

   private:
    void OnGui();
    static constexpr size_t snesBufferSize = 7680UL * 10000;
    int16_t snesBuffer1[snesBufferSize];
    size_t mLastFilterPreset;
    kitdsp::SNES::Echo snes1{snesBuffer1, snesBufferSize};
    kitdsp::Resampler<float> snesSampler{kitdsp::SNES::kOriginalSampleRate, 41000};
};
