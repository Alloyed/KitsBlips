#pragma once

#include <cstdint>
#include <optional>
#include "kitdsp/control/approach.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/util.h"
#include "kitdsp/sampling/sampler.h"

namespace kitdsp {
enum class SampleLoopDirection : uint8_t {
    NoLoop,
    Forward,
    Backward,
    PingPong,
};
/**
 * Combines a sampler with a state machine for direct playback.
 * supports playback speed changing (including negative
 * speed), mid-sample looping, and limited crossfade.
 */
template <typename SAMPLE>
class SamplePlayer {
    enum class LoopState : uint8_t {
        Enter,
        Loop,
        Exit,
    };
    static constexpr bool kLoop = true;
    static constexpr bool kNoLoop = false;
    static constexpr bool kBackwards = true;
    static constexpr bool kForwards = false;

   public:
    explicit SamplePlayer() { Reset(); }

    void Reset() {
        mPosition1 = 0.0f;
        mPosition2 = 0.0f;
        mSampleLoopDirection = 1.0f;
        mState = LoopState::Exit;
        mPlaying = false;
        mFade.target = 0.0f;
        mFade.current = 0.0f;
    }

    /**
     * Set raw sample data. sampleRate should be the rate the sample data is recorded at, not the intended playback
     * rate!
     */
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

    /**
     * Set sample loop point. You should call this after setting sample data, to account for differences in sample
     * length
     */
    void SetLoop(SampleLoopDirection direction, size_t loopStart = 0, size_t loopEnd = SIZE_MAX) {
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

    /**
     * set playback speed and sample rate. you should call this, even when speed=1, to get sample-rate independent
     * cross fading.
     */
    void SetSpeed(float speed, float sampleRate) {
        mFade.SetHalfLife(1.0f, sampleRate);
        mAdvance = speed * mBufSampleRate / sampleRate;
    }

    void SetInterpolate(kitdsp::interpolate::InterpolationStrategy strategy) {
        mInterpolate = strategy;
    }

    /**
     * Play sample from start (or end, if playing backwards). If already playing, apply small crossfade to avoid clicks.
     */
    void Play() {
        if (mAdvance > 0) {
            Play(0.0f);
        } else {
            Play(narrow_cast<float>(mRawSamples.size()));
        }
    }
    /**
     * Play sample from specified position. If already playing, apply small crossfade to avoid clicks.
     */
    void Play(float startPosition) {
        if (mPlaying) {
            mPosition2 = mPosition1;
            mFade.current = 1.0f;
        }
        mPosition1 = startPosition;
        mSampleLoopDirection = 1.0f;
        mState = mDirection == SampleLoopDirection::NoLoop ? LoopState::Exit : LoopState::Enter;
        mPlaying = true;
    }
    /**
     * Stop sample with small fadeout to avoid clicks.
     */
    void Choke() {
        if (mPlaying) {
            mPosition2 = mPosition1;
            // jump to end
            mPosition1 = mAdvance * mSampleLoopDirection > 0 ? mRawSamples.size() : 0;
            mState = LoopState::Exit;
            mFade.current = 1.0f;
        }
    }
    /**
     * Exits the current loop and lets the sample finish playing naturally. If we haven't started playing the loop,
     * we'll let the sample finish playing as if no loop were defined. If there's no loop defined, this is a no-op.
     */
    void Exit() { mState = LoopState::Exit; }

    bool IsPlaying() const { return mPlaying; }
    bool IsLooping() const { return mState == LoopState::Loop; }

    SAMPLE Process() {
        SAMPLE outRaw = SAMPLE(0);
        etl::span<SAMPLE> out{&outRaw, 1};
        Process(out);
        return outRaw;
    }

    void Process(etl::span<SAMPLE>& out) {
        if (!mDirectSampler.has_value() || !mPlaying) {
            std::fill(out.begin(), out.end(), SAMPLE(0));
            return;
        }

        size_t index = 0;
        while (index < out.size()) {
            if (!mPlaying) {
                std::fill(out.begin() + index, out.end(), SAMPLE(0));
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

            // check for start of loop
            // if so, we'll undo our change to mPosition and let the current sample be computed by
            // ProcessLoop()/ProcessExit() as needed
            if (mAdvance > 0 && mPosition1 >= mLoopEnd) {
                switch (mDirection) {
                    case SampleLoopDirection::NoLoop: {
                        mState = LoopState::Exit;
                        mPosition1 -= mAdvance;
                        return;
                    }
                    case SampleLoopDirection::Forward: {
                        mState = LoopState::Loop;
                        mPosition1 -= (mLoopEnd - mLoopStart);
                        return;
                    }
                    case SampleLoopDirection::Backward:
                    case SampleLoopDirection::PingPong: {
                        mState = LoopState::Loop;
                        mPosition1 -= (mPosition1 - mLoopEnd);
                        mSampleLoopDirection = -1.0f;
                        return;
                    }
                }
            } else if (mAdvance < 0 && mPosition1 <= mLoopStart) {
                switch (mDirection) {
                    case SampleLoopDirection::NoLoop: {
                        mState = LoopState::Exit;
                        mPosition1 -= mAdvance;
                        return;
                    }
                    case SampleLoopDirection::Forward: {
                        mState = LoopState::Loop;
                        mPosition1 += (mLoopEnd - mLoopStart);
                        return;
                    }
                    case SampleLoopDirection::Backward:
                    case SampleLoopDirection::PingPong: {
                        mState = LoopState::Loop;
                        mPosition1 += (mLoopStart - mPosition1);
                        mSampleLoopDirection = -1.0f;
                        return;
                    }
                }
            }

            // output
            SAMPLE out1{};
            if (mAdvance > 0) {
                out1 = mDirectSampler->template Read<STRATEGY, kNoLoop, kForwards>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, kNoLoop, kBackwards>(mPosition1);
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }
    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessLoop(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            float advance = mAdvance * mSampleLoopDirection;
            mPosition1 += advance;
            // check for state change/loop
            switch (mDirection) {
                case SampleLoopDirection::NoLoop: {
                    mState = LoopState::Exit;
                    return;
                }
                case SampleLoopDirection::Forward:
                case SampleLoopDirection::Backward: {
                    if (mPosition1 <= mLoopStart) {
                        mPosition1 += (mLoopEnd - mLoopStart);
                    } else if (mPosition1 >= mLoopEnd) {
                        mPosition1 -= (mLoopEnd - mLoopStart);
                    }
                    break;
                }
                case SampleLoopDirection::PingPong: {
                    if (mPosition1 <= mLoopStart) {
                        mPosition1 += (mLoopStart - mPosition1);
                        mSampleLoopDirection *= -1.0f;
                    } else if (mPosition1 >= mLoopEnd) {
                        mPosition1 -= (mPosition1 - mLoopEnd);
                        mSampleLoopDirection *= -1.0f;
                    }
                    break;
                }
            }

            // output
            SAMPLE out1{};
            float relativePosition = mPosition1 - mLoopStart;
            if (advance > 0.0f) {
                out1 = mLoopSampler->template Read<STRATEGY, kLoop, kForwards>(relativePosition);
            } else {
                out1 = mLoopSampler->template Read<STRATEGY, kLoop, kBackwards>(relativePosition);
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    void ProcessExit(etl::span<SAMPLE>& out, size_t& index) {
        for (; index < out.size(); ++index) {
            float advance = mAdvance * mSampleLoopDirection;
            mPosition1 += advance;
            // update state
            if ((advance > 0.0f && mPosition1 >= narrow_cast<float>(mRawSamples.size())) ||
                (advance < 0.0f && mPosition1 <= 0.0f)) {
                if (!mFade.IsChanging()) {
                    mPlaying = false;
                    return;
                }
            }

            SAMPLE out1{};
            if (advance > 0.0f) {
                out1 = mDirectSampler->template Read<STRATEGY, kNoLoop, kForwards>(mPosition1);
            } else {
                out1 = mDirectSampler->template Read<STRATEGY, kNoLoop, kBackwards>(mPosition1);
            }
            out[index] = ProcessFade<STRATEGY>(out1);
        }
    }

    template <interpolate::InterpolationStrategy STRATEGY>
    SAMPLE ProcessFade(SAMPLE out1) {
        if (mFade.current > 0.0f) {
            mFade.Process();
            float advance = mAdvance * mSampleLoopDirection;
            mPosition2 += advance;

            SAMPLE out2{};
            if (advance > 0.0f) {
                out2 = mDirectSampler->template Read<STRATEGY, kNoLoop, kForwards>(mPosition2);
            } else {
                out2 = mDirectSampler->template Read<STRATEGY, kNoLoop, kBackwards>(mPosition2);
            }
            return kitdsp::lerp(out1, out2, mFade.current);
        } else {
            return out1;
        }
    }

    etl::span<SAMPLE> mRawSamples{};
    std::optional<Sampler1D<SAMPLE>> mDirectSampler{};
    std::optional<Sampler1D<SAMPLE>> mLoopSampler{};
    kitdsp::Approach mFade{};
    size_t mLoopStart = 0;
    size_t mLoopEnd = SIZE_MAX;

    float mBufSampleRate{};
    float mAdvance = 1.0f;
    float mSampleLoopDirection = 1.0f;
    float mPosition1 = 0.0f;
    float mPosition2 = 0.0f;

    SampleLoopDirection mDirection = SampleLoopDirection::NoLoop;
    LoopState mState = LoopState::Exit;
    interpolate::InterpolationStrategy mInterpolate = interpolate::InterpolationStrategy::Cubic;
    bool mPlaying = false;
};
}  // namespace kitdsp