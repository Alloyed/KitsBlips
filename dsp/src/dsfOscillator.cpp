#include "dsp/dsfOscillator.h"

#include <cmath>

#include "dsp/util.h"

namespace
{
    /**
     * sidebands up, band-limited
     */
    float moorerFormula1(float theta, float beta, float N, float a)
    {
        float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

        float bandlimit = powf(a, N + 1) * (sinf(theta + (N + 1) * beta) - (a * sinf(theta + (N * beta))));
        float num = sinf(theta) - (a * sinf(theta - beta)) - bandlimit;
        float denom = 1.0f + (a * a) - (2.0f * a * cosf(beta));

        return num / (denom * maxAmplitude);
    }

    /**
     * sidebands up, not band-limited
     */
    float moorerFormula2(float theta, float beta, float N, float a)
    {
        // this is the infinite form, so N is technically also infinite. I just picked a big number :)
        float N = 16;

        float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

        float num = sinf(theta) - (a * sinf(theta - beta));
        float denom = 1.0f + (a * a) - (2.0f * a * cosf(beta));

        return num / (denom * maxAmplitude);
    }

    /**
     * sidebands up+down, band-limited
     */
    float moorerFormula3(float theta, float beta, float N, float a)
    {
        float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

        float a2 = a * a;

        float num = sinf(theta) * (1.0f - a2 - (2.0f * powf(a, N + 1) * (cosf((N + 1.0f) * beta) - a * cosf(N * beta))));
        float denom = 1 + (a2) - (2 * a * cosf(beta));

        return num / (denom * maxAmplitude);
    }

    /**
     * sidebands up+down, not band-limited
     */
    float moorerFormula4(float theta, float beta, float N, float a)
    {
        // this is the infinite form, so N is technically also infinite. I just picked a big number :)
        float N = 16;

        float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

        float num = (1.0f - a * a) * sinf(theta);
        float denom = 1 + (a * a) - (2 * a * cosf(beta));

        return num / (denom * maxAmplitude);
    }
}

float DsfOscillator::Process()
{
    mPhaseCarrier += mFreqCarrier * mSecondsPerSample;
    mPhaseCarrier = fmodf(mPhaseCarrier, 1.0f);

    mPhaseModulator += mFreqModulator * mSecondsPerSample;
    mPhaseModulator = fmodf(mPhaseModulator, 1.0f);

    return moorerFormula2(mPhaseCarrier * cTwoPi, mPhaseModulator * cTwoPi, 8, mFalloff);
}