#include "dsp/dsfOscillator.h"

#include <cmath>

#include "dsp/util.h"

// #define sin_(x) approxSinf(x)
// #define cos_(x) approxCosf(x)
#define sin_(x) sinf(x *cTwoPi)
#define cos_(x) cosf(x *cTwoPi)

void DsfOscillator::Process(float &out1, float &out2)
{
    mPhaseCarrier += mFreqCarrier * mSecondsPerSample;
    mPhaseCarrier = fmodf(mPhaseCarrier, 1.0f);

    mPhaseModulator += mFreqModulator * mSecondsPerSample;
    mPhaseModulator = fmodf(mPhaseModulator, 1.0f);

    out1 = mDcBlocker1.Process(blockNanf(Formula1()));
    out2 = mDcBlocker2.Process(blockNanf(Formula3()));
}

float DsfOscillator::Formula1() const
{
    float theta = mPhaseCarrier;
    float beta = mPhaseModulator;
    float N = mNumFrequencyBands;
    float a = mFalloff;
    float a2 = mFalloffPow2;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up, band-limited
     */
    float bandlimit = mFalloffPowN1 * (sin_(theta + (N + 1) * beta) - (a * sin_(theta + (N * beta))));
    float num = sin_(theta) - (a * sin_(theta - beta)) - bandlimit;
    float denom = 1.0f + a2 - (2.0f * a * cos_(beta));

    return num / (denom * b);
}

float DsfOscillator::Formula2() const
{
    float theta = mPhaseCarrier;
    float beta = mPhaseModulator;
    float a = mFalloff;
    float a2 = mFalloffPow2;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up, not band-limited
     */
    float num = sin_(theta) - (a * sin_(theta - beta));
    float denom = 1.0f + a2 - (2.0f * a * cos_(beta));

    return num / (denom * b);
}

float DsfOscillator::Formula3() const
{
    float theta = mPhaseCarrier;
    float beta = mPhaseModulator;
    float N = mNumFrequencyBands;
    float a = mFalloff;
    float a2 = mFalloffPow2;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up+down, band-limited
     */

    float num = sin_(theta) * (1.0f - a2 - (2.0f * mFalloffPowN1 * (cos_((N + 1.0f) * beta) - a * cos_(N * beta))));
    float denom = 1.0f + a2 - (2.0f * a * cos_(beta));

    return num / (denom * b);
}

float DsfOscillator::Formula4() const
{
    float theta = mPhaseCarrier;
    float beta = mPhaseModulator;
    float a = mFalloff;
    float a2 = mFalloffPow2;
    float b = mAmplitudeReciprocal;

    /**
     * sidebands up+down, not band-limited
     */
    float num = (1.0f - a2) * sin_(theta);
    float denom = 1.0f + a2 - (2.0f * a * cos_(beta));

    return num / (denom * b);
}