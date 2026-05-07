#if (defined(_M_AMD64) || defined(_M_X64) || defined(__amd64)) && !defined(__x86_64__)

#ifdef __GNUC__
#include <x86intrin.h>
#else
#include <immintrin.h>
#endif  // __GNUC__

template <typename TYPE, size_t SIZE>
struct Vector;

/*
    SIMD specialization: float2
    Uses the low 2 lanes of an __m128 register.
*/
template <>
struct alignas(16) Vector<float, 2> {
    using type = float;
    static constexpr size_t size = 2;

#ifdef __GNUC__
    __extension__
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)
#endif
        union {
        __m128 simd;

        float data[4];

        struct {
            float left;
            float right;
        };

        struct {
            float l;
            float r;
        };

        struct {
            float x;
            float y;
        };
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    Vector() : simd(_mm_setzero_ps()) {}

    Vector(float left, float right) : simd(_mm_setr_ps(left, right, 0.0f, 0.0f)) {}

    explicit Vector(float v) : simd(_mm_set1_ps(v)) {}

    explicit Vector(__m128 v) : simd(v) {}

    Vector operator+(const Vector& other) const { return Vector(_mm_add_ps(simd, other.simd)); }

    Vector operator+(float other) const { return Vector(_mm_add_ps(simd, _mm_set1_ps(other))); }

    Vector operator-(const Vector& other) const { return Vector(_mm_sub_ps(simd, other.simd)); }

    Vector operator-(float other) const { return Vector(_mm_sub_ps(simd, _mm_set1_ps(other))); }

    Vector operator*(const Vector& other) const { return Vector(_mm_mul_ps(simd, other.simd)); }

    Vector operator*(float other) const { return Vector(_mm_mul_ps(simd, _mm_set1_ps(other))); }

    Vector operator/(const Vector& other) const { return Vector(_mm_div_ps(simd, other.simd)); }

    Vector operator/(float other) const { return Vector(_mm_div_ps(simd, _mm_set1_ps(other))); }
};

/*
    SIMD specialization: float4
    Fully utilizes all 4 lanes of an __m128 register.
*/
template <>
struct alignas(16) Vector<float, 4> {
    using type = float;
    static constexpr size_t size = 4;

#ifdef __GNUC__
    __extension__
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)
#endif
        union {
        __m128 simd;

        float data[4];

        struct {
            float a;
            float b;
            float c;
            float d;
        };

        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    Vector() : simd(_mm_setzero_ps()) {}

    Vector(float a, float b, float c, float d) : simd(_mm_setr_ps(a, b, c, d)) {}

    explicit Vector(float v) : simd(_mm_set1_ps(v)) {}

    explicit Vector(__m128 v) : simd(v) {}

    Vector operator+(const Vector& other) const { return Vector(_mm_add_ps(simd, other.simd)); }

    Vector operator+(float other) const { return Vector(_mm_add_ps(simd, _mm_set1_ps(other))); }

    Vector operator-(const Vector& other) const { return Vector(_mm_sub_ps(simd, other.simd)); }

    Vector operator-(float other) const { return Vector(_mm_sub_ps(simd, _mm_set1_ps(other))); }

    Vector operator*(const Vector& other) const { return Vector(_mm_mul_ps(simd, other.simd)); }

    Vector operator*(float other) const { return Vector(_mm_mul_ps(simd, _mm_set1_ps(other))); }

    Vector operator/(const Vector& other) const { return Vector(_mm_div_ps(simd, other.simd)); }

    Vector operator/(float other) const { return Vector(_mm_div_ps(simd, _mm_set1_ps(other))); }
};
#endif