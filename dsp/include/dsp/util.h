#pragma once

#include <cmath>
#include <cstdint>

constexpr float cTwoPi = 6.28318530718;
constexpr float cPi = 3.14159265359;
constexpr float cHalfPi = 1.57079632679;

inline float clampf(float in, float min, float max)
{
    return in > max ? max : in < min ? min
                                     : in;
}

inline float fadeCpowf(float a, float b, float t)
{
    float scalar_1 = sinf(t * cHalfPi);
    float scalar_2 = sinf((1.0f - t) * cHalfPi);
    return (a * scalar_2) + (b * scalar_1);
}

inline float roundTof(float in, float increment)
{
    return in - remainderf(in, increment);
}

inline float floorf(float in, float increment)
{
    return static_cast<int32_t>(in * increment) / increment;
}

inline float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline float lerpCubicf(float x0, float x1, float x2, float x3, float t)
{
    float a0, a1, a2, a3;
    a0 = x3 - x2 - x0 + x1;
    a1 = x0 - x1 - a0;
    a2 = x2 - x0;
    a3 = x1;
    return (a0 * (t * t * t)) + (a1 * (t * t)) + (a2 * t) + (a3);
}

inline float lerpHermite4pt3oXf(float x0, float x1, float x2, float x3, float t)
{
    float c0 = x1;
    float c1 = .5f * (x2 - x0);
    float c2 = x0 - (2.5f * x1) + (2.0f * x2) - (.5f * x3);
    float c3 = (.5f * (x3 - x0)) + (1.5f * (x1 - x2));
    return (((((c3 * t) + c2) * t) + c1) * t) + c0;
}