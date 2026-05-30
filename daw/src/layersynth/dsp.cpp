#include "layersynth/dsp.h"

namespace layersynth {
bool Voice::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    const auto& layer = *mLayer;
    const auto& global = *mGlobal;

    for (size_t index = 0; index < out.left.size(); ++index) {
        mVolumeEnv.Process();
        mPitchLfo.Process();
        mNote.Process();

        float pitchNote = mNote.current + layer.mNoteOffset + (mPitchLfo.GetValue() * .2f);
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

    return mVolumeEnv.IsProcessing();
}

clapeze::ProcessStatus Layer::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    auto status = mVoices.ProcessAudio(out);
    return status;
}

clapeze::ProcessStatus Global::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    clapeze::ProcessStatus status{};
    std::fill(out.left.begin(), out.left.end(), 0.0f);
    std::fill(out.right.begin(), out.right.end(), 0.0f);
    for (auto& layer : mLayers) {
        status = layer.ProcessAudio(out);
    }
    if (mReverb && mReverbMix > 0.0f) {
        for (size_t idx = 0; idx < out.left.size(); ++idx) {
            kitdsp::float_2 tmp = {out.left[idx], out.right[idx]};
            tmp = mReverb->Process(tmp);
            out.left[idx] = kitdsp::lerp(out.left[idx], tmp.left, mReverbMix);
            out.right[idx] = kitdsp::lerp(out.right[idx], tmp.right, mReverbMix);
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