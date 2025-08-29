#pragma once

#include <cmath>
#include "kitdsp/math/util.h"

namespace kitdsp {
/*
 * From https://webaudio.github.io/Audio-EQ-Cookbook/Audio-EQ-Cookbook.txt
 * RBJ is Robert Bristow-Johnson.
 */
namespace rbj {

enum class BiquadFilterMode {
    LowPass,
    Highpass,
    BandPass,
    Notch,
    AllPass,
    LowShelf,
    HighShelf,
};

template <BiquadFilterMode MODE>
class BiquadFilter {
   public:
    BiquadFilter() {
        SetFrequency(1200.0f, 44100.0f);
        SetShelf(0.5f, -6.0f);
    }
    inline void Reset() {
        x[0] = {};
        x[1] = {};
        y[0] = {};
        y[1] = {};
    }
    /**
     * Set frequency
     * @param frequencyHz in hertz
     * @param sampleRate should be the rate Process() is called at
     */
    inline void SetFrequency(float frequency, float sampleRate) {
        float w0 = 2 * kPi * frequency / sampleRate;
        cos_w0 = cosf(w0);
        sin_w0 = sinf(w0);
        CalculateCoefficients();
    }

    /* Set Q Factor of the filter. */
    inline void SetQ(float Q_) {
        Q = Q_;
        CalculateCoefficients();
    }

    /**
     * Set frequency + resonance.
     * @param Q
     * @param dbGain
     */
    inline void SetShelf(float Q_, float dbGain) {
        Q = Q_;
        A = std::sqrt(std::pow(10.0f, dbGain / 20.0f));
        A_p_1 = A + 1.0f;
        A_m_1 = A - 1.0f;
        CalculateCoefficients();
    }

    inline float Process(float in) {
        // clang-format off
        float out = (b0 * in)
                  + (b1 * x[0])
                  + (b2 * x[1])
                  - (a1 * y[0])
                  - (a2 * y[1]);
        // clang-format on

        x[1] = x[0];
        x[0] = in;
        y[1] = y[0];
        y[0] = out;

        return out;
    }

   private:
    inline void CalculateCoefficients() {
        if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::LowPass) {
            // H(s) = 1 / (s^2 + s/Q + 1)
            float alpha = sin_w0 / (2.0f * Q);
            float a0 = 1.0f + alpha;

            b0 = ((1.0f - cos_w0) / 2.0f) / a0;
            b1 = (1.0f - cos_w0) / a0;
            b2 = ((1.0f - cos_w0) / 2.0f) / a0;
            a1 = (-2.0f * cos_w0) / a0;
            a2 = (1.0f - alpha) / a0;

        } else if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::Highpass) {
            // H(s) = s^2 / (s^2 + s/Q + 1)
            float alpha = sin_w0 / (2.0f * Q);
            float a0 = 1.0f + alpha;

            b0 = ((1.0f + cos_w0) / 2.0f) / a0;
            b1 = -(1.0f + cos_w0) / a0;
            b2 = ((1.0f + cos_w0) / 2.0f) / a0;
            a1 = (-2.0f * cos_w0) / a0;
            a2 = (1.0f - alpha) / a0;

        } else if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::BandPass) {
            // H(s) = s / (s^2 + s/Q + 1)  (constant skirt gain, peak gain = Q)
            float alpha = sin_w0 / (2.0f * Q);
            float a0 = 1 + alpha;

            b0 = (sin_w0 / 2.0f) / a0;
            b1 = 0.0f;
            b2 = (-sin_w0 / 2.0f) / a0;
            a1 = (-2.0f * cos_w0) / a0;
            a2 = (1.0f - alpha) / a0;

        } else if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::Notch) {
            // H(s) = (s^2 + 1) / (s^2 + s/Q + 1)
            float alpha = sin_w0 / (2.0f * Q);
            float a0 = 1.0f + alpha;

            b0 = 1.0f / a0;
            b1 = (-2.0f * cos_w0) / a0;
            b2 = b0;
            a1 = b1;
            a2 = (1.0f - alpha) / a0;

        } else if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::AllPass) {
            // H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)
            float alpha = sin_w0 / (2.0f * Q);
            float a0 = 1 + alpha;

            b0 = (1.0f - alpha) / a0;
            b1 = (-2.0f * cos_w0) / a0;
            b2 = 1.0f;  // (1+alpha) / a0
            a1 = b1;
            a2 = b0;
        } else if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::LowShelf) {
            // H(s) = A * (s^2 + (sqrt(A)/Q)*s + A)/(A*s^2 + (sqrt(A)/Q)*s + 1)
            float alpha = sin_w0 / (2.0f * Q);
            float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
            float a0 = A_p_1 + A_m_1 * cos_w0 + twoSqrtAAlpha;

            b0 = A * (A_p_1 - A_m_1 * cos_w0 + twoSqrtAAlpha) / a0;
            b1 = 2.0f * A * (A_m_1 - A_p_1 * cos_w0) / a0;
            b2 = A * (A_p_1 - A_m_1 * cos_w0 - twoSqrtAAlpha) / a0;
            a1 = -2.0f * (A_m_1 + A_p_1 * cos_w0) / a0;
            a2 = (A_p_1 + A_m_1 * cos_w0 - twoSqrtAAlpha) / a0;
        } else if constexpr (MODE == kitdsp::rbj::BiquadFilterMode::HighShelf) {
            // H(s) = A * (A*s^2 + (sqrt(A)/Q)*s + 1)/(s^2 + (sqrt(A)/Q)*s + A)
            float alpha = sin_w0 / (2.0f * Q);
            float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
            float a0 = A_p_1 - A_m_1 * cos_w0 + twoSqrtAAlpha;

            b0 = A * (A_p_1 + A_m_1 * cos_w0 + twoSqrtAAlpha) / a0;
            b1 = -2.0f * A * (A_m_1 + A_p_1 * cos_w0) / a0;
            b2 = A * (A_p_1 + A_m_1 * cos_w0 - twoSqrtAAlpha) / a0;
            a1 = 2.0f * (A_m_1 - A_p_1 * cos_w0) / a0;
            a2 = (A_p_1 - A_m_1 * cos_w0 - twoSqrtAAlpha) / a0;
        }
    }

    // inputs
    float cos_w0;
    float sin_w0;
    float Q;
    float A;
    float A_p_1;  // A+1
    float A_m_1;  // A-1

    // coefficients, all normalized to a0
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;

    // sample buffers
    float x[2];
    float y[2];
};
}  // namespace rbj
}  // namespace kitdsp
