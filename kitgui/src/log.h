#pragma once

#include <Corrade/Containers/String.h>

#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <source_location>
#include <string_view>
#include <chrono>
#include <ratio>

namespace kitgui::log {
using Logger = std::function<void(std::string_view message)>;
inline Logger& sLogFn() {
    static Logger logger{[](std::string_view message) { std::cout << message << '\n'; std::cout.flush(); }};
    return logger;
}
inline void rawlog(std::string_view message, std::source_location loc = std::source_location::current()) {
    sLogFn()(fmt::format("[KITGUI]({}:{}): {}", loc.file_name(), loc.line(), message));
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

class TimeRegion {
    public:
    TimeRegion(std::string_view region, std::source_location loc = std::source_location::current()): mRegionName(region), mLoc(loc), mStartTime(std::chrono::high_resolution_clock::now()) {
    }
    ~TimeRegion() {
        auto endTime = std::chrono::high_resolution_clock::now();
        double timeSpentMs = std::chrono::duration<double, std::milli>(endTime - mStartTime).count();
        kitgui::log::verbose(fmt::format("{} completed: {} ms", mRegionName, timeSpentMs), mLoc);
    }
    private:
    std::string mRegionName;
    std::source_location mLoc;
    std::chrono::steady_clock::time_point mStartTime;
};
}  // namespace kitgui::log

template <>
struct fmt::formatter<Corrade::Containers::String> : fmt::formatter<std::string_view> {
    auto format(const Corrade::Containers::String& obj, fmt::format_context& ctx) const {
        return fmt::formatter<std::string_view>::format({obj.data(), obj.size()}, ctx);
    }
};