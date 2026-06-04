#pragma once

#include <cstdint>
#include <optional>
#include "kitdsp/control/approach.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/util.h"
#include "kitdsp/sampler.h"

namespace kitdsp {
/**
 * Combines a sampler with a state machine for direct playback.
 * supports playback speed changing (including negative
 * speed), mid-sample looping, and limited crossfade.
 */
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
        if (mLoopSampler) {
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
        mLoopDirection = 1.0f;
        mState = LoopState::Exit;
        mPlaying = false;
        mFade.target = 0.0f;
        mFade.current = 0.0f;
    }

    // Play sample from start. if already playing, apply small crossfade to avoid clicks.
    void Play() {
        if (mAdvance > 0) {
            Play(0.0f);
        } else {
            Play(mRawSamples.size());
        }
    }
    // Play sample from specified position. if already playing, apply small crossfade to avoid clicks.
    void Play(float startPosition) {
        if (mPlaying) {
            mPosition2 = mPosition1;
            mFade.current = 1.0f;
        }
        mPosition1 = startPosition;
        mLoopDirection = 1.0f;
        mState = mDirection == LoopDirection::NoLoop ? LoopState::Exit : LoopState::Enter;
        mPlaying = true;
    }
    // Stop sample (with small crossfade to avoid clicks)
    void Choke() {
        if (mPlaying) {
            mPosition2 = mPosition1;
            // jump to end
            mPosition1 = mAdvance * mLoopDirection > 0 ? mRawSamples.size() : 0;
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

        SAMPLE outRaw = SAMPLE(0);
        etl::span<SAMPLE> out{&outRaw, 1};
        size_t index = 0;
        using namespace interpolate;
        switch (mInterpolate) {
            case InterpolationStrategy::None: {
                ProcessImpl<InterpolationStrategy::None>(out, index);
                break;
            }
            case InterpolationStrategy::Linear: {
                ProcessImpl<InterpolationStrategy::Linear>(out, index);
                break;
            }
            case InterpolationStrategy::Cubic: {
                ProcessImpl<InterpolationStrategy::Cubic>(out, index);
                break;
            }
            case InterpolationStrategy::Hermite: {
                ProcessImpl<InterpolationStrategy::Hermite>(out, index);
                break;
            }
        }
        return outRaw;
    }

    SAMPLE Process(etl::span<SAMPLE>& out) {
        if (!mDirectSampler.has_value() || !mPlaying) {
            std::fill(out.begin(), out.end(), SAMPLE(0));
            return;
        }

        size_t index = 0;
        while (index < out.size()) {
            if (!mPlaying) {
                std::fill(out.begin() + index, out.end(), 0.0f);
                return;
            }
            using namespace interpolate;
            switch (mInterpolate) {
                case InterpolationStrategy::None: {
                    ProcessImpl<InterpolationStrategy::None>(out, index);
                    break;
                }
                case InterpolationStrategy::Linear: {
                    ProcessImpl<InterpolationStrategy::Linear>(out, index);
                    break;
                }
                case InterpolationStrategy::Cubic: {
                    ProcessImpl<InterpolationStrategy::Cubic>(out, index);
                    break;
                }
                case InterpolationStrategy::Hermite: {
                    ProcessImpl<InterpolationStrategy::Hermite>(out, index);
                    break;
                }
            }
        }
    }

   private:
    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessImpl(etl::span<SAMPLE>& out, size_t& index) {
        switch (mState) {
            case LoopState::Enter: {
                ProcessEnter<STRATEGY>(out, index);
                break;
            }
            case LoopState::Loop: {
                ProcessLoop<STRATEGY>(out, index);
                break;
            }
            case LoopState::Exit: {
                ProcessExit<STRATEGY>(out, index);
                break;
            }
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessEnter(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            mPosition1 += mAdvance;
            if (mAdvance > 0 && mPosition1 >= mLoopEnd) {
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
                        mLoopDirection = -1.0f;
                        return;
                    }
                }
            } else if (mAdvance < 0 && mPosition1 <= mLoopStart) {
                switch (mDirection) {
                    case LoopDirection::NoLoop: {
                        mState = LoopState::Exit;
                        return;
                    }
                    case LoopDirection::Forward: {
                        mState = LoopState::Loop;
                        mPosition1 += (mLoopEnd - mLoopStart);
                        return;
                    }
                    case LoopDirection::Backward:
                    case LoopDirection::PingPong: {
                        mState = LoopState::Loop;
                        mPosition1 += (mLoopStart - mPosition1);
                        mLoopDirection = -1.0f;
                        return;
                    }
                }
            }

            // output
            float out1{};
            if (mAdvance > 0) {
                out1 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition1);
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }
    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessLoop(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            float advance = mAdvance * mLoopDirection;
            mPosition1 += advance;
            // update state
            switch (mDirection) {
                case LoopDirection::NoLoop: {
                    mState = LoopState::Exit;
                    return;
                }
                case LoopDirection::Forward:
                case LoopDirection::Backward: {
                    if (mPosition1 <= mLoopStart) {
                        mPosition1 += (mLoopEnd - mLoopStart);
                    } else if (mPosition1 >= mLoopEnd) {
                        mPosition1 -= (mLoopEnd - mLoopStart);
                    }
                    break;
                }
                case LoopDirection::PingPong: {
                    if (mPosition1 <= mLoopStart) {
                        mPosition1 += (mLoopStart - mPosition1);
                        mLoopDirection *= -1.0f;
                    } else if (mPosition1 >= mLoopEnd) {
                        mPosition1 -= (mPosition1 - mLoopEnd);
                        mLoopDirection *= -1.0f;
                    }
                    break;
                }
            }

            float relativePosition = mPosition1 - mLoopStart;

            // output
            float out1{};
            if (advance > 0.0f) {
                out1 = mLoopSampler->template Read<STRATEGY, true, false>(relativePosition);
            } else {
                out1 = mLoopSampler->template Read<STRATEGY, true, true>(relativePosition);
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessExit(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            float advance = mAdvance * mLoopDirection;
            mPosition1 += advance;
            // update state
            if ((advance > 0.0f && mPosition1 >= narrow_cast<float>(mRawSamples.size())) ||
                (advance < 0.0f && mPosition1 <= 0.0f)) {
                if (!mFade.IsChanging()) {
                    mPlaying = false;
                    return;
                }
            }

            float out1{};
            if (advance > 0.0f) {
                out1 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition1);
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    SAMPLE ProcessFade(SAMPLE out1) {
        if (mFade.current > 0.0f) {
            mFade.Process();
            float advance = mAdvance * mLoopDirection;
            mPosition2 += advance;

            float out2{};
            if (advance > 0.0f) {
                out2 = mDirectSampler->template Read<STRATEGY, false, false>(mPosition2);
            } else {
                out2 = mDirectSampler->template Read<STRATEGY, false, true>(mPosition2);
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
    float mLoopDirection = 1.0f;
    float mPosition1 = 0.0f;
    float mPosition2 = 0.0f;

    LoopDirection mDirection = LoopDirection::NoLoop;
    LoopState mState = LoopState::Exit;
    interpolate::InterpolationStrategy mInterpolate = interpolate::InterpolationStrategy::Cubic;
    bool mPlaying = false;
};
}  // namespace kitdsp