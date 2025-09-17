#pragma once
#include <Corrade/Containers/String.h>
#include <fmt/format.h>
#include <iostream>
#include <source_location>
#include <string_view>

namespace kitgui::log {
inline void rawlog(std::string_view message, std::source_location loc = std::source_location::current()) {
    std::cout << "[KITGUI](" << loc.file_name() << ":" << loc.line() << " " << loc.function_name() << "): " << message
              << '\n';
}
inline void info(std::string_view message, std::source_location loc = std::source_location::current()) {
    rawlog(message, loc);
}
inline void error(std::string_view message, std::source_location loc = std::source_location::current()) {
    rawlog(message, loc);
}
inline void verbose(std::string_view message, std::source_location loc = std::source_location::current()) {
    rawlog(message, loc);
}
}  // namespace kitgui::log

template <>
struct fmt::formatter<Corrade::Containers::String> : fmt::formatter<std::string_view> {
    auto format(const Corrade::Containers::String& obj, fmt::format_context& ctx) const {
        return fmt::formatter<std::string_view>::format({obj.data(), obj.size()}, ctx);
    }
};