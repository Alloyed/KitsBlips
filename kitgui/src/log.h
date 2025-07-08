#pragma once
#include <Corrade/Containers/String.h>
#include <SDL3/SDL.h>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

namespace kitgui::log {
inline void rawlog(std::string_view message, std::source_location loc = std::source_location::current()) {
    std::cout << "[KITGUI](" << loc.file_name() << ":" << loc.line() << " " << loc.function_name() << "): " << message
              << std::endl;
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

#define LOG_SDL_ERROR() kitgui::log::error(SDL_GetError())

template <>
struct std::formatter<Corrade::Containers::String> : std::formatter<std::string_view> {
    auto format(const Corrade::Containers::String& obj, std::format_context& ctx) const {
        return std::formatter<std::string_view>::format({obj.data(), obj.size()}, ctx);
    }
};