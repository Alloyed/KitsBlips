#include "clapApi/basePlugin.h"

#include <cassert>

/* TODO: investigate templating to avoid vtable lookups */
namespace PluginMethods {
    bool init(const clap_plugin *_plugin) {
        return true;
    }

    void destroy(const clap_plugin *plugin) {
        BasePlugin* self = &BasePlugin::GetFromPluginObject(plugin);
        delete self;
    }

    bool activate(const clap_plugin *plugin, double sampleRate, uint32_t minimumFramesCount, uint32_t maximumFramesCount) {
        return true;
    }

    void deactivate(const clap_plugin *_plugin) {
    }

    bool start_processing(const clap_plugin *_plugin) {
        return true;
    }

    void stop_processing(const clap_plugin *_plugin) {
    }

    void reset(const clap_plugin *_plugin) {
    }

    clap_process_status process(const clap_plugin *plugin, const clap_process_t *process) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);

        assert(process->audio_outputs_count == 1);
        assert(process->audio_inputs_count == 0);

        const uint32_t frameCount = process->frames_count;
        const uint32_t inputEventCount = process->in_events->size(process->in_events);
        uint32_t eventIndex = 0;
        uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

        for (uint32_t i = 0; i < frameCount; ) {
            while (eventIndex < inputEventCount && nextEventFrame == i) {
                const clap_event_header_t *event = process->in_events->get(process->in_events, eventIndex);
                if (event->time != i) {
                    nextEventFrame = event->time;
                    break;
                }

                //PluginProcessEvent(pState, event);
                eventIndex++;

                if (eventIndex == inputEventCount) {
                    nextEventFrame = frameCount;
                    break;
                }
            }
            //Audio::Render(i, nextEventFrame, process->audio_outputs[0].data32[0], process->audio_outputs[0].data32[1]);
            i = nextEventFrame;
        }

        return CLAP_PROCESS_CONTINUE;
    }

    const void* get_extension(const clap_plugin *plugin, const char *id) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        return nullptr;
        //const ExtensionMap& extensions = self.GetExtensions();
        //if (auto search = extensions.find(id); search != extensions.end()) {
        //    return search->second;
        //} else {
        //    return nullptr;
        //}
    }

    void on_main_thread(const clap_plugin *_plugin) {
    }
}

void BasePlugin::CreatePluginObject() {
    clap_plugin_t pluginObject = {
        &GetPluginDescriptor(),
        this,
        PluginMethods::init,
        PluginMethods::destroy,
        PluginMethods::activate,
        PluginMethods::deactivate,
        PluginMethods::start_processing,
        PluginMethods::stop_processing,
        PluginMethods::reset,
        PluginMethods::process,
        PluginMethods::get_extension,
        PluginMethods::on_main_thread
    };
    mPlugin = std::make_unique<clap_plugin_t>(pluginObject);
}
