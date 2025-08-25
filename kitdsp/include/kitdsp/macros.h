#pragma once

#include <utility>

// so long as C++11 is a valid target, we'll need to hide uses of more modern features behind macros.

#define KITDSP_HAS_CPP14 (__cplusplus > 201103L)
#define KITDSP_HAS_CPP20 (__cplusplus > 201703L)

// consteval requires a function to be used at compile time
#if KITDSP_HAS_CPP20
#define KITDSP_CONSTEVAL consteval
#elif KITDSP_HAS_CPP14
#define KITDSP_CONSTEVAL constexpr
#else
#define KITDSP_CONSTEVAL
#endif

// constexpr allows an expression to be used at compile time. C++11 technically supports constexpr but I'd rather assume
// the C++14 rules.
#if KITDSP_HAS_CPP14
#define KITDSP_CONSTEXPR constexpr
#else
#define KITDSP_CONSTEXPR
#endif

#if KITDSP_HAS_CPP20
#define KITDSP_UNUSED [[maybe_unused]]
#else
#define KITDSP_UNUSED
#endif

#if defined(_MSC_VER)
#define KITDSP_DLLEXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define KITDSP_DLLEXPORT __attribute__((visibility("default")))
#else
#define KITDSP_DLLEXPORT
#endif

#if defined(__clang__)
#define KITDSP_SUPPRESS(x) [[gsl::suppress(#x)]]
#else
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) && !defined(__NVCC__)
#define KITDSP_SUPPRESS(x) [[gsl::suppress(x)]]
#else
#define KITDSP_SUPPRESS(x)
#endif  // _MSC_VER
#endif  // __clang__

// narrow_cast(): a searchable way to do narrowing casts of values
template <class T, class U>
// clang-format off
KITDSP_SUPPRESS(type.1) // NO-FORMAT: attribute
                         // clang-format on
    constexpr T narrow_cast(U&& u) noexcept {
    return static_cast<T>(std::forward<U>(u));
}
