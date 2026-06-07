#pragma once

#include <etl/span.h>
#include "kitdsp/math/util.h"
#include "kitdsp/math/vector.h"

namespace kitdsp {
/**
 * A crossover filter splits a signal into low and high frequencies. When adding
 * them back together, the result is all-pass: every frequency's magnitude is
 * preserved, but the phases will be a little different. You can use this for
 * multiband processing, or, in the real world, sending different bands to
 * different speakers (this is how subwoofer/midrange/tweeter setups work)
 *
 * This is the 4th order Linkwitz-Riley filter, as described here:
 * https://www.musicdsp.org/en/latest/Filters/266-4th-order-linkwitz-riley-filters.html
 */
class CrossoverFilter {
   public:
    /**
     * Set frequency + resonance.
     * @param frequencyHz in hertz
     * @param sampleRate should be the rate Process() is called at
     */
    inline void SetFrequency(float crossoverFrequency, float sampleRate) {
        if (crossoverFrequency == mFrequency && sampleRate == mSampleRate) {
            return;
        }
        mFrequency = crossoverFrequency;
        mSampleRate = sampleRate;

        // shared coefficients
        double cowc = kitdsp::kTwoPi * mFrequency;
        double cowc2 = cowc * cowc;
        double cowc3 = cowc2 * cowc;
        double cowc4 = cowc2 * cowc2;

        double cok = cowc / std::tan(kitdsp::kPi * mFrequency / sampleRate);
        double cok2 = cok * cok;
        double cok3 = cok2 * cok;
        double cok4 = cok2 * cok2;
        double sq_tmp1 = kitdsp::kSqrtTwo * cowc3 * cok;
        double sq_tmp2 = kitdsp::kSqrtTwo * cowc * cok3;
        double a_tmp = 4.0 * cowc2 * cok2 + 2.0 * sq_tmp1 + cok4 + 2.0 * sq_tmp2 + cowc4;

        bCoef.d1 = (4.0 * (cowc4 + sq_tmp1 - cok4 - sq_tmp2)) / a_tmp;
        bCoef.d2 = (6.0 * cowc4 - 8.0 * cowc2 * cok2 + 6.0 * cok4) / a_tmp;
        bCoef.d3 = (4.0 * (cowc4 - sq_tmp1 + sq_tmp2 - cok4)) / a_tmp;
        bCoef.d4 = (cok4 - 2.0 * sq_tmp1 + cowc4 - 2.0 * sq_tmp2 + 4.0 * cowc2 * cok2) / a_tmp;

        // lowpass
        lpCoef0 = cowc4 / a_tmp;
        lpCoef.d1 = 4.0 * cowc4 / a_tmp;
        lpCoef.d2 = 6.0 * cowc4 / a_tmp;
        lpCoef.d3 = lpCoef.d1;
        lpCoef.d4 = lpCoef0;

        // highpass
        hpCoef0 = cok4 / a_tmp;
        hpCoef.d1 = -4.0 * cok4 / a_tmp;
        hpCoef.d2 = 6.0 * cok4 / a_tmp;
        hpCoef.d3 = hpCoef.d1;
        hpCoef.d4 = hpCoef0;
    }
    inline void Reset() {
        x = {};
        lpy = {};
        hpy = {};
    }
    inline void Process(float in, float& hp, float& lp) {
        double tempx = in;
        double tempy = 0.0;

        // High pass
        tempy = hpCoef0 * tempx + ((hpCoef * x) - (bCoef * hpy)).sum();
        hp = narrow_cast<float>(tempy);
        Shift(hpy, tempy);

        // Low pass
        tempy = lpCoef0 * tempx + ((lpCoef * x) - (bCoef * lpy)).sum();
        lp = narrow_cast<float>(tempy);
        Shift(lpy, tempy);
        Shift(x, tempx);
    }

    inline void ProcessBlock(etl::span<float> in, etl::span<float> hp, etl::span<float> lp) {
        // slightly weird: we want to increment x for each loop, but restart it when we move onto the next one. the
        // final state should be the last 4 samples as usual
        double_4 xinit = x;

        // High pass
        for (size_t t = 0; t < in.size(); ++t) {
            double tempy = hpCoef0 * in[t] + ((hpCoef * x) - (bCoef * hpy)).sum();
            hp[t] = narrow_cast<float>(tempy);
            Shift(hpy, tempy);
            Shift(x, in[t]);
        }

        // Low pass
        x = xinit;
        for (size_t t = 0; t < in.size(); ++t) {
            double tempy = lpCoef0 * in[t] + ((lpCoef * x) - (bCoef * hpy)).sum();
            lp[t] = narrow_cast<float>(tempy);
            Shift(lpy, tempy);
            Shift(x, in[t]);
        }
    }

   private:
    double lpCoef0;
    double_4 lpCoef;
    double hpCoef0;
    double_4 hpCoef;
    double_4 bCoef;

    double_4 x;
    double_4 lpy;
    double_4 hpy;

    static void Shift(double_4& m, double m0) {
        m.d4 = m.d3;
        m.d3 = m.d2;
        m.d2 = m.d1;
        m.d1 = m0;
    }

    float mFrequency;
    float mSampleRate;
};
}  // namespace kitdsp