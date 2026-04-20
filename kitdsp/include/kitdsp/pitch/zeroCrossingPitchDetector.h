#pragma once

#include <etl/span.h>

/**
 * Detects the pitch of a signal by counting the number of zero crossings
 * This works well for very simple monophonic signals, such as those produced by synth oscillators
 */

 namespace kitdsp {
  namespace pitch {

    class ZeroCrossingPitchDetector {
      public:
      ZeroCrossingPitchDetector();
      void Reset();

      void Process(etl::span<float> input);
      void Process(float input);

      float GetFrequency() const;

      private:
      float mSampleRate;
      float mLastSample;
    };
 }
  }