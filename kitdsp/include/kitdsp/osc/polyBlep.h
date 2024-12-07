// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Polynomial approximation of band-limited step for band-limited waveform
// synthesis.

// derived from: https://github.com/pichenettes/stmlib/blob/master/dsp/polyblep.h
// blep :p

#pragma once

namespace kitdsp {
/*
 * PolyBLEPs are closely related to the minBLEP approach, where you insert
 * a precomputed band-limited impulse response whenever you encounter a
 * discontinuity in your waveform. it's a little worse, and a little
 * faster, because it uses a polynomial approximation that is only defined
 * for two samples.
 * for more info on MinBLEPs:
 *    https://www.experimentalscene.com/articles/minbleps.php
 * and a short evaluation of PolyBLEPs in comparison:
 *    https://www.kvraudio.com/forum/viewtopic.php?t=375517
 * Using the Blep methods directly can be more performant, but as a starting point PolyBlepProcessor can help you
 * organize your code to work with polybleps.
 */

namespace polyblep {
template <typename SAMPLE>
inline float ThisBlepSample(SAMPLE t) {
    return SAMPLE(0.5f) * t * t;
}

template <typename SAMPLE>
inline float NextBlepSample(SAMPLE t) {
    t = SAMPLE(1.0f) - t;
    return SAMPLE(-0.5f) * t * t;
}

/*
 * Integrated bleps are the same as the direct blep, but pre-integrated for your
 * convenience. use when your discontinuity happens in the derivative of your function and not directly
 */

template <typename SAMPLE>
inline float NextIntegratedBlepSample(SAMPLE t) {
    const float t1 = SAMPLE(0.5f) * t;
    const float t2 = t1 * t1;
    const float t4 = t2 * t2;
    return SAMPLE(0.1875f) - t1 + SAMPLE(1.5f) * t2 - t4;
}

template <typename SAMPLE>
inline float ThisIntegratedBlepSample(SAMPLE t) {
    return NextIntegratedBlepSample(SAMPLE(1.0f) - t);
}

template <typename SAMPLE>
class PolyBlepProcessor {
   public:
    /**
     * @param timeSinceDiscontinuitySamples It's assumed that your discontinuity happened at some fractional point in
     *   the past. Use this to mark how long in the past it was (usually sub-sample)
     * @param magnitudeAndDirection how big was the discontinuity, and was it jumping from high or low or the reverse?
     */
    void InsertDiscontinuity(SAMPLE timeSinceDiscontinuitySamples, SAMPLE magnitudeAndDirection) {
        mThisSample += ThisBlepSample(timeSinceDiscontinuitySamples) * magnitudeAndDirection;
        mNextSample += NextBlepSample(timeSinceDiscontinuitySamples) * magnitudeAndDirection;
    }

    /**
     * @param timeSinceDiscontinuitySamples It's assumed that your discontinuity happened at some fractional point in
     *   the past. Use this to mark how long in the past it was (usually sub-sample)
     * @param magnitudeAndDirection how big was the discontinuity, and was it jumping from high or low or the reverse?
     */
    void InsertIntegratedDiscontinuity(SAMPLE timeSinceDiscontinuitySamples, SAMPLE magnitudeAndDirection) {
        mThisSample += ThisIntegratedBlepSample(timeSinceDiscontinuitySamples) * magnitudeAndDirection;
        mNextSample += NextIntegratedBlepSample(timeSinceDiscontinuitySamples) * magnitudeAndDirection;
    }

    SAMPLE Process(SAMPLE input) {
        SAMPLE thisSample = mThisSample;
        mThisSample = mNextSample;
        mNextSample = SAMPLE(0.0f);
        return input + thisSample;
    }

   private:
    SAMPLE mThisSample = 0.0f;
    SAMPLE mNextSample = 0.0f;
};

}  // namespace polyblep
}  // namespace kitdsp