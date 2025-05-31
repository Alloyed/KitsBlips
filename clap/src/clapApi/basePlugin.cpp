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

        self.ProcessRaw(process);

        return CLAP_PROCESS_CONTINUE;
    }

    const void* get_extension(const clap_plugin *plugin, const char *id) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        return nullptr;
        const ExtensionMap& extensions = self.GetExtensions();
        if (auto search = extensions.find(id); search != extensions.end()) {
            return search->second;
        } else {
            return nullptr;
        }
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