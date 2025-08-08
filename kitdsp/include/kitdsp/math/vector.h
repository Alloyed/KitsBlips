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
