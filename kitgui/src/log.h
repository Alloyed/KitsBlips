#pragma once

#include <Corrade/Containers/String.h>

#include <fmt/format.h>
#include <chrono>
#include <functional>
#include <iostream>
#include <ratio>
#include <source_location>
#include <string_view>
#include "kitgui/context.h"

namespace kitgui::log {
using Logger = std::function<void(std::string_view message)>;
inline void defaultLogger(std::string_view message) {
    std::cout << message << '\n';
    std::cout.flush();
}
inline void rawlog(const Context& ctx, std::string_view message, std::source_location loc = std::source_location::current()) {
    ctx.GetLogger()(fmt::format("[KITGUI]({}:{}): {}", loc.file_name(), loc.line(), message));
}
inline void info(const Context& ctx, std::string_view message, std::source_location loc = std::source_location::current()) {
    rawlog(ctx, message, loc);
}
inline void error(const Context& ctx, std::string_view message, std::source_location loc = std::source_location::current()) {
    rawlog(ctx, message, loc);
}
inline void verbose(const Context& ctx, std::string_view message, std::source_location loc = std::source_location::current()) {
    rawlog(ctx, message, loc);
}

class TimeRegion {
   public:
    explicit TimeRegion(const Context& ctx, std::string_view region, std::source_location loc = std::source_location::current())
        : ctx(ctx), mRegionName(region), mLoc(loc), mStartTime(std::chrono::high_resolution_clock::now()) {}
    ~TimeRegion() {
        auto endTime = std::chrono::high_resolution_clock::now();
        double timeSpentMs = std::chrono::duration<double, std::milli>(endTime - mStartTime).count();
        kitgui::log::verbose(ctx, fmt::format("{} completed: {} ms", mRegionName, timeSpentMs), mLoc);
    }
    TimeRegion(const TimeRegion&) = delete;
    TimeRegion(TimeRegion&&) = delete;
    TimeRegion& operator=(const TimeRegion&) = delete;
    TimeRegion& operator=(TimeRegion&&) = delete;

   private:
    const Context& ctx;
    std::string mRegionName;
    std::source_location mLoc;
    std::chrono::high_resolution_clock::time_point mStartTime;
};
}  // namespace kitgui::log

template <>
struct fmt::formatter<Corrade::Containers::String> : fmt::formatter<std::string_view> {
    auto format(const Corrade::Containers::String& obj, fmt::format_context& ctx) const {
        return fmt::formatter<std::string_view>::format({obj.data(), obj.size()}, ctx);
    }
};