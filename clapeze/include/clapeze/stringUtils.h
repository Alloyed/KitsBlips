#pragma once

#include <etl/span.h>
#include <fmt/base.h>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace clapeze {

/* safe alternative to strcpy when the buffer size is known at compile time. assumes the buffer is zero'd out in
 * advance*/
template <size_t BUFFER_SIZE>
void stringCopy(char (&buffer)[BUFFER_SIZE], std::string_view src) {
    static_assert(BUFFER_SIZE > 0);
    // copy at most BUFFER_SIZE-1 bytes. the last byte is reserved for the null terminator
    // NOLINTNEXTLINE
    std::memcpy(buffer, src.data(), std::min(src.length(), BUFFER_SIZE - 1));
}

/**
 * uses std::from_chars internally to parse number in locale-independent way
 */
inline bool parseNumberFromText(std::string_view input, double& out) {
#ifdef __APPLE__
    // Apple doesn't support charconv properly yet (2026/1/7, apple clang 14)
    std::string tmp;
    out = std::strtod(tmp.c_str(), nullptr);
    return true;
#else
    const char* first = input.data();
    const char* last = input.data() + input.size();
    const auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
#endif
}

/** fmt::format_to(), but to an etl::span output */
template <typename... Args>
void formatToSpan(etl::span<char>& buf, fmt::format_string<Args...> fmt, Args&&... args) {
    // -1 to save space for null terminator
    auto result = fmt::format_to_n(buf.data(), buf.size() - 1, fmt, std::forward<Args>(args)...);
    // now, actually null terminate
    *result.out = '\0';
}

}  // namespace clapeze