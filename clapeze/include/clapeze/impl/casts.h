#pragma once

#include <cassert>
#include <type_traits>
#include <utility>

namespace clapeze::impl {
/* use to signal that you intend to narrow the size of a given input */
template <class T, class U>
    requires std::is_arithmetic_v<T> && std::is_arithmetic_v<U>
constexpr T narrow_cast(U u) noexcept {
    return static_cast<T>(u);
}

/* use when turning a void* userdata type into its actual class */
template <class T, class U>
    requires std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<U>, void>
constexpr T userdata_cast(U* u) noexcept {
    return static_cast<T>(u);
}

/* use to signal that your input is a base class, and you intend to use it as derived class */
template <class T, class U>
    requires std::is_pointer_v<T> || std::is_reference_v<T>
constexpr T down_cast(U&& u) noexcept {
    if constexpr (std::is_pointer_v<T>) {
        assert(dynamic_cast<T>(u) != nullptr);
        return static_cast<T>(std::forward<U>(u));
    } else {
        assert(dynamic_cast<std::remove_reference_t<T>*>(&u) != nullptr);
        return static_cast<T>(std::forward<U>(u));
    }
}

/* Convert an enum into its integral type*/
template <class T, class U>
    requires std::is_enum_v<U> && std::is_convertible_v<std::underlying_type_t<U>, T>
constexpr T integral_cast(U&& u) noexcept {
    return static_cast<T>(u);
}
}  // namespace clapeze::impl