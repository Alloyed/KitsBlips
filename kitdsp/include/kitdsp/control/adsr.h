#pragma once

#include <cmath>
#include "kitdsp/control/approach.h"
#include "kitdsp/math/util.h"

namespace kitdsp {
/**
 * Implements an ADSR with the "traditional" capacitor discharge curve as used in analog applications. The curve is
 * convex going up, concave going down, and is on a linear scale within a [0, 1] output range.
 */
class ApproachAdsr {
   public:
    enum class State {
        // the envelope is idle when it is at 0, waiting for a new trigger.
        Idle,
        // the envelope can be "choked", this just means to jump to idle in about a ms, independent of release time.
        Choke,
        // the envelope is opening up to 1.
        Attack,
        // the envelope has reached 1, and is decaying to the sustain level
        Decay,
        // the envelope is waiting for the close trigger at the current sustain level
        Sustain,
        // the envelope has been closed, and is decaying to to idle.
        Release
    };
    ApproachAdsr() {}
    ~ApproachAdsr() = default;
    inline void Reset() {
        mState = State::Idle;
        mCurrent = 0.0f;
    }
    inline float Process() { return Update(true); }

    void SetParams(float attackMs, float decayMs, float sustainValue, float releaseMs, float sampleRate) {
        // 0 -> 1
        mAttackH = Approach::CalculateHalfLifeFromSettleTime(attackMs, sampleRate, cSettlePrecision, 1.0f);
        // 1 -> sustain
        mDecayH = Approach::CalculateHalfLifeFromSettleTime(decayMs, sampleRate, cSettlePrecision, 1.0f - sustainValue);
        mSustain = sustainValue;
        // sustain -> 0
        mReleaseH = Approach::CalculateHalfLifeFromSettleTime(releaseMs, sampleRate, cSettlePrecision, sustainValue);
        // any -> 0
        // Chokes just need to end "pretty soon" to sound correct
        mReleaseH = Approach::CalculateHalfLifeFromSettleTime(1.0f, sampleRate, cSettlePrecision, 1.0f);
        // TODO: mState not updated until after Process() called at least once
        Update(false);
    }

    float GetValue() const { return mCurrent; }

    void TriggerOpen() {
        // currently implements "retrigger" logic (unconditionally go to `A` state)
        // alternatives include:
        //  * Legato: if in `ADS` state don't do anything, otherwise go to `A`
        //  * Reset: choke, then go to `A` state afterward
        //  * Oneshot: complete `A` state, then skip `DS` and do `R` immediately after
        mState = State::Attack;
    }

    void TriggerClose() {
        switch (mState) {
            case State::Attack:
            case State::Decay:
            case State::Sustain: {
                mState = State::Release;
                break;
            }
            case State::Release:
            case State::Choke:
            case State::Idle: {
                // do nothing
                break;
            }
        }
    }

    void TriggerChoke() { mState = State::Choke; }

    bool IsProcessing() { return mState != State::Idle; }

   private:
    float Update(bool isProcessing) {
        float target{};
        float targetH{};
        switch (mState) {
            case State::Idle: {
                return 0.0f;
            }
            case State::Sustain: {
                return mSustain;
            }
            case State::Attack: {
                target = 1.0f;
                targetH = mAttackH;
                break;
            }
            case State::Decay: {
                target = mSustain;
                targetH = mDecayH;
                break;
            }
            case State::Release: {
                target = 0.0f;
                targetH = mReleaseH;
                break;
            }
            case State::Choke: {
                target = 0.0f;
                targetH = mChokeH;
                break;
            }
        }

        if (isProcessing) {
            mCurrent = lerpf(mCurrent, target, targetH);
        }

        if (fabsf(target - mCurrent) < cSettlePrecision) {
            mCurrent = target;
            switch (mState) {
                case State::Attack: {
                    mState = State::Decay;
                    break;
                }
                case State::Decay: {
                    mState = State::Sustain;
                    break;
                }
                case State::Choke:
                case State::Release: {
                    mState = State::Idle;
                    break;
                }
                case State::Idle:
                case State::Sustain: {
                    break;
                }
            }
        }

        return mCurrent;
    }
    static constexpr float cSettlePrecision = 0.0001f;
    State mState{};
    float mCurrent = 0.0f;
    float mAttackH = 0.0f;
    float mDecayH = 0.0f;
    float mSustain = 0.0f;
    float mReleaseH = 0.0f;
    float mChokeH = 0.0f;
};
}  // namespace kitdsp