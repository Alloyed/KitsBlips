#include "clapeze/pluginHost.h"

#include <clap/all.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
namespace clapeze {

PluginHost::PluginHost(const clap_host_t* host)
    : mHost(host),
      mThreadCheck(static_cast<const clap_host_thread_check_t*>(host->get_extension(host, CLAP_EXT_THREAD_CHECK))),
      mLog(static_cast<const clap_host_log_t*>(host->get_extension(host, CLAP_EXT_LOG))),
      mTimer(static_cast<const clap_host_timer_support_t*>(host->get_extension(host, CLAP_EXT_TIMER_SUPPORT))),
      mGui(static_cast<const clap_host_gui_t*>(host->get_extension(host, CLAP_EXT_GUI))) {}

bool PluginHost::SupportsExtension(const char* extensionName) const {
    const void* ext = mHost->get_extension(mHost, extensionName);
    return ext != nullptr;
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

void PluginHost::LogSupportMatrix() const {
    static auto cStandardExtensions = {
        CLAP_EXT_AMBISONIC,
        CLAP_EXT_AUDIO_PORTS,
        CLAP_EXT_AUDIO_PORTS_CONFIG,
        CLAP_EXT_CONTEXT_MENU,
        CLAP_EXT_EVENT_REGISTRY,
        CLAP_EXT_GUI,
        CLAP_EXT_LATENCY,
        CLAP_EXT_LOG,
        CLAP_EXT_NOTE_NAME,
        CLAP_EXT_NOTE_PORTS,
        CLAP_EXT_PARAMS,
        CLAP_EXT_POSIX_FD_SUPPORT,
        CLAP_EXT_PRESET_LOAD,
        CLAP_EXT_REMOTE_CONTROLS,
        CLAP_EXT_STATE,
        CLAP_EXT_SURROUND,
        CLAP_EXT_TAIL,
        CLAP_EXT_THREAD_CHECK,
        CLAP_EXT_THREAD_POOL,
        CLAP_EXT_TIMER_SUPPORT,
        CLAP_EXT_TRACK_INFO,
        CLAP_EXT_VOICE_INFO,
    };
    static auto cDraftExtensions = {
        CLAP_EXT_MINI_CURVE_DISPLAY,
        CLAP_EXT_RESOURCE_DIRECTORY,
        CLAP_EXT_SCRATCH_MEMORY,
        CLAP_EXT_TRANSPORT_CONTROL,
        CLAP_EXT_TRIGGERS,
        CLAP_EXT_TUNING,
        CLAP_EXT_UNDO,
    };

    std::string hostString;
    hostString.append(mHost->name);
    std::replace(hostString.begin(), hostString.end(), ' ', '_');
#ifdef _WIN32
    hostString.append("-win-");
#endif
#ifdef __linux__
    hostString.append("-lin-");
#endif
#ifdef __APPLE__
    hostString.append("-mac-");
#endif
    hostString.append(mHost->version);

    std::stringstream ss;

    // header
    ss << "Name,Status,Host\n";
    // data
    for (const auto& x : cStandardExtensions) {
        ss << x << "," << (SupportsExtension(x) ? "Supported" : "Not Supported") << "," << hostString << "\n";
    }
    for (const auto& x : cDraftExtensions) {
        ss << x << "," << (SupportsExtension(x) ? "Supported" : "Not Supported") << "," << hostString << "\n";
    }

    /*
    // log to file
    */
    FILE* fp = fopen("CLAP_SUPPORT.csv", "w");
    fprintf(fp, "%s\n", ss.str().c_str());
    fclose(fp);
    /*
    // log to stdout
    */
    printf("%s\n", ss.str().c_str());
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
    if (mTimer && mActiveTimers.find(id) != mActiveTimers.end()) {
        mActiveTimers.erase(id);
        mTimer->unregister_timer(mHost, id);
    }
}

}  // namespace clapeze