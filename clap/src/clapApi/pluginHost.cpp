#include "clapApi/pluginHost.h"

#include <cassert>

PluginHost::PluginHost(const clap_host_t* host)
    : mHost(host),
      mThreadCheck(static_cast<const clap_host_thread_check_t*>(host->get_extension(host, CLAP_EXT_THREAD_CHECK))),
      mLog(static_cast<const clap_host_log_t*>(host->get_extension(host, CLAP_EXT_LOG))),
      mTimer(static_cast<const clap_host_timer_support_t*>(host->get_extension(host, CLAP_EXT_TIMER_SUPPORT))),
      mGui(static_cast<const clap_host_gui_t*>(host->get_extension(host, CLAP_EXT_GUI))) {
}

bool PluginHost::IsMainThread() const {
    return mThreadCheck && mThreadCheck->is_main_thread(mHost);
}

bool PluginHost::IsAudioThread() const {
    return mThreadCheck && mThreadCheck->is_audio_thread(mHost);
}

void PluginHost::Log(LogSeverity severity, const std::string& message) const {
    if (mLog) {
        return mLog->log(mHost, static_cast<clap_log_severity>(severity), message.c_str());
    }
}

void PluginHost::RequestCallback() const {
    mHost->request_callback(mHost);
}

PluginHost::TimerId PluginHost::AddTimer(uint32_t periodMs, PluginHost::TimerFn fn) {
    if (mTimer) {
        TimerId id;
        mTimer->register_timer(mHost, periodMs, &id);
        mActiveTimers.emplace(id, fn);
        return id;
    }
    return 0;
}

void PluginHost::OnTimer(PluginHost::TimerId id) const {
    if (mTimer) {
        mActiveTimers.at(id)();
    }
}

void PluginHost::CancelTimer(PluginHost::TimerId id) {
    if (mTimer) {
        mActiveTimers.erase(id);
        mTimer->unregister_timer(mHost, id);
    }
}
