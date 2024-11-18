#pragma once

#include <cmath>
#include <cstdint>

// approximations of common functions
namespace kitdsp
{
    namespace approx
    {
        // you know what this is
        inline float rsqrtf(float number)
        {
            const float threehalfs = 1.5f;
            float x2 = number * 0.5f;

            // evil floating point bit level hacking
            union
            {
                float f;
                uint32_t i;
            } y;

            y.f = number;
            y.i = 0x5f3759df - (y.i >> 1);               // what the fuck?
            y.f = y.f * (threehalfs - (x2 * y.f * y.f)); // 1st iteration
            // y.f  = y.f * ( threehalfs - ( x2 * y.f * y.f ) );   // 2nd iteration, this can be removed

            return y.f;
        }

        // https://github.com/raphlinus/synthesizer-io/blob/master/synthesizer-io-core/benches/sigmoid.rs
        inline float tanhf(float x)
        {
            float xx = x * x;
            float x1 = x + (0.16489087f + 0.00985468f * xx) * (x * xx);
            return x1 * rsqrtf(1.0f + x1 * x1);
        }

        // https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
        // period is [0,1] instead of [0,2pi]
        inline float sin2pif_nasty(float x)
        {
            // limit range
            x = x - truncf(x) - 0.5f;

            return 2.0f * x * (1.0f - fabsf(2.0f * x));
        }

        // https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
        // period is [0,1] instead of [0,2pi]
        inline float cos2pif_nasty(float x)
        {
            // limit range
            x = x - truncf(x + 0.25f) - 0.5f;

            return 2.0f * x * (1.0f - fabsf(2.0f * x));
        }

        /*
         * adapted from https://github.com/squinkylabs/Demo/blob/main/src/VCO3.cpp
         * input: _x must be >= 0, and <= 2 * pi.
         */
        inline float sinf_squinky(float _x)
        {
            constexpr static float twoPi = 2 * 3.141592653589793238;
            constexpr static float pi = 3.141592653589793238;
            _x -= (_x > pi) ? twoPi : 0.0f;

            float xneg = _x < 0;
            float xOffset = xneg ? pi / 2.f : -pi / 2.f;
            xOffset += _x;
            float xSquared = xOffset * xOffset;
            float ret = xSquared * 1.f / 24.f;
            float correction = ret * xSquared * .02 / .254;
            ret += -.5;
            ret *= xSquared;
            ret += 1.f;

            ret -= correction;
            return xneg ? -ret : ret;
        }
    }
}
