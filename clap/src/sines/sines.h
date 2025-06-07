#pragma once

#include "clapApi/common.h"
#include "clapApi/ext/parameters.h"
#include "clapApi/instrumentPlugin.h"
#include "kitdsp/osc/naiveOscillator.h"

enum class SinesParams : clap_id { Volume, Count };
using SinesParamsExt = ParametersExt<SinesParams>;

class Sines : public InstrumentPlugin<SinesParamsExt> {
   public:
    
    static const PluginEntry Entry;
    Sines(PluginHost& host): InstrumentPlugin(host) {}
    ~Sines() = default;
    void Config() override;
    void ProcessAudio(StereoAudioBuffer& out, SinesParamsExt::AudioParameters& params) override;
    void ProcessNoteOn(const NoteTuple& note, float velocity) override;
    void ProcessNoteOff(const NoteTuple& note) override;
    void ProcessNoteChoke(const NoteTuple& note) override;
    void Reset() override;

   private:
    kitdsp::naive::TriangleOscillator mOsc {};
    float mAmplitude {};
    float mTargetAmplitude {};
    NoteTuple mNote {};
};
