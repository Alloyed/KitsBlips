#pragma once

#include <clap/clap.h>
#include <functional>
#include <string>

namespace clapeze {

class TimerSupportExt;
enum class LogSeverity {
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
    friend TimerSupportExt;

   public:
    using TimerId = clap_id;
    using TimerFn = std::function<void()>;

    PluginHost(const clap_host_t* host);

    template <typename ExtType>
    bool TryGetExtension(const char* extensionName, const clap_host_t*& hostOut, const ExtType*& extOut) const;
    bool SupportsExtension(const char* extensionName) const;

    void LogSupportMatrix() const;

    /** Returns true if we are currently on the main thread */
    bool IsMainThread() const;
    /** Returns true if we are currently on the audio thread */
    bool IsAudioThread() const;

    /** Add a message to the host's log */
    void Log(LogSeverity severity, const std::string& message) const;

    /** requests that the host calls Plugin.OnMainThread() next chance it gets. */
    void RequestCallback() const;

    /** Add a timer to be run every `periodMs` milliseconds. to run only once, call `CancelTimer()` in the body of your
     * timer function. */
    TimerId AddTimer(uint32_t periodMs, TimerFn timerFn);
    /** Cancels an active timer with the provided id. this is safe to call from inside of timers. */
    void CancelTimer(TimerId id);

   private:
    void OnTimer(TimerId id) const;
    const clap_host_t* mHost;
    const clap_host_thread_check_t* mThreadCheck;
    const clap_host_log_t* mLog;
    const clap_host_timer_support_t* mTimer;
    const clap_host_gui_t* mGui;
    std::unordered_map<TimerId, TimerFn> mActiveTimers;
};

template <typename ExtType>
bool PluginHost::TryGetExtension(const char* extensionName, const clap_host_t*& hostOut, const ExtType*& extOut) const {
    extOut = static_cast<const ExtType*>(mHost->get_extension(mHost, extensionName));
    if (extOut) {
        hostOut = mHost;
        return true;
    } else {
        return false;
    }
}
}  // namespace clapeze