#pragma once

#include "clapeze/common.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/instrumentPlugin.h"
#include "kitdsp/osc/naiveOscillator.h"

enum class SinesParams : clap_id { Volume, Count };
using SinesParamsExt = ParametersExt<SinesParams>;


class SinesProcessor : public InstrumentProcessor<SinesParamsExt::ProcessParameters> {
    public:
    SinesProcessor(SinesParamsExt::ProcessParameters& params) : InstrumentProcessor(params) {}
    ~SinesProcessor() = default;

    void ProcessAudio(StereoAudioBuffer& out) override;
    void ProcessNoteOn(const NoteTuple& note, float velocity) override;
    void ProcessNoteOff(const NoteTuple& note) override;
    void ProcessNoteChoke(const NoteTuple& note) override;
    void ProcessReset() override;
    
    private:
    kitdsp::naive::TriangleOscillator mOsc{};
    float mAmplitude{};
    float mTargetAmplitude{};
    NoteTuple mNote{};
};

class Sines : public InstrumentPlugin {
   public:
    static const PluginEntry Entry;
    Sines(PluginHost& host) : InstrumentPlugin(host) {}
    ~Sines() = default;

   protected:
    void Config() override;
};
