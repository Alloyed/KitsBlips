#include "dsp/dsfOscillator.h"

#include <cmath>

#include "dsp/util.h"

float DsfOscillator::Process()
{
    mPhaseCarrier += mFreqCarrier * mSecondsPerSample;
    mPhaseCarrier = fmodf(mPhaseCarrier, 1.0f);

    mPhaseModulator += mFreqModulator * mSecondsPerSample;
    mPhaseModulator = fmodf(mPhaseModulator, 1.0f);

    return Formula2();
}

float DsfOscillator::Formula1() const
{
    float theta = mPhaseCarrier * cTwoPi;
    float beta = mPhaseModulator * cTwoPi;
    float N = mNumFrequencyBands;
    float a = mFalloff;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up, band-limited
     */
    float bandlimit = powf(a, N + 1) * (sinf(theta + (N + 1) * beta) - (a * sinf(theta + (N * beta))));
    float num = sinf(theta) - (a * sinf(theta - beta)) - bandlimit;
    float denom = 1.0f + (a * a) - (2.0f * a * cosf(beta));

    return num / (denom * b);
}

float DsfOscillator::Formula2() const
{
    float theta = mPhaseCarrier * cTwoPi;
    float beta = mPhaseModulator * cTwoPi;
    float N = mNumFrequencyBands;
    float a = mFalloff;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up, not band-limited
     */
    float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

    float num = sinf(theta) - (a * sinf(theta - beta));
    float denom = 1.0f + (a * a) - (2.0f * a * cosf(beta));

    return num / (denom * b);
}

float DsfOscillator::Formula3() const
{
    float theta = mPhaseCarrier * cTwoPi;
    float beta = mPhaseModulator * cTwoPi;
    float N = mNumFrequencyBands;
    float a = mFalloff;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up+down, band-limited
     */
    float a2 = a * a;

    float num = sinf(theta) * (1.0f - a2 - (2.0f * powf(a, N + 1) * (cosf((N + 1.0f) * beta) - a * cosf(N * beta))));
    float denom = 1 + (a2) - (2 * a * cosf(beta));

    return num / (denom * b);
}

float DsfOscillator::Formula4() const
{
    float theta = mPhaseCarrier * cTwoPi;
    float beta = mPhaseModulator * cTwoPi;
    float N = mNumFrequencyBands;
    float a = mFalloff;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up+down, not band-limited
     */
    float num = (1.0f - a * a) * sinf(theta);
    float denom = 1 + (a * a) - (2 * a * cosf(beta));

    return num / (denom * b);
}