#pragma once

#include <cstdint>
#include "kitdsp/math/util.h"

namespace kitdsp {
namespace hash_impl {
inline uint64_t splittable64(uint64_t x) {
    // Published by Sebastiano Vigna (vigna@acm.org)
    // https://xoshiro.di.unimi.it/splitmix64.c
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9U;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebU;
    x ^= x >> 31;
    return x;
}

inline uint32_t lowbias32(uint32_t x) {
    // Published by Chris Wellons
    // https://nullprogram.com/blog/2018/07/31/
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
}  // namespace hash_impl

/** With arbitrary input, returns a float between 0-1, inclusive. this is handy for fake RNG :p */
inline float hash(uint32_t in) {
    return static_cast<float>(hash_impl::lowbias32(in)) / static_cast<float>(UINT32_MAX);
}
inline float hash(int32_t in) {
    return static_cast<float>(hash_impl::lowbias32(bit_cast<uint32_t>(in))) / static_cast<float>(UINT32_MAX);
}
inline float hash(float in) {
    return static_cast<float>(hash_impl::lowbias32(bit_cast<uint32_t>(in))) / static_cast<float>(UINT32_MAX);
}
inline float hash(uint64_t in) {
    return static_cast<float>(hash_impl::splittable64(in)) / static_cast<float>(UINT64_MAX);
}
inline float hash(int64_t in) {
    return static_cast<float>(hash_impl::splittable64(bit_cast<uint64_t>(in))) / static_cast<float>(UINT64_MAX);
}
inline float hash(double in) {
    return static_cast<float>(hash_impl::splittable64(bit_cast<uint64_t>(in))) / static_cast<float>(UINT64_MAX);
}
}  // namespace kitdsp