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
#include <kitdsp/sampling/samplePlayer.h>
#include <kitdsp/util/spanAllocator.h>
#include <kitdsp/volume/safetyLimiter.h>
#include <optional>

namespace layersynth {

using SamplePlayer = kitdsp::SamplePlayer<float>;
using Lfo = kitdsp::lfo::TriangleOscillator;
using Envelope = kitdsp::ApproachAdsr;

class Voice;
class Layer;
class Global;

class Voice {
   public:
    explicit Voice(clapeze::BaseProcessor& p, size_t idx) { (void)p;(void)idx; }
    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity);
    void ProcessNoteOff() {
        mFilterEnv.TriggerClose();
        mVolumeEnv.TriggerClose();
    }
    void ProcessChoke() {
        mFilterEnv.TriggerChoke();
        mVolumeEnv.TriggerChoke();
    }
    void Reset() {
        mPcmSampler.Reset();
        mFilter.Reset();
        mFilterEnv.Reset();
        mVolumeEnv.Reset();
        mPitchLfo.Reset();
    }
    bool ProcessAudio(clapeze::StereoAudioBuffer& out);

    SamplePlayer mPcmSampler;
    kitdsp::EmileSvf mFilter{};
    Envelope mFilterEnv{};
    Envelope mVolumeEnv{};
    Lfo mPitchLfo{};
    kitdsp::Approach mNote{};

    const Global* mGlobal{};
    const Layer* mLayer{};

    float mVelocity{};
    float mBaseFrequency{};

    kitdsp::SvfFilterMode mFilterMode{};
};

constexpr size_t cMaxVoices = 16;
constexpr float cMaxChorusTimeMs = 20;

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

    void Activate(float sampleRate, size_t maxBlockSize, kitdsp::DynamicSpanAllocator<float>& memory) {
        mVoices.SetNumVoices(cMaxVoices);
        mVoices.SetStrategy(clapeze::VoiceStrategy::Poly);
        mChorus = std::make_optional<kitdsp::Chorus>(
            memory.alloc(narrow_cast<size_t>(cMaxChorusTimeMs * sampleRate / 1000.0f)), sampleRate);
        mScratch.Resize(maxBlockSize);
    }

    clapeze::StereoAudioScratchBuffer mScratch;
    clapeze::VoicePool<clapeze::BaseProcessor, Voice, cMaxVoices> mVoices;
    std::optional<kitdsp::Chorus> mChorus;

    int32_t mWaveIndex{};
    float mNoteOffset{};
    float mTune{};
    float mPitchLfoMult{};

    float mFilterNote{};
    float mFilterRes{};
    float mFilterTrackingMult{};
    float mFilterEnvMult{};

    float mVcaMult{};
    float mVcaVelocityMult{};

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
        mLimit.Reset();
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
        for (auto& layer : mLayers) {
            layer.Activate(sampleRate, maxBlockSize, mMemory);
        }
        mReverb = {mMemory.alloc(kitdsp::PSX::Reverb::GetBufferDesiredSizeFloats(sampleRate)), sampleRate};
    }

    etl::vector<Layer, cNumLayers> mLayers;
    std::optional<kitdsp::PSX::Reverb> mReverb{};
    kitdsp::SafetyLimiter<kitdsp::float_2> mLimit;
    kitdsp::DynamicSpanAllocator<float> mMemory{};
    float mReverbMix{};
    float mSampleRate{};
    float mTune{};
    float mPortamento{};

   private:
};
}  // namespace layersynth