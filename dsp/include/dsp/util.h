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

inline float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
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