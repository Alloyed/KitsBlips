#pragma once

#include "clapApi/effectPlugin.h"
#include "clapApi/ext/parameters.h"

#include <kitdsp/dbMeter.h>
#include <kitdsp/samplerate/resampler.h>
#include <kitdsp/snesEcho.h>
#include <kitdsp/snesEchoFilterPresets.h>

// TODO: enum classes
enum SnechoParams : clap_id {
    Params_Size,
    Params_Feedback,
    Params_FilterPreset,
    Params_SizeRange,
    Params_Mix,
    Params_FreezeEcho,
    Params_EchoDelayMod,
    Params_FilterMix,
    Params_ClearBuffer,
    Params_ResetHead,
    Params_Count
};
using SnechoParamsExt = ParametersExt<clap_id>;

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
