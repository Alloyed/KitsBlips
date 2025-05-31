#pragma once

#include <string>
#include <functional>

#include "clap/id.h"

// forward declarations
typedef struct clap_host clap_host_t;
typedef struct clap_host_log clap_host_log_t;
typedef struct clap_host_timer_support clap_host_timer_support_t;
typedef struct clap_host_gui clap_host_gui_t;

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
	public:
		using TimerId = clap_id;
		using TimerFn = std::function<void()>;

		PluginHost(const clap_host_t* host);

		/** Add a message to the host's log */
		void Log(LogSeverity severity, const std::string& message) const;

		/** Add a timer to be run every `periodMs` milliseconds. to run only once, call `CancelTimer()` in the body of your timer function. */
		TimerId AddTimer(uint32_t periodMs, TimerFn timerFn);
		/** Cancels an active timer with the provided id. this is safe to call from inside of timers. */
		void CancelTimer(TimerId id);
		/** implementation. TODO: friend declaration to hide this? */
		void OnTimer(TimerId id) const;

	private:
		const clap_host_t* mHost;
		const clap_host_log_t* mLog;
		const clap_host_timer_support_t* mTimer;
		const clap_host_gui_t* mGui;
		std::unordered_map<TimerId, TimerFn> mActiveTimers;
};
