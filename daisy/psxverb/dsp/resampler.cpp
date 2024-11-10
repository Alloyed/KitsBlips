#include "dsp/resampler.h"

Resampler::Resampler(float sourceRate, float targetRate)
: mPeriod(targetRate / sourceRate)
{
}