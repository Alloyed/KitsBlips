#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include "kitdsp/macros.h"

namespace kitdsp {
constexpr float kTwoPi = 6.28318530718f;
constexpr float kPi = 3.14159265359f;
constexpr float kHalfPi = 1.57079632679f;

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
F lerp(F x0, F x1, float t) {
    return x0 + (x1 - x0) * t;
}

template <typename F>
F fade(F a, F b, float t) {
    // this looks like lerp, but it's designed to more precisely replicate the
    // input a/b signals. it does not behave monotonically outside of the 0-1
    // range, which may be annoying in some cases.
    float tInv = 1.0f - t;
    return (a * tInv) + (b * t);
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
    // -1 so that the result for exact powers of 2 stays the same
    x--;
    // this effectively turns on every bit below the max (eg. 0100 -> 0111)
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    // now, when we add 1, each bit carries until the next highest bit is the only one on (eg. 0111 -> 1000)
    x++;

    return x;
}

inline KITDSP_CONSTEXPR uint32_t ceilToPowerOf2(float f) {
    return ceilToPowerOf2(static_cast<uint32_t>(ceilf(f)));
}

inline float blockNanf(float in, float valueIfNan = 0.0f) {
    return std::isfinite(in) ? in : valueIfNan;
}

template <class T2, class T1>
KITDSP_CONSTEXPR T2 bit_cast(T1 t1) {
    static_assert(sizeof(T1) == sizeof(T2), "Types must match sizes");
    static_assert(std::is_pod<T1>::value, "Requires POD input");
    static_assert(std::is_pod<T2>::value, "Requires POD output");

    T2 t2;
    std::memcpy(std::addressof(t2), std::addressof(t1), sizeof(T1));
    return t2;
}
}  // namespace kitdsp

#include "kitdsp/math/util_arm.inl"