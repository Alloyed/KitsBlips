#ifdef __arm__
namespace kitdsp {
template <>
inline float min<float>(float a, float b) {
    float r;
    asm("vminnm.f32 %[d], %[n], %[m]" : [d] "=t"(r) : [n] "t"(a), [m] "t"(b) :);
    return r;
}

template <>
inline float max<float>(float a, float b) {
    float r;
    asm("vmaxnm.f32 %[d], %[n], %[m]" : [d] "=t"(r) : [n] "t"(a), [m] "t"(b) :);
    return r;
}
}  // namespace kitdsp

#endif