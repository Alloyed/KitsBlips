#include "kitdsp/osc/dsfOscillator.h"

#include "kitdsp/lookupTables/sineLut.h"
#include "kitdsp/math/util.h"
#include "kitdsp/osc/oscillatorUtil.h"

using namespace kitdsp;

#define sin_(x) sinf(x *kTwoPi)
#define cos_(x) cosf(x *kTwoPi)
//#define sin_(x) sin2pif_lut(x)
//#define cos_(x) sin2pif_lut((x) + 0.25f)

void DsfOscillator::Process() {
    mPhaseCarrier = Phasor::WrapPhase(mPhaseCarrier + mFreqCarrier * mSecondsPerSample);
    mPhaseModulator = Phasor::WrapPhase(mPhaseModulator + mFreqModulator * mSecondsPerSample);
}

float DsfOscillator::Formula1() const {
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

    return blockNanf(num / (denom * b));
}

float DsfOscillator::Formula2() const {
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

    return blockNanf(num / (denom * b));
}

float DsfOscillator::Formula3() const {
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

    return blockNanf(num / (denom * b));
}

float DsfOscillator::Formula4() const {
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

    return blockNanf(num / (denom * b));
}
