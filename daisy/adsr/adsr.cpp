#include <daisy.h>
#include <daisy_patch_sm.h>

#include <kitdsp/math/util.h>

using namespace kitdsp;
using namespace daisy;
using namespace patch_sm;


const float cEpsilon = 0.005f;
float lerpPowf(float a, float b, float curve, float t) {
    return lerpf(a, b, clampf(powf(t, curve), 0.0f, 1.0f));
}

/**
 * Implements an ADSR envelope using the "smooth lerp" technique common in
 * gamedev this means that the envelopes are hardcoded to be exponential decay
 * envelopes, which implies logarithmic attacks
 */
class ApproachAdsr {
   public:
    enum class State { Attack, Decay, Sustain, Release, Idle };

    void Init(float sampleRate) { mSampleTimeMs = 1000.0f / sampleRate; }

    void Set(State state, float value) {
        switch (state) {
            case State::Attack: {
                mAttackTime = value;
                mAttackApproach = calculateApproach(mAttackTime, 0.0f, 1.0f);
            } break;
            case State::Decay: {
                mDecayTime = value;
                mDecayApproach =
                    calculateApproach(mDecayTime, 1.0f, mSustainLevel);
            } break;
            case State::Sustain: {
                mSustainLevel = value;
                mDecayApproach =
                    calculateApproach(mDecayTime, 1.0f, mSustainLevel);
                mReleaseApproach =
                    calculateApproach(mReleaseTime, mSustainLevel, 0.0f);
            } break;
            case State::Release: {
                mReleaseTime = value;
                mReleaseApproach =
                    calculateApproach(mReleaseTime, mSustainLevel, 0.0f);
            } break;
            case State::Idle:
                return;
        }
    }

    float Process(bool gate, bool retrigger) {
        // calculate state transistions
        retrigger = gate || retrigger;
        if (gate || retrigger) {
            if (retrigger && !mLastRetrigger) {
                mState = State::Attack;
            }

            if (mState == State::Idle) {
                mState = State::Attack;
            } else if (mState == State::Attack &&
                       mLastLevel >= 1.0f - cEpsilon) {
                mLastLevel = 1.0f;
                mState = State::Decay;
            } else if (mState == State::Decay &&
                       mLastLevel <= mSustainLevel + cEpsilon) {
                mLastLevel = mSustainLevel;
                mState = State::Sustain;
            }
        } else {
            if (mState != State::Idle && mState != State::Release) {
                mState = State::Release;
            } else if (mState == State::Release &&
                       mLastLevel <= 0.0f + cEpsilon) {
                mLastLevel = 0.0f;
                mState = State::Idle;
            }
        }
        mLastRetrigger = retrigger;

        // update & return value based on state
        float targetApproach, targetLevel;
        if (mState == State::Attack) {
            targetApproach = mAttackApproach;
            targetLevel = 1.0f;
        } else if (mState == State::Decay) {
            targetApproach = mDecayApproach;
            targetLevel = mSustainLevel;
        } else if (mState == State::Sustain) {
            // this could be smoothed too to account for real time changes in
            // sustain level, if necessary
            targetApproach = mDecayApproach;
            targetLevel = mSustainLevel;
            // return mLastLevel;
        } else if (mState == State::Release) {
            targetApproach = mReleaseApproach;
            targetLevel = 0.0f;
        } else if (mState == State::Idle) {
            // return mLastLevel;
            return 0.0f;
        }

        // mLastLevel = targetLevel;
        mLastLevel = lerpf(mLastLevel, targetLevel, targetApproach);
        return mLastLevel;
    }

    State GetState() const { return mState; }

   private:
    float calculateApproach(float targetTimeMs,
                            float lastLevel,
                            float nextLevel) {
        // https://www.youtube.com/watch?v=LSNQuFEDOyQ
        // the target precision should be whatever gets us within cEpsilon of
        // the target, in absolute terms
        float precision = cEpsilon * fabsf(nextLevel - lastLevel);
        return 1.0f - blockNanf(powf(precision, mSampleTimeMs / targetTimeMs));
    }

    float mSampleTimeMs;

    State mState{State::Idle};
    float mLastLevel{0.0f};
    bool mLastRetrigger{false};

    float mAttackTime{0.0f};
    float mAttackApproach{1.0f};
    float mDecayTime{0.0f};
    float mDecayApproach{1.0f};
    float mSustainLevel{1.0f};
    float mReleaseApproach{1.0f};
    float mReleaseTime{0.0f};
};

DaisyPatchSM hw;
Switch button, toggle;
ApproachAdsr adsr;

float knobValue(int32_t cvEnum) {
    return clampf(hw.controls[cvEnum].Value(), 0.0f, 1.0f);
}

float jackValue(int32_t cvEnum) {
    return clampf(hw.controls[cvEnum].Value(), -1.0f, 1.0f);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    hw.ProcessAllControls();
    button.Debounce();
    toggle.Debounce();

    bool gate = hw.gate_in_1.State() || button.Pressed();
    bool retrig = hw.gate_in_2.Trig();

    float a = lerpPowf(3.0f, 10000.0f, 2.0f, knobValue(CV_1) + jackValue(CV_5));
    float d = lerpPowf(3.0f, 10000.0f, 2.0f, knobValue(CV_2) + jackValue(CV_6));
    float s = lerpf(0.0f, 1.0f,
                    clampf(knobValue(CV_3) + jackValue(CV_7), 0.0f, 1.0f));
    float r = lerpPowf(3.0f, 10000.0f, 2.0f, knobValue(CV_4) + jackValue(CV_8));

    adsr.Set(ApproachAdsr::State::Attack, a);
    adsr.Set(ApproachAdsr::State::Decay, d);
    adsr.Set(ApproachAdsr::State::Sustain, s);
    adsr.Set(ApproachAdsr::State::Release, r);

    for (size_t i = 0; i < size; i++) {
        float level = adsr.Process(gate, retrig);
        OUT_L[i] = -level;
        OUT_R[i] = level - 1.0f;
    }

    // visualize
    hw.WriteCvOut(CV_OUT_2, lerpf(0.0f, 5.0f, -OUT_L[size - 1]));

    // stage gates!
    // A+D and S+R combined for lack of open jacks
    const auto state = adsr.GetState();
    hw.gate_out_1.Write(state == ApproachAdsr::State::Attack ||
                        state == ApproachAdsr::State::Decay);
    hw.gate_out_2.Write(state == ApproachAdsr::State::Sustain ||
                        state == ApproachAdsr::State::Release);
}

int main(void) {
    hw.Init();
    // low sample rate low block size for cv workload
    hw.SetAudioBlockSize(1);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ);

    button.Init(DaisyPatchSM::B7, hw.AudioCallbackRate());
    toggle.Init(DaisyPatchSM::B8, hw.AudioCallbackRate());
    adsr.Init(hw.AudioSampleRate());
    hw.StartAudio(AudioCallback);

    for (;;) {
    }
}
