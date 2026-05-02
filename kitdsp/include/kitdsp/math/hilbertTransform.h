#pragma once

/*
 * This file is adapted from the surgeXT project, original URL at:
 *
 *  https://github.com/surge-synthesizer/sst-basic-blocks/blob/main/include/sst/basic-blocks/dsp/HilbertTransform.h
 *
 * The following comment is from that file:
 *
 * ---
 *
 * This file is available for use under the MIT license or
 * the GPL3 license, as described in README.
 *
 * Thanks to Sean Costello for the conversation which led
 * to BaconPaul understanding the hilbert transform in the
 * context of pitch shifters, and from sharing this
 * serial-biquad implementation with allpass coefficients.
 * The coefficients first appear in one of the
 * Electronotes series papers by Bernie Hutchins.
 *
 * The hilbert transform takes a real valued signal
 * and returns a complex valued signal which allows you
 * to create the "analytic signal" - namely the signal with
 * only positive frequencies. That is, take a real signal
 * u(t) = cos(wt) and imagine it as a complex signal
 * U(t) = 0.5 (e^iwt + e^-iwt).  The modified signal
 * v(t) = H(u(t)) contains only the positive
 * frequency. Basically at this point go read wikipedia
 * and stuff, but that gives you loads of cool properties
 * for modifying phase and pitch in the complex plane.
 *
 * The actual calculation of the hilbert transform numerically
 * is tricky, which is why we are particularly grateful to
 * Sean for sharing this implementation (which he shared
 * in float form, and which is presented here both in
 * mono-float form and in stereo-SSE form).
 */

#include <math.h>

#include <cassert>
#include <complex>
#include <utility>
#include "kitdsp/math/util.h"

namespace kitdsp {
struct HilbertTransformMonoFloat {
    struct BQ {
        float a1{1}, a2{0}, b0{1}, b1{0}, b2{0}, reg0{0}, reg1{0};
        inline void reset() {
            reg0 = 0;
            reg1 = 0;
        }
        inline void setCoefs(float _a1, float _a2, float _b0, float _b1, float _b2) {
            a1 = _a1;
            a2 = _a2;
            b0 = _b0;
            b1 = _b1;
            b2 = _b2;
        }

        inline float step(float input) {
            double op{};

            op = input * b0 + reg0;
            reg0 = static_cast<float>(input * b1 - a1 * op + reg1);
            reg1 = static_cast<float>(input * b2 - a2 * op);

            return (float)op;
        }
    } allpass[2][3];

    float sampleRate{0};
    void setSampleRate(float sr) {
        sampleRate = sr;
        setHilbertCoefs();
    }

    float hilbertCoefs[12];
    void setHilbertCoefs() {
        assert(sampleRate);
        float a1{}, a2{}, b0{}, b1{}, b2{};

        // phase difference network normalized pole frequencies
        // first six numbers are for real, the rest for imaginary
        float poles[12] = {.3609f,  2.7412f, 11.1573f, 44.7581f, 179.6242f, 798.4578f,
                           1.2524f, 5.5671f, 22.3423f, 89.6271f, 364.7914f, 2770.1114f};

        float minFreq = 25.0f;  // minimum frequency of phase differencing network, in Hertz

        // bilinear z-transform for calculating coefficients for 1st order allpasses
        for (int j = 0; j < 12; j++)
            hilbertCoefs[j] =
                (1.0f - (minFreq * kPi * poles[j]) / sampleRate) / (1.0f + (minFreq * kPi * poles[j]) / sampleRate);

        // calculate biquad coeffcients for 2nd order allpasses, using the first order allpasses
        // in the following order: highest (in a branch) by lowest, next highest by next lowest,
        // next highest by next lowest.

        for (int j = 0; j < 3; j++) {
            allpass[0][j].reset();  // clears states
            a1 = -(hilbertCoefs[j] + hilbertCoefs[5 - j]);
            a2 = hilbertCoefs[j] * hilbertCoefs[5 - j];
            b0 = hilbertCoefs[j] * hilbertCoefs[5 - j];
            b1 = -(hilbertCoefs[j] + hilbertCoefs[5 - j]);
            b2 = 1.0f;
            allpass[0][j].setCoefs(a1, a2, b0, b1, b2);
        }

        for (int j = 0; j < 3; j++) {
            allpass[1][j].reset();  // clears states
            a1 = -(hilbertCoefs[j + 6] + hilbertCoefs[11 - j]);
            a2 = hilbertCoefs[j + 6] * hilbertCoefs[11 - j];
            b0 = hilbertCoefs[j + 6] * hilbertCoefs[11 - j];
            b1 = -(hilbertCoefs[j + 6] + hilbertCoefs[11 - j]);
            b2 = 1.0f;
            allpass[1][j].setCoefs(a1, a2, b0, b1, b2);
        }
    }

    std::pair<float, float> stepPair(float in) {
        float im{in}, re{in};

        for (int i = 0; i < 3; ++i) {
            re = allpass[0][i].step(re);
            im = allpass[1][i].step(im);
        }
        return {re, im};
    }

    std::complex<float> stepComplex(float in) {
        auto pair = stepPair(in);
        return {pair.first, pair.second};
    }
};
}  // namespace kitdsp