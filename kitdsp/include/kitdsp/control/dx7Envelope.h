#pragma once

#include <cmath>
#include "kitdsp/control/approach.h"
#include "kitdsp/math/units.h"
#include "kitdsp/math/util.h"

namespace kitdsp {
/**
 * Implements an multi-stage envelope inspired by the 8-knob design used on the DX-7.
 * linear curve, linear scale, within a [0, 1] output range.
 */
class Dx7Envelope {
   public:
    enum class State {
        // the envelope is idle when it is at 0, waiting for a new trigger.
        Idle,
        // the envelope can be "choked", this just means to jump to idle in about a ms, independent of release time.
        Choke,
        // segment 1 (attack-like)
        Segment0,
        // segment 2 (hold-like)
        Segment1,
        // segment 3 (decay/sustain-like)
        Segment2,
        // segment 4 (release-like)
        Segment3,
    };
    Dx7Envelope() {}
    ~Dx7Envelope() = default;
    inline void Reset() {
        mState = State::Idle;
        mCurrent = 0.0f;
    }
    inline float Process() { return Update(true); }

    void SetSegment(size_t idx, float level, float durationMs, float sampleRate) {
        mLevel[idx] = level;
        mDurationMs[idx] = durationMs;
        if (sampleRate != mSampleRate) {
            mSampleRate = sampleRate;
        }
        UpdateAdvances();
        Update(false);
    }

    float GetValue() const { return mCurrent; }

    void TriggerOpen() {
        // currently implements "retrigger" logic (unconditionally go to `A` state)
        // alternatives include:
        //  * Legato: if in `ADS` state don't do anything, otherwise go to `A`
        //  * Reset: choke, then go to `A` state afterward
        //  * Oneshot: complete `A` state, then skip `DS` and do `R` immediately after
        mState = State::Segment0;
    }

    void TriggerClose() {
        switch (mState) {
            case State::Segment0:
            case State::Segment1:
            case State::Segment2: {
                mState = State::Segment3;
                break;
            }
            case State::Segment3:
            case State::Choke:
            case State::Idle: {
                // do nothing
                break;
            }
        }
    }

    void TriggerChoke() {
        mState = State::Choke;
        // 1 ms
        mChokeAdvance = -mCurrent * 1000.0f / mSampleRate;
    }

    bool IsProcessing() const { return mState != State::Idle; }

   private:
    void UpdateAdvances() {
        float lastLevel = 0.0f;
        for (size_t idx = 0; idx < 4; ++idx) {
            float dy = mLevel[idx] - lastLevel;
            float durationSamples = msToSamples(mDurationMs[idx], mSampleRate);
            mAdvance[idx] = durationSamples == 0.0f ? dy : dy / durationSamples;
            lastLevel = mLevel[idx];
        }
    }

    float Update(bool isProcessing) {
        float target{};
        float advance{};
        switch (mState) {
            case State::Idle: {
                return 0.0f;
            }
            case State::Segment0: {
                target = mLevel[0];
                advance = mAdvance[0];
                break;
            }
            case State::Segment1: {
                target = mLevel[1];
                advance = mAdvance[1];
                break;
            }
            case State::Segment2: {
                target = mLevel[2];
                advance = mAdvance[2];
                break;
            }
            case State::Segment3: {
                target = mLevel[3];
                advance = mAdvance[3];
                break;
            }
            case State::Choke: {
                target = 0.0f;
                advance = mChokeAdvance;
                break;
            }
        }
        bool isGoingDown = mCurrent > target;
        if (isProcessing) {
            mCurrent += advance;
        }

        if (isGoingDown ? mCurrent <= target : mCurrent >= target) {
            mCurrent = target;
            switch (mState) {
                case State::Segment0: {
                    mState = State::Segment1;
                    break;
                }
                case State::Segment1: {
                    mState = State::Segment2;
                    break;
                }
                case State::Segment2: {
                    // sustain
                    break;
                }
                case State::Segment3: {
                    TriggerChoke();
                    break;
                }
                case State::Choke: {
                    mState = State::Idle;
                    break;
                }
                case State::Idle: {
                    break;
                }
            }
        }

        return mCurrent;
    }
    State mState{};
    float mSampleRate{};
    float mChokeAdvance = 1.0f;
    float mCurrent = 0.0f;
    float mLevel[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float mDurationMs[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float mAdvance[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};
}  // namespace kitdsp
