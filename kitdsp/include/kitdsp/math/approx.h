#pragma once

#include <cmath>
#include <cstdint>
#include "kitdsp/math/util.h"
#include "util.h"

// approximations of common functions
namespace kitdsp {
namespace approx {
// you know what this is
inline float rsqrtf(float number) {
    const float threehalfs = 1.5f;
    float x2 = number * 0.5f;

    // evil floating point bit level hacking
    union {
        float f;
        uint32_t i;
    } y;

    y.f = number;
    y.i = 0x5f3759df - (y.i >> 1);                // what the fuck?
    y.f = y.f * (threehalfs - (x2 * y.f * y.f));  // 1st iteration
    // y.f  = y.f * ( threehalfs - ( x2 * y.f * y.f ) );   // 2nd iteration,
    // this can be removed

    return y.f;
}

// https://github.com/raphlinus/synthesizer-io/blob/master/synthesizer-io-core/benches/sigmoid.rs
inline float tanhf(float x) {
    float xx = x * x;
    float x1 = x + (0.16489087f + 0.00985468f * xx) * (x * xx);
    return x1 * rsqrtf(1.0f + x1 * x1);
}

// https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
// period is [0,1] instead of [0,2pi]
inline float sin2pif_nasty(float x) {
    // limit range
    x = x - truncf(x) - 0.5f;

    return 2.0f * x * (1.0f - fabsf(2.0f * x));
}

// https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
// period is [0,1] instead of [0,2pi]
inline float cos2pif_nasty(float x) {
    // limit range
    x = x - truncf(x + 0.25f) - 0.5f;

    return 2.0f * x * (1.0f - fabsf(2.0f * x));
}

/*
 * adapted from https://github.com/squinkylabs/Demo/blob/main/src/VCO3.cpp
 * input: _x must be >= 0, and <= 2 * pi.
 */
inline float sinf_squinky(float _x) {
    _x -= (_x > kPi) ? kTwoPi : 0.0f;

    bool xneg = _x < 0;
    float xOffset = xneg ? kPi / 2.f : -kPi / 2.f;
    xOffset += _x;
    float xSquared = xOffset * xOffset;
    float ret = xSquared * 1.f / 24.f;
    float correction = ret * xSquared * .02f / .254f;
    ret += -.5f;
    ret *= xSquared;
    ret += 1.f;

    ret -= correction;
    return xneg ? -ret : ret;
}

// https://gist.github.com/bitonic/d0f5a0a44e37d4f0be03d34d47acb6cf#file-vectorized-atan2f-cpp-L131
// which itself references:
// sheet 11,  "Approximations for digital computers", C. Hastings, 1955
inline float atanf(float x) {
    constexpr float a1 = 0.99997726f;
    constexpr float a3 = -0.33262347f;
    constexpr float a5 = 0.19354346f;
    constexpr float a7 = -0.11643287f;
    constexpr float a9 = 0.05265332f;
    constexpr float a11 = -0.01172120f;

    float x_sq = x * x;
    return x * (a1 + x_sq * (a3 + x_sq * (a5 + x_sq * (a7 + x_sq * (a9 + x_sq * a11)))));
}

inline float atan2f(float x, float y) {
    bool swap = std::abs(x) < std::abs(y);
    float atan_input = swap ? x / y : y / x;

    // Approximate atan
    float res = atanf(atan_input);

    // If swapped, adjust atan output
    res = swap ? (atan_input >= 0.0f ? kTwoPi : -kTwoPi) - res : res;
    // Adjust the result depending on the input quadrant
    if (x < 0.0f) {
        res = (y >= 0.0f ? kPi : -kPi) + res;
    }

    return res;
}
}  // namespace approx
}  // namespace kitdsp
