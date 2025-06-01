#pragma once

#include "clapApi/common.h"
#include "clapApi/ext/parameters.h"
#include "clapApi/instrumentPlugin.h"
#include "kitdsp/osc/naiveOscillator.h"

class Sines : public InstrumentPlugin {
   public:
    static const PluginEntry Entry;
    Sines(PluginHost& host): InstrumentPlugin(host) {}
    ~Sines() = default;
    void Config() override;
    void ProcessAudio(StereoAudioBuffer& out, ParametersExt::AudioParameters& params) override;
    void ProcessNoteOn(const NoteTuple& note, float velocity) override;
    void ProcessNoteOff(const NoteTuple& note) override;
    void ProcessNoteChoke(const NoteTuple& note) override;
    bool Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) override;
    void Reset() override;

   private:
    kitdsp::naive::TriangleOscillator mOsc;
    float mAmplitude;
    NoteTuple mNote;
    float mSampleRate;
};
