#pragma once

#include <clapeze/processor/voice.h>
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

class Voice;
class Tone;
class Processor;

/* Unlike the D50, we are combining the synth/PCM partial types into a single signal path. */
class Partial {
   public:
    enum class Wave : uint8_t { Pulse, Saw, Pcm };
    Partial(int32_t number) : mNumber(number) {}
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

    float Process() {
        mFilterEnv.Process();
        mVolumeEnv.Process();

        float pitchNote = mNote + mNoteOffset + (mPitchEnv ? mPitchEnv->GetValue() * mPitchEnvMult : 0.0f) +
                          (mPitchLfo ? mPitchLfo->GetValue() * mPitchLfoMult : 0.0f);
        float filterNote = mFilterNote + (mNote * mFilterTrackingMult) + (mFilterEnv.GetValue() * mFilterEnvMult) +
                           (mFilterLfo ? mFilterLfo->GetValue() * mFilterLfoMult : 0.0f);
        float res = mFilterRes * 0.89f;  // acts as cap, experimentally determined
        float filterSteepness = 0.5f;    // steeper means "achieves self-oscillation quicker"
        float filterQ = 0.5f * std::exp(filterSteepness * (res / (1 - res)));  // [0, 1] -> [0.5, inf]
        mFilter.SetFrequency(kitdsp::midiToFrequency(filterNote, mTune), sr, filterQ);

        float waveout = 0.0f;
        switch (mWave) {
            case Wave::Pulse: {
                mOsc.SetFrequency(kitdsp::midiToFrequency(pitchNote, mTune), sr);
                mOsc.GetOscillator().SetDuty(mDuty + (mDutyLfo ? mDutyLfo->GetValue() * mDutyLfoMult : 0.0f));
                float osc = mOsc.Process();
                waveout = mFilter.Process<kitdsp::SvfFilterMode::LowPass>(osc);
                break;
            }
            case Wave::Saw: {
                mOsc.SetFrequency(kitdsp::midiToFrequency(pitchNote, mTune), sr);
                mOsc.GetOscillator().SetDuty(mDuty + (mDutyLfo ? mDutyLfo->GetValue() * mDutyLfoMult : 0.0f));
                float osc = mOsc.Process();
                waveout = mFilter.Process<kitdsp::SvfFilterMode::LowPass>(osc) *
                          kitdsp::approx::cos2pif_nasty(mOsc.GetPhase());
                break;
            }
            case Wave::Pcm: {
                if (mPcmSampler) {
                    constexpr float baseFreq = 261.626f;  // middle C
                    float mPcmAdvance =
                        (mPcmSampler->GetSampleRate() / sr) * (kitdsp::midiToFrequency(pitchNote, mTune) / baseFreq);
                    mPcmPhase += mPcmAdvance;
                    waveout = mPcmSampler->Read<false, kitdsp::interpolate::InterpolationStrategy::Linear>(mPcmPhase);
                }
                break;
            }
        }

        float vca = mVcaMult * mVolumeEnv.GetValue() + (mVcaLfo ? mVcaLfo->GetValue() * mVcaLfoMult : 0.0f);
        return vca * waveout;
    }
    bool IsProcessing() const { return mVolumeEnv.IsProcessing(); }

    kitdsp::OversampledOscillator<kitdsp::naive::PulseOscillator, 2> mOsc;
    kitdsp::EmileSvf mFilter;
    kitdsp::ApproachAdsr mFilterEnv;
    kitdsp::ApproachAdsr mVolumeEnv;

    const kitdsp::ApproachAdsr* mPitchEnv{};
    const kitdsp::lfo::TriangleOscillator* mPitchLfo{};
    const kitdsp::lfo::TriangleOscillator* mDutyLfo{};
    const kitdsp::lfo::TriangleOscillator* mFilterLfo{};
    const kitdsp::lfo::TriangleOscillator* mVcaLfo{};
    const Sampler* mPcmSampler{};

    int32_t mNumber;
    float sr{};
    float mNote{};
    float mNoteOffset{};
    float mTune{};
    float mPitchEnvMult{};
    float mPitchLfoMult{};
    float mDuty{};
    float mDutyLfoMult{};
    float mFilterNote{};
    float mFilterTrackingMult{};
    float mFilterEnvMult{};
    float mFilterLfoMult{};
    float mFilterRes{};
    float mVcaMult{};
    float mVcaLfoMult{};

    float mPcmAdvance{};
    float mPcmPhase{};

    Wave mWave;

   private:
};

class Tone {
   public:
    Tone() : mPartial1(1), mPartial2(2), mEq() {
        // hardcoded pitch routing
        SetLfoRoute(1, 1, 1);
        mPartial1.mPitchEnv = &mPitchEnv;
        SetLfoRoute(2, 1, 1);
        mPartial2.mPitchEnv = &mPitchEnv;
    }
    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
        (void)velocity;
        mPartial1.ProcessNoteOn(note, velocity);
        mPartial2.ProcessNoteOn(note, velocity);
        mPitchEnv.TriggerOpen();
    }
    void ProcessNoteOff() {
        mPartial1.ProcessNoteOff();
        mPartial2.ProcessNoteOff();
        mPitchEnv.TriggerClose();
    }
    void ProcessChoke() {
        mPartial1.ProcessChoke();
        mPartial2.ProcessChoke();
        mPitchEnv.TriggerChoke();
    }
    void Reset() {
        mPartial1.Reset();
        mPartial2.Reset();
        mLfo1.Reset();
        mLfo2.Reset();
        mLfo3.Reset();
        mPitchEnv.Reset();
        if (mChorus) {
            mChorus->Reset();
        }
    }
    kitdsp::float_2 Process() {
        mPitchEnv.Process();
        mLfo1.Process();
        mLfo2.Process();
        mLfo3.Process();

        float mix = kitdsp::lerp(mPartial1.Process(), mPartial2.Process(), mPartialMix);
        mix = mEq.Process(mix);
        if (mChorus) {
            return mChorus->Process(mix);
        } else {
            return {mix, mix};
        }
    }
    bool IsProcessing() const { return mPartial1.IsProcessing() || mPartial2.IsProcessing(); }

    void SetLfoRoute(int32_t partial, int32_t target, int32_t sourceLfo) {
        Partial* p{};
        if (partial == 1) {
            p = &mPartial1;
        } else {
            p = &mPartial2;
        }

        const kitdsp::lfo::TriangleOscillator** pTarget{};
        if (target == 1) {
            pTarget = &p->mPitchLfo;
        } else if (target == 2) {
            pTarget = &p->mDutyLfo;
        } else if (target == 3) {
            pTarget = &p->mFilterLfo;
        } else {
            pTarget = &p->mVcaLfo;
        }

        if (sourceLfo == 1) {
            *pTarget = &mLfo1;
        } else if (sourceLfo == 2) {
            *pTarget = &mLfo2;
        } else {
            *pTarget = &mLfo3;
        }
    }

    Partial mPartial1;
    Partial mPartial2;
    // tri, saw, square, s&h
    kitdsp::lfo::TriangleOscillator mLfo1;  // pitch, pulse width, cutoff, vca
    kitdsp::lfo::TriangleOscillator mLfo2;  // pulse width, cutoff, vca
    kitdsp::lfo::TriangleOscillator mLfo3;  // pulse width, cutoff, vca
    kitdsp::ApproachAdsr mPitchEnv;
    std::optional<kitdsp::Chorus> mChorus;
    kitdsp::Equalizer3Band mEq;

    float mPartialMix;

   private:
};

class Voice {
   public:
    enum class ToneAlgorithm : uint8_t { OneOnly, TwoOnly, Both };
    explicit Voice(Processor& p) : mTone1(), mTone2() { (void)p; }
    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
        (void)velocity;
        mNote = note.key;
        mTone1.ProcessNoteOn(note, velocity);
        mTone2.ProcessNoteOn(note, velocity);
    }
    void ProcessNoteOff() {
        mTone1.ProcessNoteOff();
        mTone2.ProcessNoteOff();
    }
    void ProcessChoke() {
        mTone1.ProcessChoke();
        mTone2.ProcessChoke();
    }
    void Reset() {
        mTone1.Reset();
        mTone2.Reset();
    }
    bool ProcessAudio(clapeze::StereoAudioBuffer& out) {
        switch (mAlgo) {
            case ToneAlgorithm::OneOnly: {
                for (uint32_t index = 0; index < out.left.size(); index++) {
                    kitdsp::float_2 outf = mTone1.Process();
                    out.left[index] += outf.left;
                    out.right[index] += outf.right;
                }
                break;
            }
            case ToneAlgorithm::TwoOnly: {
                for (uint32_t index = 0; index < out.left.size(); index++) {
                    kitdsp::float_2 outf = mTone2.Process();
                    out.left[index] += outf.left;
                    out.right[index] += outf.right;
                }
                break;
            }
            case ToneAlgorithm::Both: {
                for (uint32_t index = 0; index < out.left.size(); index++) {
                    kitdsp::float_2 outf = mTone1.Process() + mTone2.Process();
                    out.left[index] += outf.left;
                    out.right[index] += outf.right;
                }
                break;
            }
        }

        // if no longer processing, time to sleep
        return mTone1.IsProcessing() || mTone2.IsProcessing();
    }

    Tone mTone1;
    Tone mTone2;
    ToneAlgorithm mAlgo = ToneAlgorithm::OneOnly;

   private:
    float mNote;
};

constexpr size_t cMaxVoices = 1;
template <typename Processor>
class SynthProcessor {
   public:
    explicit SynthProcessor(Processor& p) : mVoices(p) {}
    ~SynthProcessor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) {
        mVoices.SetNumVoices(cMaxVoices);
        mVoices.SetStrategy(clapeze::VoiceStrategy::Poly);

        auto status = mVoices.ProcessAudio(out);
        if (mReverb) {
            for (size_t idx = 0; idx < out.left.size(); ++idx) {
                kitdsp::float_2 tmp = {out.left[idx], out.right[idx]};
                tmp = mReverb->Process(tmp);
                out.left[idx] = tmp.left;
                out.right[idx] = tmp.right;
            }
        }
        return status;
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) { mVoices.ProcessNoteOn(note, velocity); }

    void ProcessNoteOff(const clapeze::NoteTuple& note) { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() {
        mVoices.Reset();
        if (mReverb) {
            mReverb->Reset();
        }
    }

    void Activate(double _sampleRate, size_t minBlockSize, size_t maxBlockSize) {
        (void)minBlockSize;
        (void)maxBlockSize;
        float sampleRate = narrow_cast<float>(_sampleRate);
        mMemory.reset();
        // mReverb = {mMemory.alloc(kitdsp::PSX::Reverb::GetBufferDesiredSizeFloats(sampleRate)), sampleRate};
        mVoices.ForEach([&](Voice& voice) {
            // voice.mTone1.mChorus = {mMemory.alloc(narrow_cast<size_t>(sampleRate) / 4u), sampleRate};
            // voice.mTone2.mChorus = {mMemory.alloc(narrow_cast<size_t>(sampleRate) / 4u), sampleRate};

            voice.mTone1.mPartial1.sr = sampleRate;
            voice.mTone1.mPartial2.sr = sampleRate;
            voice.mTone2.mPartial1.sr = sampleRate;
            voice.mTone2.mPartial2.sr = sampleRate;
        });
    }

    clapeze::VoicePool<Processor, Voice, cMaxVoices> mVoices;
    std::optional<kitdsp::PSX::Reverb> mReverb{};
    kitdsp::DynamicSpanAllocator<float> mMemory{};

   private:
};
}  // namespace layersynth