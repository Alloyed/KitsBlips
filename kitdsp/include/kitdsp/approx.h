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
            const float threehalfs = 1.5F;
            float x2 = number * 0.5F;
            
            // evil floating point bit level hacking
            union {
                float f;
                int32_t i;
            } y;

            y.f = number;
            y.i = 0x5f3759df - (y.i >> 1); // what the fuck?
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
        inline float sin2pif(float x)
        {
            // limit range
            x = x - truncf(x) - 0.5f;

            return 2.0f * x * (1.0f - fabsf(2.0f * x));
        }

        // https://bmtechjournal.wordpress.com/2020/05/27/super-fast-quadratic-sinusoid-approximation/
        // period is [0,1] instead of [0,2pi]
        inline float cos2pif(float x)
        {
            // limit range
            x = x - truncf(x + 0.25f) - 0.5f;

            return 2.0f * x * (1.0f - fabsf(2.0f * x));
        }

        // below: https://github.com/VCVRack/Rack/blob/v2/include/dsp/approx.hpp
        namespace rack
        {

            // https://github.com/VCVRack/Fundamental/blob/v2/src/VCO.cpp#L7
            //  period is [0,1] instead of [0,2pi]
            inline float sin2pi_pade_05_5_4(float x)
            {
                x -= 0.5f;
                float num = (-6.283185307f * x + 33.19863968f * powf(x, 3) - 32.44191367 * powf(x, 5));
                float denom = (1 + 1.296008659 * powf(x, 2) + 0.7028072946 * powf(x, 4));
                return num / denom;
            }

            /** Evaluates a polynomial with coefficients `a[n]` at `x`.
            Uses Horner's method.
            https://en.wikipedia.org/wiki/Horner%27s_method
            */
            template <typename T, size_t N>
            T polyHorner(const T (&a)[N], T x)
            {
                if (N == 0)
                    return 0;

                T y = a[N - 1];
                for (size_t n = 1; n < N; n++)
                {
                    y = a[N - 1 - n] + y * x;
                }
                return y;
            }

            /**
             * Returns `2^floor(x)`.
             * If xf is given, sets it to the fractional part of x.
             * This is useful in the computation `2^x = 2^floor(x) * 2^frac(x)`.
             */
            inline float exp2Floor(float x, float *xf)
            {
                x += 127;
                // x should be positive now, so this always truncates towards -inf.
                int32_t xi = x;
                if (xf)
                    *xf = x - xi;
                // Set mantissa of float
                union
                {
                    float yi;
                    int32_t yii;
                };
                yii = xi << 23;
                return yi;
            }

            /**
             * Returns 2^x with at most 6e-06 relative error.
             * Polynomial coefficients are chosen to minimize relative error while maintaining continuity and giving
             * exact values at integer values of `x`. Thanks to Andy Simper for coefficients.
             */
            float exp2_taylor5(float x)
            {
                float xf;
                float yi = exp2Floor(x, &xf);

                const float a[] = {
                    1.0,
                    0.69315169353961,
                    0.2401595990753,
                    0.055817908652,
                    0.008991698010,
                    0.001879100722,
                };
                float yf = polyHorner(a, xf);
                return yi * yf;
            }
        }
    }
}
