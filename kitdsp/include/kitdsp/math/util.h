#pragma once

#include <cmath>
#include <cstdint>
#include "kitdsp/macros.h"

namespace kitdsp {
constexpr float kTwoPi = 6.28318530718;
constexpr float kPi = 3.14159265359;
constexpr float kHalfPi = 1.57079632679;

template <typename F>
inline F min(F a, F b) {
    return a < b ? a : b;
}

template <typename F>
inline F max(F a, F b) {
    return a > b ? a : b;
}

template <typename F>
inline F clamp(F in, F min, F max) {
    return in > max ? max : in < min ? min : in;
}

template <typename F>
F fade(F a, F b, float t) {
    // this looks like lerp, but it's designed to more precisely replicate the
    // input a/b signals. it does not behave monotonically outside of the 0-1
    // range, which may be annoying in some cases.
    return a * (F(1) - t) + b * t;
}

inline float fadeCpowf(float a, float b, float t) {
    float scalar_1 = sinf(t * kHalfPi);
    float scalar_2 = sinf((1.0f - t) * kHalfPi);
    return (a * scalar_2) + (b * scalar_1);
}

inline float roundTo(float in, float increment) {
    return in - std::remainder(in, increment);
}

inline KITDSP_CONSTEXPR uint32_t ceilToPowerOf2(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

inline KITDSP_CONSTEXPR uint32_t ceilToPowerOf2(float f) {
    return ceilToPowerOf2(static_cast<uint32_t>(ceilf(f)));
}

template <typename F>
F lerpf(F x0, F x1, float t) {
    return x0 + (x1 - x0) * t;
}

inline float blockNanf(float in, float valueIfNan = 0.0f) {
    return std::isfinite(in) ? in : valueIfNan;
}

/**
 * turns a midi note number (fractional allowed) into a frequence in standard A4=440 western tuning.
 * for ref: midi note 69 is A4, 48 is C3
 */
inline float midiToFrequency(float midiNote) {
    return exp2((midiNote - 69.0f) / 12.0f) * 440.0f;
}
}  // namespace kitdsp

#include "kitdsp/math/util_arm.inl"