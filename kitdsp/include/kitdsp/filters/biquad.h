#pragma once

#include <cmath>
#include <complex>
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

class BiquadFilter {
   public:
    BiquadFilter() {
        SetFrequency<BiquadFilterMode::LowPass>(1200.0f, 44100.0f);
        SetShelf<BiquadFilterMode::LowPass>(0.5f, -6.0f);
        Reset();
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
    template <BiquadFilterMode MODE>
    inline void SetFrequency(float frequency, float sampleRate) {
        float w0 = 2 * kPi * frequency / sampleRate;
        cos_w0 = cosf(w0);
        sin_w0 = sinf(w0);
        CalculateCoefficients<MODE>();
    }

    /* Set Q Factor of the filter. */
    template <BiquadFilterMode MODE>
    inline void SetQ(float Q_) {
        Q = Q_;
        CalculateCoefficients<MODE>();
    }

    /**
     * Sets the shelf factors of the filter (only necessary for low-shelf and high-shelf)
     * @param S 1 is maximum steepness. >1 increases the slope
     * @param dbGain in db.
     */
    template <BiquadFilterMode MODE>
    inline void SetShelf(float S, float dbGain) {
        Q = S;
        A = std::sqrt(std::pow(10.0f, dbGain / 20.0f));
        A_p_1 = A + 1.0f;
        A_m_1 = A - 1.0f;
        CalculateCoefficients<MODE>();
    }

    template <BiquadFilterMode MODE>
    inline std::complex<float> TransferFunction(std::complex<float> s);

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
    template <BiquadFilterMode MODE>
    void CalculateCoefficients();

    // inputs
    float cos_w0{};
    float sin_w0{};
    float Q{};
    float A{};
    float A_p_1{};  // A+1
    float A_m_1{};  // A-1

    // coefficients, all normalized to a0
    float b0{};
    float b1{};
    float b2{};
    float a1{};
    float a2{};

    // sample buffers
    float x[2];
    float y[2];
};

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::LowPass>(std::complex<float> s) {
    return 1.0f / ((s * s) + s / Q + 1.0f);
}

template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::LowPass>() {
    // H(s) = 1 / (s^2 + s/Q + 1)
    float alpha = sin_w0 / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b0 = ((1.0f - cos_w0) / 2.0f) / a0;
    b1 = (1.0f - cos_w0) / a0;
    b2 = ((1.0f - cos_w0) / 2.0f) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::Highpass>(std::complex<float> s) {
    // H(s) = s^2 / (s^2 + s/Q + 1)
    return (s * s) / ((s * s) + (s / Q) + 1.0f);
}

template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::Highpass>() {
    // H(s) = s^2 / (s^2 + s/Q + 1)
    float alpha = sin_w0 / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b0 = ((1.0f + cos_w0) / 2.0f) / a0;
    b1 = -(1.0f + cos_w0) / a0;
    b2 = ((1.0f + cos_w0) / 2.0f) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::BandPass>(std::complex<float> s) {
    // H(s) = s / (s^2 + s/Q + 1)  (constant skirt gain, peak gain = Q)
    return s / ((s * s) + (s / Q) + 1.0f);
}
template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::BandPass>() {
    // H(s) = s / (s^2 + s/Q + 1)  (constant skirt gain, peak gain = Q)
    float alpha = sin_w0 / (2.0f * Q);
    float a0 = 1 + alpha;

    b0 = (sin_w0 / 2.0f) / a0;
    b1 = 0.0f;
    b2 = (-sin_w0 / 2.0f) / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::Notch>(std::complex<float> s) {
    // H(s) = (s^2 + 1) / (s^2 + s/Q + 1)
    return (s * s + 1.0f) / ((s * s) + (s / Q) + 1.0f);
}
template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::Notch>() {
    // H(s) = (s^2 + 1) / (s^2 + s/Q + 1)
    float alpha = sin_w0 / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b0 = 1.0f / a0;
    b1 = (-2.0f * cos_w0) / a0;
    b2 = b0;
    a1 = b1;
    a2 = (1.0f - alpha) / a0;
}

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::AllPass>(std::complex<float> s) {
    // H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)
    return ((s * s) - (s / Q) + 1.0f) / ((s * s) + (s / Q) + 1.0f);
}
template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::AllPass>() {
    // H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)
    float alpha = sin_w0 / (2.0f * Q);
    float a0 = 1 + alpha;

    b0 = (1.0f - alpha) / a0;
    b1 = (-2.0f * cos_w0) / a0;
    b2 = 1.0f;  // (1+alpha) / a0
    a1 = b1;
    a2 = b0;
}

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::LowShelf>(std::complex<float> s) {
    // H(s) = A * (s^2 + (sqrt(A)/Q)*s + A)/(A*s^2 + (sqrt(A)/Q)*s + 1)
    return A * (((s * s) + (std::sqrt(A) / Q) * s + A) / (A * s * s + (std::sqrt(A) / Q) * s + 1.0f));
}
template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::LowShelf>() {
    // H(s) = A * (s^2 + (sqrt(A)/Q)*s + A)/(A*s^2 + (sqrt(A)/Q)*s + 1)
    float alpha = sin_w0 / (2.0f * Q);
    float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
    float a0 = A_p_1 + A_m_1 * cos_w0 + twoSqrtAAlpha;

    b0 = A * (A_p_1 - A_m_1 * cos_w0 + twoSqrtAAlpha) / a0;
    b1 = 2.0f * A * (A_m_1 - A_p_1 * cos_w0) / a0;
    b2 = A * (A_p_1 - A_m_1 * cos_w0 - twoSqrtAAlpha) / a0;
    a1 = -2.0f * (A_m_1 + A_p_1 * cos_w0) / a0;
    a2 = (A_p_1 + A_m_1 * cos_w0 - twoSqrtAAlpha) / a0;
}

template <>
inline std::complex<float> BiquadFilter::TransferFunction<BiquadFilterMode::HighShelf>(std::complex<float> s) {
    // H(s) = A * (A*s^2 + (sqrt(A)/Q)*s + 1)/(s^2 + (sqrt(A)/Q)*s + A)
    return A * (A * s * s + (std::sqrt(A) / Q) * s + 1.0f) / (s * s + (std::sqrt(A) / Q) * s + A);
}
template <>
inline void BiquadFilter::CalculateCoefficients<BiquadFilterMode::HighShelf>() {
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
}  // namespace rbj
}  // namespace kitdsp
