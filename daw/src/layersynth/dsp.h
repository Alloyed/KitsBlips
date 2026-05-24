#pragma once

#include <clapeze/features/params/dynamicParametersFeature.h>
#include <clapeze/processor/voice.h>
#include <etl/array.h>
#include <kitdsp/apps/chorus.h>
#include <kitdsp/apps/equalizer3Band.h>
#include <kitdsp/apps/psxReverb.h>
#include <kitdsp/control/adsr.h>
#include <kitdsp/control/approach.h>
#include <kitdsp/control/gate.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/interpolate.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>
#include <kitdsp/osc/blepOscillator.h>
#include <kitdsp/osc/naiveOscillator.h>
#include <kitdsp/sampler.h>
#include <kitdsp/spanAllocator.h>
#include <optional>


namespace layersynth {

using Sampler = kitdsp::Sampler1D<float>;
using Lfo = kitdsp::lfo::TriangleOscillator;
using Envelope = kitdsp::ApproachAdsr;

class Voice;
class Layer;
class Global;

class Voice {
   public:
    explicit Voice(clapeze::BaseProcessor& p) { (void)p; }
    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
        (void)velocity;
        mNote = note.key;
        mFilterEnv.TriggerOpen();
        mVolumeEnv.TriggerOpen();
        mPcmPhase = 0.0f;
    }
    void ProcessNoteOff() {
        mFilterEnv.TriggerClose();
        mVolumeEnv.TriggerClose();
    }
    void ProcessChoke() {
        mFilterEnv.TriggerChoke();
        mVolumeEnv.TriggerChoke();
    }
    void Reset() {
        mOsc.Reset();
        mFilter.Reset();
        mFilterEnv.Reset();
        mVolumeEnv.Reset();
        mPcmPhase = 0.0f;
    }
    bool ProcessAudio(clapeze::StereoAudioBuffer& out);

    kitdsp::OversampledOscillator<kitdsp::naive::PulseOscillator, 2> mOsc;
    kitdsp::EmileSvf mFilter;
    kitdsp::ApproachAdsr mFilterEnv;
    kitdsp::ApproachAdsr mVolumeEnv;

    const Global* mGlobal{};
    const Layer* mLayer{};
    const Envelope* mPitchEnv{};
    const Lfo* mPitchLfo{};
    const Lfo* mDutyLfo{};
    const Lfo* mFilterLfo{};
    const Lfo* mVcaLfo{};
    const Sampler* mPcmSampler{};

    int32_t mNumber;

    float mNote{};
    float mPcmAdvance{};
    float mPcmPhase{};
};

constexpr size_t cMaxVoices = 16;
class Layer {
   public:
    explicit Layer(clapeze::BaseProcessor& p, Global& sp, clapeze::params::DynamicAudioHandle& params)
        : mVoices(p, params) {
        for (Voice& voice : mVoices.IterAll()) {
            voice.mLayer = this;
            voice.mGlobal = &sp;
        }
    }

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out);

    void Reset() { mVoices.Reset(); }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) { mVoices.ProcessNoteOn(note, velocity); }

    void ProcessNoteOff(const clapeze::NoteTuple& note) { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) { mVoices.ProcessNoteChoke(note); }

    void Activate() {
        mVoices.SetNumVoices(cMaxVoices);
        mVoices.SetStrategy(clapeze::VoiceStrategy::Poly);
    }

    clapeze::VoicePool<clapeze::BaseProcessor, Voice, cMaxVoices> mVoices;
    float mNoteOffset{};
    float mTune{};
    float mPitchEnvMult{};
    float mPitchLfoMult{};
    float mFilterNote{};
    float mFilterTrackingMult{};
    float mFilterEnvMult{};
    float mFilterLfoMult{};
    float mFilterRes{};
    float mVcaMult{};
    float mVcaLfoMult{};

   private:
};

constexpr size_t cNumLayers = 4;
class Global {
   public:
    explicit Global(clapeze::BaseProcessor& p, clapeze::params::DynamicAudioHandle& params) : mLayers() {
        for (size_t i = 0; i < cNumLayers; ++i) {
            mLayers.emplace_back(p, *this, params);
        }
    }
    ~Global() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out);

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
        for (auto& layer : mLayers) {
            layer.ProcessNoteOn(note, velocity);
        }
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) {
        for (auto& layer : mLayers) {
            layer.ProcessNoteOff(note);
        }
    }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) {
        for (auto& layer : mLayers) {
            layer.ProcessNoteChoke(note);
        }
    }

    void ProcessReset() {
        for (auto& layer : mLayers) {
            layer.Reset();
        }
        if (mReverb) {
            mReverb->Reset();
        }
    }

    void Activate(double _sampleRate, size_t minBlockSize, size_t maxBlockSize) {
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRate = narrow_cast<float>(_sampleRate);
        mSampleRate = sampleRate;
        mMemory.reset();
        mReverb = {mMemory.alloc(kitdsp::PSX::Reverb::GetBufferDesiredSizeFloats(sampleRate)), sampleRate};
    }

    etl::vector<Layer, cNumLayers> mLayers;
    std::optional<kitdsp::PSX::Reverb> mReverb{};
    kitdsp::DynamicSpanAllocator<float> mMemory{};
    float mReverbMix{};
    float mSampleRate{};
    float mTune{};

   private:
};
}  // namespace layersynth