#include "layersynth/dsp.h"

namespace layersynth {
bool Voice::ProcessAudio(clapeze::StereoAudioBuffer& out) {
    const auto& layer = *mLayer;
    const auto& global = *mGlobal;

    for (size_t index = 0; index < out.left.size(); ++index) {
        mFilterEnv.Process();
        mVolumeEnv.Process();

        float pitchNote = mNote + layer.mNoteOffset + (mPitchEnv ? mPitchEnv->GetValue() * layer.mPitchEnvMult : 0.0f) +
                          (mPitchLfo ? mPitchLfo->GetValue() * layer.mPitchLfoMult : 0.0f);
        float filterNote = layer.mFilterNote + (mNote * layer.mFilterTrackingMult) +
                           (mFilterEnv.GetValue() * layer.mFilterEnvMult) +
                           (mFilterLfo ? mFilterLfo->GetValue() * layer.mFilterLfoMult : 0.0f);
        float res = layer.mFilterRes * 0.89f;  // acts as cap, experimentally determined
        float filterSteepness = 0.5f;          // steeper means "achieves self-oscillation quicker"
        float filterQ = 0.5f * std::exp(filterSteepness * (res / (1 - res)));  // [0, 1] -> [0.5, inf]
        mFilter.SetFrequency(kitdsp::midiToFrequency(filterNote, global.mTune), global.mSampleRate, filterQ);

        float waveout = 0.0f;
        if (mPcmSampler) {
            constexpr float baseFreq = 261.626f;  // middle C
            float mPcmAdvance = (mPcmSampler->GetSampleRate() / global.mSampleRate) *
                                (kitdsp::midiToFrequency(pitchNote, global.mTune) / baseFreq);
            mPcmPhase += mPcmAdvance;
            waveout = mPcmSampler->Read<false, kitdsp::interpolate::InterpolationStrategy::Linear>(mPcmPhase);
        }
        waveout = mFilter.Process<kitdsp::SvfFilterMode::LowPass>(waveout);

        float vca =
            (layer.mVcaMult + (mVcaLfo ? mVcaLfo->GetValue() * layer.mVcaLfoMult : 0.0f)) * mVolumeEnv.GetValue();
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

}  // namespace layersynth