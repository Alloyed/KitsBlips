#pragma once
#include <cmath>

namespace kitdsp
{
    namespace math
    {
        inline float clamp(float x, float a = 0.f, float b = 1.f)
        {
            return fminf(fmaxf(x, a), b);
        }

        inline float rescale(float x, float xMin, float xMax, float yMin, float yMax)
        {
            return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
        }

        inline float crossfade(float a, float b, float p)
        {
            return a + (b - a) * p;
        }

        template <typename T>
        inline T lerp(T a, T b, T t)
        {
            return crossfade(a, b, t);
        }

        /** Returns 1 for positive numbers, -1 for negative numbers, and 0 for zero.
         *  See https://en.wikipedia.org/wiki/Sign_function.
         */
        template <typename T>
        T sgn(T x)
        {
            return x > 0 ? 1 : (x < 0 ? -1 : 0);
        }
    }
}