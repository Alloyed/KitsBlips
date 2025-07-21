#include "clapeze/basePlugin.h"

#include <cassert>
#include "clapeze/pluginHost.h"

namespace clapeze {

BaseFeature* BasePlugin::TryGetFeature(const char* name) {
    if (auto search = mFeatures.find(name); search != mFeatures.end()) {
        return search->second.get();
    } else {
        return nullptr;
    }
}
const BaseFeature* BasePlugin::TryGetFeature(const char* name) const {
    if (auto search = mFeatures.find(name); search != mFeatures.end()) {
        return search->second.get();
    } else {
        return nullptr;
    }
}

PluginHost& BasePlugin::GetHost() {
    return mHost;
}
const PluginHost& BasePlugin::GetHost() const {
    return mHost;
}

bool BasePlugin::Init() {
    Config();
    return ValidateConfig();
}

bool BasePlugin::ValidateConfig() {
    if (mProcessor == nullptr) {
        mHost.Log(LogSeverity::Fatal,
                  "mProcessor is null. did you forget to call ConfigProcessor() in your Config method?");
        return false;
    }
    for (const auto& [_, extension] : mFeatures) {
        if (!extension->Validate(*this)) {
            return false;
        }
    }
    return true;
}

bool BasePlugin::Activate(double sampleRate, uint32_t minBlockSize, uint32_t maxBlockSize) {
    mProcessor->mSampleRate = sampleRate;
    mProcessor->mMinBlockSize = minBlockSize;
    mProcessor->mMaxBlockSize = maxBlockSize;
    mProcessor->Activate(sampleRate, minBlockSize, maxBlockSize);
    return true;
}
void BasePlugin::Deactivate() {
    mProcessor->Deactivate();
}
void BasePlugin::Reset() {
    mProcessor->ProcessReset();
}
BaseProcessor& BasePlugin::GetProcessor() const {
    return *mProcessor.get();
}
void BasePlugin::OnMainThread() {}

bool BasePlugin::_init(const clap_plugin* plugin) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    return self.Init();
}

void BasePlugin::_destroy(const clap_plugin* plugin) {
    BasePlugin* self = &BasePlugin::GetFromPluginObject(plugin);
    delete self;
}

bool BasePlugin::_activate(const clap_plugin* plugin,
                           double sampleRate,
                           uint32_t minFramesCount,
                           uint32_t maxFramesCount) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    return self.Activate(sampleRate, minFramesCount, maxFramesCount);
}

void BasePlugin::_deactivate(const clap_plugin* plugin) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    self.Deactivate();
}

bool BasePlugin::_start_processing(const clap_plugin* _plugin) {
    return true;
}

void BasePlugin::_stop_processing(const clap_plugin* _plugin) {}

void BasePlugin::_reset(const clap_plugin* plugin) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    self.Reset();
}

clap_process_status BasePlugin::_process(const clap_plugin* plugin, const clap_process_t* process) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    BaseProcessor& p = *self.mProcessor.get();

    // in clap parlance, samples == frames
    // for us, we'll use time as a clue that the values are related to sample-accurate timing.
    const uint32_t timeCount = process->frames_count;
    const uint32_t inputEventCount = process->in_events->size(process->in_events);
    uint32_t eventIndex = 0;
    p.mOutEvents = process->out_events;
    p.mTime = 0;
    uint32_t nextTimeIndex = inputEventCount ? 0 : timeCount;

    p.ProcessFlush(*process);

    while (p.mTime < timeCount) {
        // process events that occurred this frame
        while (eventIndex < inputEventCount && nextTimeIndex == p.mTime) {
            const clap_event_header_t* event = process->in_events->get(process->in_events, eventIndex);
            if (event->time != p.mTime) {
                nextTimeIndex = event->time;
                break;
            }

            p.ProcessEvent(*event);
            eventIndex++;

            if (eventIndex == inputEventCount) {
                nextTimeIndex = timeCount;
                break;
            }
        }
        size_t rangeStart = p.mTime;
        size_t rangeStop = nextTimeIndex;
        // process audio from this frame
        p.ProcessAudio(*process, rangeStart, rangeStop);
        p.mTime = nextTimeIndex;
    }

    p.mOutEvents = nullptr;
    return CLAP_PROCESS_CONTINUE;
}

const void* BasePlugin::_get_extension(const clap_plugin* plugin, const char* name) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    const BaseFeature* extension = self.TryGetFeature(name);
    return extension != nullptr ? extension->Extension() : nullptr;
}

void BasePlugin::_on_main_thread(const clap_plugin* plugin) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    self.OnMainThread();
}

const clap_plugin_t* BasePlugin::GetOrCreatePluginObject(const clap_plugin_descriptor_t* meta) {
    if (mPlugin.get() == nullptr) {
        clap_plugin_t pluginObject = {meta,
                                      this,
                                      &_init,
                                      &_destroy,
                                      &_activate,
                                      &_deactivate,
                                      &_start_processing,
                                      &_stop_processing,
                                      &_reset,
                                      &_process,
                                      &_get_extension,
                                      &_on_main_thread};
        mPlugin = std::make_unique<clap_plugin_t>(pluginObject);
    }
    return mPlugin.get();
}

}  // namespace clapeze