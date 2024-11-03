#pragma once

#include "dsp/util.h"
#include <cstdint>
#include <cstddef>

class Resampler
{
  // TODO: we should filter the inputs to avoid aliasing
  // https://dspguru.com/dsp/faqs/multirate/decimation/
public:
  enum class InterpolationStrategy
  {
    None,
    Linear,
    Hermite,
    Cubic
  };

  Resampler(float sourceRate, float targetRate);

  template <typename F>
  void Process(float inputLeft,
               float inputRight,
               float &outputLeft,
               float &outputRight,
               InterpolationStrategy strategy,
               F &&callback)
  {
    mSampleCounter += 1.0f;
    if (mSampleCounter > mPeriod * 10000.0f)
    {
      // prevent extreme overrun scenarios
      mSampleCounter = mPeriod;
    }

    while (mSampleCounter > mPeriod)
    {
      // shift
      mOutputLeft[0] = mOutputLeft[1];
      mOutputLeft[1] = mOutputLeft[2];
      mOutputLeft[2] = mOutputLeft[3];

      mOutputRight[0] = mOutputRight[1];
      mOutputRight[1] = mOutputRight[2];
      mOutputRight[2] = mOutputRight[3];

      callback(inputLeft, inputRight, mOutputLeft[3], mOutputRight[3]);
      mSampleCounter -= mPeriod;
    }
    float t = mSampleCounter / mPeriod;

    // Linear interpolation
    switch (strategy)
    {
    case InterpolationStrategy::None:
    {
      outputLeft = mOutputLeft[3];
      outputRight = mOutputRight[3];
    }
    break;
    case InterpolationStrategy::Linear:
    {
      outputLeft = lerpf(mOutputLeft[2], mOutputLeft[3], t);
      outputRight = lerpf(mOutputRight[2], mOutputRight[3], t);
    }
    break;
    case InterpolationStrategy::Hermite:
    {
      outputLeft = lerpHermite4pt3oXf(mOutputLeft[0], mOutputLeft[1], mOutputLeft[2], mOutputLeft[3], t);
      outputRight = lerpHermite4pt3oXf(mOutputRight[0], mOutputRight[1], mOutputRight[2], mOutputRight[3], t);
    }
    break;
    case InterpolationStrategy::Cubic:
    {
      outputLeft = lerpCubicf(mOutputLeft[0], mOutputLeft[1], mOutputLeft[2], mOutputLeft[3], t);
      outputRight = lerpCubicf(mOutputRight[0], mOutputRight[1], mOutputRight[2], mOutputRight[3], t);
    }
    break;
    }
  }

private:
  float mOutputLeft[4]{};
  float mOutputRight[4]{};
  float mSampleCounter{};
  float mPeriod{};
};