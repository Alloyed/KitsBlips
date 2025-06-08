#include "clapeze/basePlugin.h"

#include <cassert>
#include "clapeze/pluginHost.h"

BaseExt* BasePlugin::TryGetExtension(const char* name) {
    if (auto search = mExtensions.find(name); search != mExtensions.end()) {
        return search->second.get();
    } else {
        return nullptr;
    }
}
const BaseExt* BasePlugin::TryGetExtension(const char* name) const {
    if (auto search = mExtensions.find(name); search != mExtensions.end()) {
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
    if(mProcessor == nullptr)
    {
        mHost.Log(LogSeverity::Fatal, "mProcessor is null. did you forget to call ConfigProcessor() in your Config method?");
        return false;
    }
    for(const auto& [_, extension] : mExtensions) {
        if(!extension->Validate(*this))
        {
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
    BaseProcessor& processor = *self.mProcessor.get();

    const uint32_t frameCount = process->frames_count;
    const uint32_t inputEventCount = process->in_events->size(process->in_events);
    uint32_t eventIndex = 0;
    uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

    processor.ProcessFlush(*process);

    for (uint32_t frameIndex = 0; frameIndex < frameCount;) {
        // process events that occurred this frame
        while (eventIndex < inputEventCount && nextEventFrame == frameIndex) {
            const clap_event_header_t* event = process->in_events->get(process->in_events, eventIndex);
            if (event->time != frameIndex) {
                nextEventFrame = event->time;
                break;
            }

            processor.ProcessEvent(*event);
            eventIndex++;

            if (eventIndex == inputEventCount) {
                nextEventFrame = frameCount;
                break;
            }
        }
        size_t rangeStart = frameIndex;
        size_t rangeStop = nextEventFrame;
        // process audio from this frame
        processor.ProcessAudio(*process, rangeStart, rangeStop);
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
    }
    return mPlugin.get();
}
