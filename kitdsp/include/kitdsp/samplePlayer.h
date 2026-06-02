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
        if (buf.size() == 0 || sampleRate == 0.0f) {
            mRawSamples = {};
            mBufSampleRate = 0.0f;
            mDirectSampler = std::nullopt;
            mLoopStart = 0;
            mLoopEnd = 0;
            mLoopSampler = std::nullopt;
        } else {
            mRawSamples = buf;
            mBufSampleRate = sampleRate;
            mDirectSampler = Sampler1D<SAMPLE>(mRawSamples, mBufSampleRate);
            // update loop if needed
            mLoopStart = kitdsp::clamp<size_t>(mLoopStart, 0, mRawSamples.size());
            mLoopEnd = kitdsp::clamp<size_t>(mLoopEnd, 0, mRawSamples.size());
            mLoopSampler = Sampler1D<SAMPLE>(mRawSamples.subspan(mLoopStart, mLoopEnd - mLoopStart), mBufSampleRate);
        }
    }

    void SetLoop(LoopDirection direction, size_t loopStart, size_t loopEnd) {
        if (loopEnd < loopStart) {
            std::swap(loopEnd, loopStart);
        }
        mDirection = direction;
        mLoopStart = kitdsp::clamp<size_t>(loopStart, 0, mRawSamples.size());
        mLoopEnd = kitdsp::clamp<size_t>(loopEnd, 0, mRawSamples.size());
        if(mLoopSampler) {
            mLoopSampler = Sampler1D<SAMPLE>(mRawSamples.subspan(mLoopStart, mLoopEnd - mLoopStart), mBufSampleRate);
        }
    }

    void SetSpeed(float speed, float sampleRate) {
        mFade.SetHalfLife(1.0f, sampleRate);
        mAdvance = speed * mBufSampleRate / sampleRate;
    }

    void Reset() {
        mPosition1 = 0.0f;
        mPosition2 = 0.0f;
        mAdvanceDirection = 1.0f;
        mState = LoopState::Exit;
        mPlaying = false;
        mFade.target = 0.0f;
        mFade.current = 0.0f;
    }

    // Play sample from start. if already playing, apply small crossfade to avoid clicks.
    void Play() {
        if (mPlaying) {
            mPosition2 = mPosition1;
            mFade.current = 1.0f;
        }
        mPosition1 = 0.0f;
        mAdvanceDirection = 1.0f;
        mState = mDirection == LoopDirection::NoLoop ? LoopState::Exit : LoopState::Enter;
        mPlaying = true;
    }
    // Stop sample (with small crossfade to avoid clicks)
    void Choke() {
        if (mPlaying) {
            mPosition2 = mPosition1;
            mPosition1 = mRawSamples.size();
            mState = LoopState::Exit;
            mFade.current = 1.0f;
        }
    }
    // Leave active loop (if there is one). if we haven't hit the loop yet we'll just pretend it isn't there.
    void Exit() { mState = LoopState::Exit; }
    bool IsPlaying() const { return mPlaying; }

    SAMPLE Process() {
        if (!mDirectSampler.has_value() || !mPlaying) {
            return SAMPLE(0);
        }

        SAMPLE out = SAMPLE(0);
        etl::span<SAMPLE> outbuf{&out, 1};
        size_t index = 0;
        using namespace interpolate;
        switch (mInterpolate) {
            case InterpolationStrategy::None: {
                ProcessImpl1<InterpolationStrategy::None>(outbuf, index);
                break;
            }
            case InterpolationStrategy::Linear: {
                ProcessImpl1<InterpolationStrategy::Linear>(outbuf, index);
                break;
            }
            case InterpolationStrategy::Cubic: {
                ProcessImpl1<InterpolationStrategy::Cubic>(outbuf, index);
                break;
            }
            case InterpolationStrategy::Hermite: {
                ProcessImpl1<InterpolationStrategy::Hermite>(outbuf, index);
                break;
            }
        }
        return out;
    }

    SAMPLE Process(etl::span<SAMPLE>& outbuf) {
        if (!mDirectSampler.has_value() || !mPlaying) {
            std::fill(outbuf.begin(), outbuf.end(), SAMPLE(0));
            return;
        }

        size_t index = 0;
        while (index < outbuf.size()) {
            if (!mPlaying) {
                std::fill(outbuf.begin() + index, outbuf.end(), 0.0f);
                return;
            }
            using namespace interpolate;
            switch (mInterpolate) {
                case InterpolationStrategy::None: {
                    ProcessImpl1<InterpolationStrategy::None>(outbuf, index);
                    break;
                }
                case InterpolationStrategy::Linear: {
                    ProcessImpl1<InterpolationStrategy::Linear>(outbuf, index);
                    break;
                }
                case InterpolationStrategy::Cubic: {
                    ProcessImpl1<InterpolationStrategy::Cubic>(outbuf, index);
                    break;
                }
                case InterpolationStrategy::Hermite: {
                    ProcessImpl1<InterpolationStrategy::Hermite>(outbuf, index);
                    break;
                }
            }
        }
    }

   private:
    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessImpl1(etl::span<SAMPLE>& out, size_t& index) {
        switch (mState) {
            case LoopState::Enter: {
                ProcessImpl2Enter<STRATEGY>(out, index);
                break;
            }
            case LoopState::Loop: {
                ProcessImpl2Loop<STRATEGY>(out, index);
                break;
            }
            case LoopState::Exit: {
                ProcessImpl2Exit<STRATEGY>(out, index);
                break;
            }
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessImpl2Enter(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            mPosition1 += mAdvance * mAdvanceDirection;
            // update state
            if (mPosition1 >= mLoopEnd) {
                switch (mDirection) {
                    case LoopDirection::NoLoop: {
                        mState = LoopState::Exit;
                        return;
                    }
                    case LoopDirection::Forward: {
                        mState = LoopState::Loop;
                        mPosition1 -= (mLoopEnd - mLoopStart);
                        return;
                    }
                    case LoopDirection::Backward:
                    case LoopDirection::PingPong: {
                        mState = LoopState::Loop;
                        mPosition1 -= (mPosition1 - mLoopEnd);
                        mAdvanceDirection = -1.0f;
                        return;
                    }
                }
            }

            if(mPosition1 < 0 || mPosition1 > mRawSamples.size())
            {
                printf("OW BADEmter!\n");
            }

            // output
            float out1{};
            if (mAdvanceDirection > 0) {
                out1 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition1);
            }
            if(std::abs(out1) > 1.0f)
            {
                printf("OW BADLOOP!\n");
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }
    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessImpl2Loop(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            mPosition1 += mAdvance * mAdvanceDirection;
            // update state
            switch (mDirection) {
                case LoopDirection::NoLoop: {
                    mState = LoopState::Exit;
                    return;
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
                        mAdvanceDirection = 1.0f;
                    } else if (mPosition1 >= mLoopEnd) {
                        mPosition1 -= (mPosition1 - mLoopEnd);
                        mAdvanceDirection = -1.0f;
                    }
                    break;
                }
            }

            float relativePosition = mPosition1 - mLoopStart;
            if(relativePosition < 0 || relativePosition > mLoopEnd - mLoopStart)
            {
                printf("OW BADLOOP!\n");
            }

            // output
            float out1{};
            if (mAdvanceDirection > 0.0f) {
                out1 = mLoopSampler->template Read<STRATEGY, true, false>(relativePosition);
            } else {
                out1 = mLoopSampler->template Read<STRATEGY, true, true>(relativePosition);
            }
            if(std::abs(out1) > 1.0f)
            {
                printf("OW BADLOOP!\n");
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessImpl2Exit(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            mPosition1 += mAdvance * mAdvanceDirection;
            // update state
            if ((mAdvanceDirection > 0.0f && mPosition1 >= narrow_cast<float>(mRawSamples.size())) ||
                (mAdvanceDirection < 0.0f && mPosition1 <= 0.0f)) {
                if (!mFade.IsChanging()) {
                    mPlaying = false;
                    return;
                }
            }

            if(mPosition1 < 0 || mPosition1 > mRawSamples.size())
            {
                printf("OW BADExit!\n");
            }

            float out1{};
            if (mAdvanceDirection > 0.0f) {
                out1 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition1);
            }
            if(std::abs(out1) > 1.0f)
            {
                printf("OW BADLOOP!\n");
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    SAMPLE ProcessFade(SAMPLE out1) {
        if (mFade.current > 0.0f) {
            mFade.Process();
            mPosition2 += mAdvance * mAdvanceDirection;

            if(mPosition2 < 0 || mPosition2 > mRawSamples.size())
            {
                printf("OW BADFADE!\n");
            }

            float out2{};
            if (mAdvanceDirection > 0.0f) {
                out2 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition2);
            } else {
                out2 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition2);
            }
            if(std::abs(out2) > 1.0f)
            {
                printf("OW BADLOOP!\n");
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
    size_t mLoopStart = 0;
    size_t mLoopEnd = SIZE_MAX;

    float mBufSampleRate;
    float mAdvance = 1.0f;
    float mAdvanceDirection = 1.0f;
    float mPosition1 = 0.0f;
    float mPosition2 = 0.0f;

    LoopDirection mDirection = LoopDirection::NoLoop;
    LoopState mState = LoopState::Exit;
    interpolate::InterpolationStrategy mInterpolate = interpolate::InterpolationStrategy::Cubic;
    bool mPlaying = false;
};
}  // namespace kitdsp