#pragma once

#include <etl/span.h>
#include <kitdsp/control/approach.h>

/**
 * Detects the pitch of a signal by counting the number of zero crossings
 * This works well for very simple monophonic signals, such as those produced by synth oscillators
 */

 namespace kitdsp {
  namespace pitch {

    class ZeroCrossingPitchDetector {
      public:
      ZeroCrossingPitchDetector(float sampleRate);
      void Reset();

      void Process(etl::span<float> input);
      void Process(float input);

      float GetFrequency() const;
      float GetPeriod() const;

      private:
      float mSampleRate;
      float mLastSample = 0.0f;
      size_t mNextPeriod = 0;
      Approach mSmoothedPeriod;
    };
 }
  }