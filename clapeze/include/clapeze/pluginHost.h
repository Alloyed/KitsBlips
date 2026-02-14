#pragma once

#include <clap/clap.h>
#include <functional>
#include <string>
#include <utility>
#include "clapeze/impl/casts.h"
#include "fmt/base.h"
#include "fmt/format.h"

namespace clapeze {

class TimerSupportFeature;
enum class LogSeverity : uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4,
};

/**
 * PluginHost wraps over the capabilities and methods that the plugin host
 * (aka, the DAW) provide to us. this can be held onto for the lifetime of the
 * plugin safely, but only call it from the main thread.
 */
class PluginHost {
    friend TimerSupportFeature;

   public:
    using TimerId = clap_id;
    using TimerFn = std::function<void()>;
    using LogFn = std::function<void(LogSeverity severity, const std::string& message)>;

    explicit PluginHost(const clap_host_t* host);

    template <typename TExtension>
    bool TryGetExtension(const char* extensionName, const clap_host_t*& hostOut, const TExtension*& extOut) const;
    bool HostSupportsExtension(const char* extensionName) const;
    static const void* TryGetPluginExtension(const char* name);

    void LogSupportMatrix() const;

    /** Returns a user-friendly display string for the host's name */
    std::string_view GetName() const;
    /** Returns an arbitrary version number representing the host. may or may not be semver, up to the host! */
    std::string_view GetVersion() const;

    /** Returns true if we are currently on the main thread */
    bool IsMainThread() const;
    /** Returns true if we are currently on the audio thread */
    bool IsAudioThread() const;

    /** Add a message to the host's log */
    void Log(LogSeverity severity, const std::string& message) const;
    /** Add a message to the host's log */
    template <typename... Args>
    void LogFmt(LogSeverity severity, fmt::format_string<Args...> fmt, Args&&... args) const {
        Log(severity, fmt::format(fmt, std::forward<Args>(args)...));
    }
    /** Add a callback to be run every time Log() is called. use for log visualization */
    void SetLogFn(LogFn fn);

    /** requests that the host restarts the plugin next chance it gets. */
    void RequestRestart() const;

    /** requests that the host activates the processing thread the next chance it gets. */
    void RequestProcess() const;

    /** requests that the host calls Plugin.OnMainThread() next chance it gets. */
    void RequestCallback() const;

    /** Add a timer to be run every `periodMs` milliseconds. to run only once, call `CancelTimer()` in the body of your
     * timer function. */
    TimerId AddTimer(uint32_t periodMs, TimerFn timerFn);
    /** Cancels an active timer with the provided id. this is safe to call from inside of timers. */
    void CancelTimer(TimerId id);

   private:
    void OnTimer(TimerId id) const;
    static void _on_timer(const clap_plugin_t* plugin, clap_id timerId);
    const clap_host_t* mHost;
    const clap_host_thread_check_t* mThreadCheck;
    const clap_host_log_t* mLog;
    const clap_host_timer_support_t* mTimer;
    std::unordered_map<TimerId, TimerFn> mActiveTimers{};
    LogFn mLogFn{};
};

template <typename THostExtension>
bool PluginHost::TryGetExtension(const char* extensionName,
                                 const clap_host_t*& hostOut,
                                 const THostExtension*& extOut) const {
    extOut = impl::userdata_cast<const THostExtension*>(mHost->get_extension(mHost, extensionName));
    if (extOut) {
        hostOut = mHost;
        return true;
    } else {
        return false;
    }
}
}  // namespace clapeze