#pragma once

#include <cmath>
#include <cstdint>

namespace kitdsp {
constexpr float cTwoPi = 6.28318530718;
constexpr float cPi = 3.14159265359;
constexpr float cHalfPi = 1.57079632679;

inline float minf(float a, float b) {
    return a < b ? a : b;
}

inline float maxf(float a, float b) {
    return a > b ? a : b;
}

inline float clampf(float in, float min, float max) {
    return in > max ? max : in < min ? min : in;
}

inline float fadeCpowf(float a, float b, float t) {
    float scalar_1 = sinf(t * cHalfPi);
    float scalar_2 = sinf((1.0f - t) * cHalfPi);
    return (a * scalar_2) + (b * scalar_1);
}

inline float roundTof(float in, float increment) {
    return in - remainderf(in, increment);
}

inline float floorf(float in, float increment) {
    return static_cast<int32_t>(in * increment) / increment;
}

inline float floorf(float in) {
    return static_cast<int32_t>(in);
}

inline uint32_t ceilToPowerOf2(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

inline uint32_t ceilToPowerOf2(float f) {
    return ceilToPowerOf2(static_cast<uint32_t>(ceilf(f)));
}

template <typename F>
F lerpf(F a, F b, float t) {
    return a + (b - a) * t;
}

template <typename F>
inline F lerpCubicf(F x0, F x1, F x2, F x3, float t) {
    F a0, a1, a2, a3;
    a0 = x3 - x2 - x0 + x1;
    a1 = x0 - x1 - a0;
    a2 = x2 - x0;
    a3 = x1;
    return (a0 * (t * t * t)) + (a1 * (t * t)) + (a2 * t) + (a3);
}

template <typename F>
inline F lerpHermite4pt3oXf(F x0, F x1, F x2, F x3, float t) {
    F c0 = x1;
    F c1 = (x2 - x0) * 0.5f;
    F c2 = x0 - (x1 * 2.5f) + (x2 * 2.0f) - (x3 * .5f);
    F c3 = ((x3 - x0) * .5f) + ((x1 - x2) * 1.5f);
    return (((((c3 * t) + c2) * t) + c1) * t) + c0;
}

inline float blockNanf(float in, float valueIfNan = 0.0f) {
    return std::isfinite(in) ? in : valueIfNan;
}
}  // namespace kitdsp