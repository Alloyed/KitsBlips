#include "dsp/dsfOscillator.h"

#include <cmath>

#include "dsp/util.h"

namespace
{
    // https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/

    // period is [0,1] instead of [0,2pi]
    float approxSinf(float x)
    {
        // limit range
        x = x - static_cast<uint8_t>(x) - 0.5f;

        return 2.0f * x * (1.0f - fabs(2.0f * x));
    }

    // period is [0,1] instead of [0,2pi]
    float approxCosf(float x)
    {
        approxSinf(x - 0.5f);
    }

}

#define sin_(x) approxSinf(x)
#define cos_(x) approxCosf(x)
// #define sin_(x) sinf(x * cTwoPi)
// #define cos_(x) cosf(x * cTwoPi)

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