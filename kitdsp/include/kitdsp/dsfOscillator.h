#pragma once

#include <cmath>
#include <cstdint>
#include "kitdsp/dcBlocker.h"

namespace kitdsp
{
    /**
     *
     * DSF stands for Discrete Summation Formula, sometimes referred to as "Moorer Synthesis"
     *
     * all documentation i could find on dsf synthesis:
     *
     * https://ccrma.stanford.edu/files/papers/stanm5.pdf
     * https://www.verklagekasper.de/synths/dsfsynthesis/dsfsynthesis.html
     * https://manuals.noiseengineering.us/li/#algorithm-ss
     * https://www.youtube.com/watch?v=IoAc2241gx8
     *
     */
    class DsfOscillator
    {
    public:
        DsfOscillator() {}
        ~DsfOscillator() {}
        void Init(float sampleRate)
        {
            mSecondsPerSample = 1.0f / sampleRate;
            mNyquistFrequency = sampleRate * 0.5f;
            CalcFrequencyBands();
        }

        inline void SetFreqCarrier(const float f)
        {
            if (f != mFreqCarrier)
            {
                mFreqCarrier = f;
                CalcFrequencyBands();
            }
        }

        inline void SetFreqModulator(const float f)
        {
            if (f != mFreqModulator)
            {
                mFreqModulator = f;
                CalcFrequencyBands();
            }
        }

        inline void SetFalloff(const float f)
        {
            if (f != mFalloff)
            {
                mFalloff = f;
                mFalloffPow2 = f * f;
                CalcAmplitude();
            }
        }

        void Process(float &out1, float &out2);

        void PhaseAdd(float _phase)
        {
            mPhaseCarrier += _phase;
            mPhaseModulator += _phase * mFreqModulator / mFreqCarrier;
        }

        void Reset(float _phase = 0.0f)
        {
            mPhaseCarrier = _phase;
            mPhaseModulator = _phase * mFreqModulator / mFreqCarrier;
            mDcBlocker1.Reset();
            mDcBlocker2.Reset();
        }

    private:
        // params
        float mFreqCarrier{100.0f};
        float mFreqModulator{200.0f};
        float mFalloff{0.0f};

        float mSecondsPerSample{1.0f / 41000.0f};

        // state
        float mPhaseCarrier{0.0f};
        float mPhaseModulator{0.0f};
        DcBlocker mDcBlocker1;
        DcBlocker mDcBlocker2;

        // cached intermediates
        float mNyquistFrequency{41000.0f * 0.5f};
        float mAmplitudeReciprocal{1.0f};
        float mNumFrequencyBands{0.0f};
        float mFalloffPow2{0.0f};
        float mFalloffPowN1{0.0f};

        inline void CalcAmplitude()
        {
            mFalloffPowN1 = powf(mFalloff, mNumFrequencyBands + 1);
            mAmplitudeReciprocal = (1.0f - powf(mFalloff, mNumFrequencyBands)) / (1.0f - mFalloff);
        }

        inline void CalcFrequencyBands()
        {
            float N = static_cast<int32_t>((mNyquistFrequency - mFreqCarrier) / mFreqModulator);
            if (N != mNumFrequencyBands)
            {
                mNumFrequencyBands = N;
                CalcAmplitude();
            }
        }

        float Formula1() const;
        float Formula2() const;
        float Formula3() const;
        float Formula4() const;
    };
}