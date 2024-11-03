#pragma once
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
    }

    inline void SetFreqCarrier(const float f)
    {
        mFreqCarrier = f;
    }

    inline void SetFreqModulator(const float f)
    {
        mFreqCarrier = f;
    }

    inline void SetFalloff(const float f)
    {
        mFalloff = f;
    }

    float Process();

    void PhaseAdd(float _phase)
    {
        mPhaseCarrier += _phase;
        mPhaseModulator += _phase * mFreqModulator / mFreqCarrier;
    }

    void Reset(float _phase = 0.0f)
    {
        mPhaseCarrier = _phase;
        mPhaseModulator = _phase * mFreqModulator / mFreqCarrier;
    }

private:
    float mFreqCarrier{100.0f};
    float mPhaseCarrier{0.0f};

    float mFreqModulator{200.0f};
    float mPhaseModulator{0.0f};

    float mFalloff{0.0f};
    float mSecondsPerSample{1.0f / 41000.0f};
};
