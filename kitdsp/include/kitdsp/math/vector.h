#pragma once

#include <cstddef>
#include <cstdint>

namespace kitdsp {

template <typename TYPE, size_t SIZE>
struct Vector;

/** This is a math vector type and not a C++ vector type. */
template <typename TYPE>
struct Vector<TYPE, 2> {
    using type = TYPE;
    static constexpr size_t size = 2;

// We're using the nonstandard "anonymous union" extension, which is supported in all the compilers we care about anways (gcc, clang, msvc).
// this lets us do intentional "field aliases", so v.data[0] is the same as v.left is the same as v.x, as if this were a GLSL vector type :)
#ifdef __GNUC__
    __extension__
#endif
#ifdef _MSC_VER
#pragma warning( push )  
#pragma warning( disable : 4201 )
#endif
    union {
        TYPE data[2];
        struct {
            TYPE left;
            TYPE right;
        };
        struct {
            TYPE l;
            TYPE r;
        };
        struct {
            TYPE x;
            TYPE y;
        };
    };
#ifdef _MSC_VER
#pragma warning( pop )  
#endif

    Vector(): left(0), right(0) { }

    Vector(type left, type right): left(left), right(right) { }

    explicit Vector(type in): left(in), right(in) { }

    Vector<type, size> operator+(const Vector<type, size>& other) const {
        return {data[0] + other.data[0], data[1] + other.data[1]};
    }

    Vector<type, size> operator+(const type& other) const { return {data[0] + other, data[1] + other}; }

    Vector<type, size> operator-(const Vector<type, size>& other) const {
        return {data[0] - other.data[0], data[1] - other.data[1]};
    }

    Vector<type, size> operator-(const type& other) const { return {data[0] - other, data[1] - other}; }

    Vector<type, size> operator*(const Vector<type, size>& other) const {
        return {data[0] * other.data[0], data[1] * other.data[1]};
    }

    Vector<type, size> operator*(const type& other) const { return {data[0] * other, data[1] * other}; }

    Vector<type, size> operator/(const Vector<type, size>& other) const {
        return {data[0] / other.data[0], data[1] / other.data[1]};
    }

    Vector<type, size> operator/(const type& other) const { return {data[0] / other, data[1] / other}; }
};

using float_2 = Vector<float, 2>;
using int32_2 = Vector<int32_t, 2>;
using uint32_2 = Vector<uint32_t, 2>;
using size_2 = Vector<size_t, 2>;
using bool_2 = Vector<bool, 2>;

}  // namespace kitdsp
