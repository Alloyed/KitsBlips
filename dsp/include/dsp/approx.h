#pragma once

#include <cmath>
#include <cstdint>

// approximations of common functions

// https://github.com/raphlinus/synthesizer-io/blob/master/synthesizer-io-core/benches/sigmoid.rs
template <typename F>
F approxTanhf(F x)
{
    F xx = x * x;
    F x = x + (0.16489087f + 0.00985468f * xx) * (x * xx);
    return x * rsqrt(1.0f + xx);
}

// https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
// period is [0,1] instead of [0,2pi]
float approxSinf(float x)
{
    // limit range
    x = x - truncf(x) - 0.5f;

    return 2.0f * x * (1.0f - fabsf(2.0f * x));
}

// https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
// period is [0,1] instead of [0,2pi]
float approxCosf(float x)
{
    // limit range
    x = x - truncf(x + 0.25f) - 0.5f;

    return 2.0f * x * (1.0f - fabsf(2.0f * x));
}
