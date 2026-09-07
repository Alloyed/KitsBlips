#pragma once

#include "kitdsp/math/vector.h"
#include "kitdsp/math/approx.h"

namespace kitdsp {
/**
 * Equal-power pan is also known as as the "-3db law", "true panning", or
 * "circular panning".
 * Use as the default.
 * @example `pannedSignal = rawSignal * equalPowerPan(pan);`
 * @param pan 0 is full left, 1 is full right, 0.5 is center
 */
inline float_2 equalPowerPan(float pan) {
    pan = clamp(pan, 0.0f, 1.0f);
    return {
        approx::cos2pif_nasty(pan*0.25f),
        approx::sin2pif_nasty(pan*0.25f)
    };
}

/**
 * Linear pan is also known as as the "-6db law". useful if you want the
 * panning effect to not have any effect on the summed mono signal, because
 * adding them back up always results in a gain of 1.
 * @example `pannedSignal = rawSignal * linearPan(pan);`
 * @param pan 0 is full left, 1 is full right, 0.5 is center
 */
inline float_2 linearPan(float pan) {
    pan = clamp(pan, 0.0f, 1.0f);
    return {1.0f - pan, pan};
}

/**
 * Compromise pan is also known as as the "-4.5db law". It's in between the
 * two.
 * @example `pannedSignal = rawSignal * compromisePan(pan);`
 * @param pan 0 is full left, 1 is full right, 0.5 is center
 */
inline float_2 compromisePan(float pan) {
    pan = clamp(pan, 0.0f, 1.0f);
    return {
        std::sqrt((1.0f - pan) * approx::cos2pif_nasty(pan*0.25f)),
        std::sqrt(pan          * approx::sin2pif_nasty(pan*0.25f)),
    };
}
}  // namespace kitdsp
