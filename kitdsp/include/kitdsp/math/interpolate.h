#pragma once

namespace kitdsp {
namespace interpolate {
enum class InterpolationStrategy { None, Linear, Hermite, Cubic };
/**
 * @param t a 0-1 fractional value that lies between x0 and x1
 */
template <typename SAMPLE>
SAMPLE linear(SAMPLE x0, SAMPLE x1, float t) {
    return x0 + (x1 - x0) * t;
}

/**
 * @param t a 0-1 fractional value that lies between x0 and x1
 */
template <typename SAMPLE>
inline SAMPLE cubic(SAMPLE xm1, SAMPLE x0, SAMPLE x1, SAMPLE x2, float t) {
    // https://dsp.stackexchange.com/questions/70165/a-cubic-interpolation-function-folkloric-copypasta-or-clever-trade-off
    SAMPLE a0 = x2 - x1 - xm1 + x0;
    SAMPLE a1 = xm1 - x0 - a0;
    SAMPLE a2 = x1 - xm1;
    SAMPLE a3 = x0;
    return (a0 * (t * t * t)) + (a1 * (t * t)) + (a2 * t) + (a3);
}

/**
 * @param t a 0-1 fractional value that lies between x0 and x1
 */
template <typename SAMPLE>
inline SAMPLE hermite4pt3oX(SAMPLE xm1, SAMPLE x0, SAMPLE x1, SAMPLE x2, float t) {
    SAMPLE c0 = x0;
    SAMPLE c1 = (x1 - xm1) * 0.5f;
    SAMPLE c2 = xm1 - (x0 * 2.5f) + (x1 * 2.0f) - (x2 * .5f);
    SAMPLE c3 = ((x2 - xm1) * .5f) + ((x0 - x1) * 1.5f);
    return (((((c3 * t) + c2) * t) + c1) * t) + c0;
}
}  // namespace interpolate
}  // namespace kitdsp