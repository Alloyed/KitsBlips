#include "clapApi/basePlugin.h"

#include <cassert>

BaseExt* BasePlugin::TryGetExtension(const char* name) {
    if (auto search = mExtensions.find(name); search != mExtensions.end()) {
        return search->second.get();
    } else {
        return nullptr;
    }
}

double BasePlugin::GetSampleRate() const {
    return mSampleRate;
}

PluginHost& BasePlugin::GetHost() {
    return mHost;
}

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
    self.mSampleRate = sampleRate;
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

    const uint32_t frameCount = process->frames_count;
    const uint32_t inputEventCount = process->in_events->size(process->in_events);
    uint32_t eventIndex = 0;
    uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

    self.ProcessFlush(*process);

    for (uint32_t frameIndex = 0; frameIndex < frameCount;) {
        // process events that occurred this frame
        while (eventIndex < inputEventCount && nextEventFrame == frameIndex) {
            const clap_event_header_t* event = process->in_events->get(process->in_events, eventIndex);
            if (event->time != frameIndex) {
                nextEventFrame = event->time;
                break;
            }

            self.ProcessEvent(*event);
            eventIndex++;

            if (eventIndex == inputEventCount) {
                nextEventFrame = frameCount;
                break;
            }
        }
        size_t rangeStart = frameIndex;
        size_t rangeStop = nextEventFrame;
        // process audio from this frame
        self.ProcessAudio(*process, rangeStart, rangeStop);
        frameIndex = nextEventFrame;
    }

    return CLAP_PROCESS_CONTINUE;
}

const void* BasePlugin::_get_extension(const clap_plugin* plugin, const char* name) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    const BaseExt* extension = self.TryGetExtension(name);
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
        Config();
    }
    return mPlugin.get();
}