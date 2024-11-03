#include "dsp/dsfOscillator.h"

#include <cmath>

#include "dsp/util.h"

namespace
{
    /**
     * sidebands up, not band-limited
     */
    float moorerFormula2(float theta, float beta, float a)
    {
        // this is the infinite form, so N is technically also infinite. I just picked a big number :)
        float N = 16;

        float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

        float num = sin(theta) - (a * sin(theta - beta));
        float denom = 1 + (a * a) - (2 * cos(beta));

        return num / (denom * maxAmplitude);
    }

    /**
     * sidebands up+down, not band-limited
     */
    float moorerFormula4(float theta, float beta, float a)
    {
        // this is the infinite form, so N is technically also infinite. I just picked a big number :)
        float N = 16;

        float maxAmplitude = (1.0f - powf(a, N)) / (1.0f - a);

        float num = (1.0f - a * a) * sin(theta);
        float denom = 1 + (a * a) - (2 * a * cos(beta));

        return num / (denom * maxAmplitude);
    }
}

float DsfOscillator::Process()
{
    mPhaseCarrier += mFreqCarrier * mSecondsPerSample;
    if (mPhaseCarrier > 1.0f)
    {
        mPhaseCarrier -= 1.0f;
    }
    mPhaseModulator += mFreqModulator * mSecondsPerSample;
    if (mPhaseModulator > 1.0f)
    {
        mPhaseModulator -= 1.0f;
    }
    return moorerFormula2(mPhaseCarrier * cTwoPi, mPhaseModulator * cTwoPi, mFalloff);
}