/*
 * These types use "ARM" format q-notation.
 *
 * https://en.wikipedia.org/wiki/Q_(number_format)
 */

#pragma once

#include <cstdint>
#include "kitdsp/math/util.h"

namespace kitdsp {
class UQ16;
class Q1_15;
/** a number with 1 bit for sign, and 15 bits for value. range [-1, 1]*/
class Q1_15 {
   public:
    Q1_15(int16_t x) : mData(x) {}  // NOLINT
    int16_t raw() const { return mData; }

    float toFloat() const { return static_cast<float>(mData) / INT16_MAX; }
    UQ16 toUQ16() const;

    Q1_15 operator+(const Q1_15 other) const { return static_cast<int16_t>(mData + other.mData); }

    Q1_15 operator-(const Q1_15 other) const { return static_cast<int16_t>(mData - other.mData); }

    Q1_15 operator*(const Q1_15 other) const {
        constexpr int16_t Q = 15;
        constexpr int16_t K = (1 << (Q - 1));

        int32_t temp = static_cast<int32_t>(mData) * static_cast<int32_t>(other.mData);
        // Rounding; mid values are rounded up
        temp += K;
        // Correct by dividing by base and saturate result
        return {static_cast<int16_t>(kitdsp::clamp(temp >> Q, INT16_MIN, INT16_MAX))};
    }

    Q1_15 operator/(const Q1_15 other) const {
        constexpr int16_t Q = 15;
        /* pre-multiply by the base (Upscale to Q16 so that the result will be in Q8 format) */
        int32_t temp = static_cast<int32_t>(mData) << Q;
        /* Rounding: mid values are rounded up (down for negative values). */
        if (std::signbit(temp) == std::signbit(other.mData)) {
            temp += other.mData >> 1;
        } else {
            temp -= other.mData >> 1;
        }
        return {static_cast<int16_t>(temp / other.mData)};
    }

   private:
    int16_t mData;
};

/** a number with 16 bits for value. range [0, 1]*/
class UQ16 {
   public:
    UQ16(uint16_t x) : mData(x) {}  // NOLINT
    uint16_t raw() const { return mData; }
    float toFloat() const { return static_cast<float>(mData) / UINT16_MAX; }
    Q1_15 toQ1_15() const;

    UQ16 operator+(const UQ16 other) const { return mData + other.mData; }

    UQ16 operator-(const UQ16 other) const { return mData - other.mData; }

    UQ16 operator*(const UQ16 other) const {
        constexpr int16_t Q = 16;
        constexpr int16_t K = static_cast<int16_t>(1 << (Q - 1));

        int32_t temp = static_cast<int32_t>(mData) * static_cast<int32_t>(other.mData);
        // Rounding; mid values are rounded up
        temp += K;
        // Correct by dividing by base and saturate result
        return {static_cast<uint16_t>(kitdsp::clamp(temp >> Q, 0, UINT16_MAX))};
    }

    UQ16 operator/(const UQ16 other) const {
        constexpr int16_t Q = 16;
        /* pre-multiply by the base (Upscale to Q16 so that the result will be in Q8 format) */
        int32_t temp = static_cast<int32_t>(mData) << Q;
        /* Rounding: mid values are rounded up (down for negative values). */
        temp += other.mData >> 1;
        return {static_cast<uint16_t>(temp / other.mData)};
    }

   private:
    uint16_t mData;
};

inline Q1_15 UQ16::toQ1_15() const {
    return {static_cast<int16_t>(mData >> 1)};
}
inline UQ16 Q1_15::toUQ16() const {
    return {static_cast<uint16_t>(kitdsp::clamp<int16_t>(mData, 0, INT16_MAX) << 1)};
}

}  // namespace kitdsp