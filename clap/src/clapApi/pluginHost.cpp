#include "clapApi/pluginHost.h"

#include <cassert>
#include "clap/clap.h"

PluginHost::PluginHost(const clap_host_t* host):
	mHost(host),
	mLog(static_cast<const clap_host_log_t* >(host->get_extension(host, CLAP_EXT_LOG))),
	mTimer(static_cast<const clap_host_timer_support_t* >(host->get_extension(host, CLAP_EXT_TIMER_SUPPORT))),
	mGui(static_cast<const clap_host_gui_t* >(host->get_extension(host, CLAP_EXT_GUI)))
{
}

void PluginHost::RequestCallback() const
{
	mHost->request_callback(mHost);
}

void PluginHost::Log(LogSeverity severity, const std::string& message) const
{
	assert(mLog);
	return mLog->log(mHost, static_cast<clap_log_severity>(severity), message.c_str());
}

PluginHost::TimerId PluginHost::AddTimer(uint32_t periodMs, PluginHost::TimerFn fn)
{
	assert(mTimer);
	TimerId id;
	mTimer->register_timer(mHost, periodMs, &id);
	mActiveTimers.emplace(id, fn);
	return id;
}

void PluginHost::OnTimer(PluginHost::TimerId id) const
{
	mActiveTimers.at(id) ();
}

void PluginHost::CancelTimer(PluginHost::TimerId id)
{
	assert(mTimer);
	mActiveTimers.erase(id);
	mTimer->unregister_timer(mHost, id);
}
