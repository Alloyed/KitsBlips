#pragma once

#include <cstdint>
#include <optional>
#include "kitdsp/control/approach.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/util.h"
#include "kitdsp/sampler.h"

namespace kitdsp {
/* Combines a sampler with a state machine for direct playback. supports mid-sample looping/crossfade/etc. */
template <typename SAMPLE>
class SamplePlayer {
   public:
    enum class LoopDirection : uint8_t {
        NoLoop,
        Forward,
        Backward,
        PingPong,
    };
    enum class LoopState : uint8_t {
        Enter,
        Loop,
        Exit,
    };
    explicit SamplePlayer() { Reset(); }

    void SetSampleData(etl::span<SAMPLE> buf, float sampleRate) {
        mRawSamples = buf;
        mBufSampleRate = sampleRate;
        mDirectSampler = Sampler1D<SAMPLE>(mRawSamples, mBufSampleRate);
        mLoopSampler = Sampler1D<SAMPLE>(mRawSamples.subspan(mLoopStart, mLoopEnd - mLoopStart), mBufSampleRate);
    }

    void Reset() {
        mPosition1 = 0.0f;
        mPosition2 = 0.0f;
        mState = LoopState::Exit;
        mPlaying = false;
        mFade.target = 0.0f;
        mFade.current = 0.0f;
    }

    // Play sample from start. if already playing, apply small crossfade to avoid clicks.
    void Play() {
        mPosition2 = mPosition1;
        mPosition1 = 0.0f;
        mState = mDirection == LoopDirection::NoLoop ? LoopState::Exit : LoopState::Enter;
        mFade.current = 1.0f;
        mPlaying = true;
    }
    // Stop sample (with small crossfade to avoid clicks)
    void Choke() {
        mPosition2 = mPosition1;
        mPosition1 = mLoopEnd;
        mState = LoopState::Exit;
        mFade.current = 1.0f;
    }
    // Leave active loop (if there is one). if we haven't hit the loop yet we'll just pretend it isn't there.
    void Exit() { mState = LoopState::Exit; }

    void SetSpeed(float speed, float sampleRate) {
        mFade.SetHalfLife(1.0f, sampleRate);
        mAdvance = speed * mBufSampleRate / sampleRate;
    }
    void SetLoop(LoopDirection direction, size_t loopStart, size_t loopEnd) {
        if (loopEnd < loopStart) {
            std::swap(loopEnd, loopStart);
        }
        mDirection = direction;
        mLoopStart = kitdsp::max<size_t>(0, loopStart);
        mLoopEnd = kitdsp::min<size_t>(loopEnd, mRawSamples.size());
        mLoopSampler = Sampler1D<SAMPLE>(mRawSamples.subspan(mLoopStart, mLoopEnd - mLoopStart), mBufSampleRate);
    }
    bool IsPlaying() const { return mPlaying; }

    SAMPLE Process() {
        if (!mDirectSampler.has_value() || !mPlaying) {
            return {};
        }

        using namespace interpolate;
        switch (mInterpolate) {
            case InterpolationStrategy::None: {
                return ProcessImpl<InterpolationStrategy::None>();
            }
            case InterpolationStrategy::Linear: {
                return ProcessImpl<InterpolationStrategy::Linear>();
            }
            case InterpolationStrategy::Cubic: {
                return ProcessImpl<InterpolationStrategy::Cubic>();
            }
            case InterpolationStrategy::Hermite: {
                return ProcessImpl<InterpolationStrategy::Hermite>();
            }
        }
    }

   private:
    template <interpolate::InterpolationStrategy STRATEGY>
    SAMPLE ProcessImpl() {
        // advance
        mPosition1 += mAdvance * mAdvanceDirection;
        switch (mState) {
            case LoopState::Enter: {
                if (mPosition1 >= mLoopEnd) {
                    switch (mDirection) {
                        case LoopDirection::NoLoop: {
                            mState = LoopState::Exit;
                            break;
                        }
                        case LoopDirection::Forward: {
                            mState = LoopState::Loop;
                            mPosition1 -= (mLoopEnd - mLoopStart);
                            break;
                        }
                        case LoopDirection::Backward:
                        case LoopDirection::PingPong: {
                            mState = LoopState::Loop;
                            mPosition1 -= (mPosition1 - mLoopEnd);
                            mAdvanceDirection = -1.0f;
                            break;
                        }
                    }
                }
                break;
            }
            case LoopState::Loop: {
                switch (mDirection) {
                    case LoopDirection::NoLoop: {
                        mState = LoopState::Exit;
                        break;
                    }
                    case LoopDirection::Forward: {
                        if (mPosition1 >= mLoopEnd) {
                            mPosition1 -= (mLoopEnd - mLoopStart);
                        }
                        break;
                    }
                    case LoopDirection::Backward: {
                        if (mPosition1 <= mLoopStart) {
                            mPosition1 += (mLoopEnd - mLoopStart);
                        }
                        break;
                    }
                    case LoopDirection::PingPong: {
                        if (mPosition1 <= mLoopStart) {
                            mPosition1 += (mLoopStart - mPosition1);
                            mAdvanceDirection = 1;
                        } else if (mPosition1 >= mLoopEnd) {
                            mPosition1 -= (mPosition1 - mLoopEnd);
                            mAdvanceDirection = -1;
                        }
                        break;
                    }
                }
                break;
            }
            case LoopState::Exit: {
                if ((mAdvanceDirection > 0 && mPosition1 >= mRawSamples.size()) ||
                    (mAdvanceDirection < 0 && mPosition1 <= 0)) {
                    mPlaying = false;
                }
                break;
            }
        }

        // read
        float out1{};
        if (mState == LoopState::Loop) {
            if (mAdvanceDirection > 0) {
                out1 = mLoopSampler->template Read<STRATEGY, true, false>(mPosition1);
            } else {
                out1 = mLoopSampler->template Read<STRATEGY, true, true>(mPosition1);
            }
        } else {
            if (mAdvanceDirection > 0) {
                out1 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition1);
            }
        }
        if (mFade.current > 0.0f) {
            mFade.Process();
            mPosition2 += mAdvance * mAdvanceDirection;
            float out2{};
            if (mAdvanceDirection > 0) {
                out2 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition2);
            } else {
                out2 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition2);
            }
            return kitdsp::lerp(out1, out2, mFade.current);
        } else {
            return out1;
        }
    }
    etl::span<SAMPLE> mRawSamples;
    std::optional<Sampler1D<SAMPLE>> mDirectSampler;
    std::optional<Sampler1D<SAMPLE>> mLoopSampler;
    kitdsp::Approach mFade{};
    size_t mLoopStart;
    size_t mLoopEnd;

    float mBufSampleRate;
    float mAdvance{};
    float mAdvanceDirection{};
    float mPosition1{};
    float mPosition2{};

    LoopDirection mDirection{};
    LoopState mState{};
    interpolate::InterpolationStrategy mInterpolate{};
    bool mPlaying{};
};
}  // namespace kitdsp