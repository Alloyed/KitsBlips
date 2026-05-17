#pragma once

#include <kitdsp/apps/chorus.h>
#include <kitdsp/control/adsr.h>
#include <kitdsp/control/approach.h>
#include <kitdsp/control/gate.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/onePole.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/units.h>
#include <kitdsp/math/util.h>
#include <kitdsp/osc/blepOscillator.h>
#include <kitdsp/osc/naiveOscillator.h>
#include <kitdsp/sampler.h>
#include <clapeze/processor/voice.h>

namespace layersynth {

class Voice;
class Tone;
class Processor;

/* Unlike the D50, we are combining the synth/PCM partial types into a single signal path. */
class Partial {
    enum class Wave: uint8_t {
        Pulse,
        Saw,
        Pcm
    };
   public:
    Partial(){}
    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
        (void)velocity;
        // mOsc.SetFrequency();
        mFilterEnv.TriggerOpen();
        mVolumeEnv.TriggerOpen();
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
    }

    float Process() {
        mFilterEnv.Process();
        mVolumeEnv.Process();

        float filterNote = mFilterNote + (mNote * mFilterTrackingMult) + (mFilterEnv.GetValue() * mFilterEnvMult) +
                           (mFilterLfo ? mFilterLfo->GetValue() * mFilterLfoMult : 0.0f);
        mFilter.SetFrequency(kitdsp::midiToFrequency(filterNote), sr, mFilterRes);
        float pitchNote = mNote + mNoteOffset + (mPitchEnv ? mPitchEnv->GetValue() * mPitchEnvMult : 0.0f) +
                          (mPitchLfo ? mPitchLfo->GetValue() * mPitchLfoMult : 0.0f);

        mOsc.SetFrequency(kitdsp::midiToFrequency(pitchNote), sr);
        mOsc.GetOscillator().SetDuty(mDuty + (mDutyLfo ? mDutyLfo->GetValue() * mDutyLfoMult : 0.0f));
        float vca = mVcaMult * mVolumeEnv.GetValue() + (mVcaLfo ? mVcaLfo->GetValue() * mVcaLfoMult : 0.0f);

        return vca * mFilter.Process<kitdsp::SvfFilterMode::LowPass>(mOsc.Process());
    }
    bool IsProcessing() const { return mVolumeEnv.IsProcessing(); }

    kitdsp::OversampledOscillator<kitdsp::naive::PulseOscillator, 2> mOsc;
    // kitdsp::Sampler1D<float, kitdsp::interpolate::InterpolationStrategy::Cubic, true> mSampler;
    kitdsp::EmileSvf mFilter;
    kitdsp::ApproachAdsr mFilterEnv;
    kitdsp::ApproachAdsr mVolumeEnv;

    kitdsp::ApproachAdsr* mPitchEnv{};
    kitdsp::lfo::TriangleOscillator* mPitchLfo{};
    kitdsp::lfo::TriangleOscillator* mDutyLfo{};
    kitdsp::lfo::TriangleOscillator* mFilterLfo{};
    kitdsp::lfo::TriangleOscillator* mVcaLfo{};

    float sr{};
    float mNote{};
    float mNoteOffset{};
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

    Wave mWave;

   private:
};

class Tone {
   public:
    Tone() : mPartial1(), mPartial2() {}
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
        if(mChorus) {
            mChorus->Reset();
        }
    }
    kitdsp::float_2 Process() {
        mPitchEnv.Process();
        mLfo1.Process();
        mLfo2.Process();
        mLfo3.Process();

        float mix = kitdsp::lerp(mPartial1.Process(), mPartial2.Process(), mPartialMix);
        if(mChorus) {
            return mChorus->Process(mix);
        }
        else {
            return {mix, mix};
        }
    }
    bool IsProcessing() const { return mPartial1.IsProcessing() || mPartial2.IsProcessing(); }

    Partial mPartial1;
    Partial mPartial2;
    // tri, saw, square, s&h
    kitdsp::lfo::TriangleOscillator mLfo1; // pitch, pulse width, cutoff, vca
    kitdsp::lfo::TriangleOscillator mLfo2; // pulse width, cutoff, vca
    kitdsp::lfo::TriangleOscillator mLfo3; // pulse width, cutoff, vca
    kitdsp::ApproachAdsr mPitchEnv;
    std::optional<kitdsp::Chorus> mChorus;
    // kitdsp::Equalizer mEq; // ok fine...

    float mPartialMix;

   private:
};

class Voice {
   public:
    explicit Voice(Processor& p) : mProcessor(p), mTone1(), mTone2() {}
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
        //const float sampleRate = static_cast<float>(mProcessor.GetSampleRate());
        for (uint32_t index = 0; index < out.left.size(); index++) {
            kitdsp::float_2 outf = mTone1.Process() + mTone2.Process();

            out.left[index] += outf.left;
            out.right[index] += outf.right;
        }

        // if no longer processing, time to sleep
        return mTone1.IsProcessing() || mTone2.IsProcessing();
    }

    Tone mTone1;
    Tone mTone2;
   private:
    Processor& mProcessor;
    float mNote;
};
}  // namespace layersynth