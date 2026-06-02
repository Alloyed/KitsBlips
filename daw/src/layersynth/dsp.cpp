#include "layersynth/dsp.h"

namespace layersynth {
bool Voice::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    const auto& layer = *mLayer;
    const auto& global = *mGlobal;

    if(mBaseFrequency == 0.0f) 
    {
        return false;
    }

    for (size_t index = 0; index < out.left.size(); ++index) {
        mVolumeEnv.Process();
        mPitchLfo.Process();
        mNote.Process();

        float pitchNote = mNote.current + layer.mNoteOffset + (mPitchLfo.GetValue() * .0f);
        float filterNote = layer.mFilterNote + (mNote.current * layer.mFilterTrackingMult);
        float res = layer.mFilterRes * 0.89f;  // acts as cap, experimentally determined
        float filterSteepness = 0.5f;          // steeper means "achieves self-oscillation quicker"
        float filterQ = 0.5f * std::exp(filterSteepness * (res / (1 - res)));  // [0, 1] -> [0.5, inf]
        mFilter.SetFrequency(kitdsp::midiToFrequency(filterNote, global.mTune), global.mSampleRate, filterQ);

        float speed = kitdsp::midiToFrequency(pitchNote, global.mTune) / mBaseFrequency;
        mPcmSampler.SetSpeed(speed, global.mSampleRate);
        float waveout = mPcmSampler.Process();
        switch (mFilterMode) {
            case kitdsp::SvfFilterMode::LowPass: {
                waveout = mFilter.Process<kitdsp::SvfFilterMode::LowPass>(waveout);
                break;
            }
            case kitdsp::SvfFilterMode::BandPass: {
                waveout = mFilter.Process<kitdsp::SvfFilterMode::BandPass>(waveout);
                break;
            }
            case kitdsp::SvfFilterMode::HighPass: {
                waveout = mFilter.Process<kitdsp::SvfFilterMode::HighPass>(waveout);
                break;
            }
        }

        float vca =
            kitdsp::lerp(layer.mVcaMult, layer.mVcaMult * mVelocity, layer.mVcaVelocityMult) * mVolumeEnv.GetValue();
        out.left[index] += vca * waveout;
        out.right[index] += vca * waveout;
    }

    if(!mVolumeEnv.IsProcessing()) 
    {
        // is there a less stupid way to do this?
        mPcmSampler.Reset();
        mFilter.Reset();
    }

    return mVolumeEnv.IsProcessing();
}

clapeze::ProcessStatus Layer::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    clapeze::StereoAudioBuffer tmp = mScratch.Alloc(out.left.size());
    auto status = mVoices.ProcessAudio(tmp);
    if(mChorus) {
        for (size_t idx = 0; idx < tmp.left.size(); ++idx) {
            kitdsp::float_2 processed = mChorus->Process(kitdsp::float_2(tmp.left[idx], tmp.right[idx]));
            tmp.left[idx] = processed.left;
            tmp.right[idx] = processed.right;
        }
    }
    out.Add(tmp);
    return status;
}

clapeze::ProcessStatus Global::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    clapeze::ProcessStatus status{};
    out.Fill(0.0f);
    out.isLeftConstant = false;
    out.isRightConstant = false;
    for (auto& layer : mLayers) {
        status = layer.ProcessAudio(out);
    }
    if (mReverb && mReverbMix > 0.0f) {
        for (size_t idx = 0; idx < out.left.size(); ++idx) {
            kitdsp::float_2 outf = {out.left[idx], out.right[idx]};
            kitdsp::float_2 tmp = mReverb->Process(outf);
            tmp = kitdsp::lerp<kitdsp::float_2>(outf, tmp, mReverbMix);
            tmp = mLimit.Process(tmp);
            out.left[idx] = tmp.left;
            out.right[idx] = tmp.right;
        }
    }
    return status;
}

void Voice::ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
    mVelocity = velocity;
    mNote.current = mNote.target;
    mNote.target = note.key;
    mNote.SetSettleTime(mGlobal->mPortamento, mGlobal->mSampleRate, mNote.target - mNote.current);
    mVolumeEnv.TriggerOpen();
    mPcmSampler.Play();
}
}  // namespace layersynth