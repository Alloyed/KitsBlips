/* Main clap entry point. enumerate plugins here! */

#include <clap/clap.h>
#include <memory>
#include <string_view>
#include <vector>
#include <array>

#include "clapApi/basePlugin.h"

#include "template/plugin.h"
#include "snecho/snecho.h"

namespace PluginFactory {

    // Add your plugin here!
    const std::vector<PluginEntry> plugins {
        Snecho::Entry,
    };

    uint32_t get_plugin_count(const clap_plugin_factory *factory) { 
        return plugins.size();
    }

    const clap_plugin_descriptor_t* get_plugin_descriptor(const clap_plugin_factory *factory, uint32_t index) {
        if(index > plugins.size()) {
            return nullptr;
        }
        const PluginEntry& entry = plugins[index];
        return &entry.meta;
    }

    const clap_plugin_t* create_plugin(const clap_plugin_factory *factory, const clap_host_t *host, const char *pluginId) {
        if (!clap_version_is_compatible(host->clap_version)) {
            return nullptr;
        }

        static PluginHost cppHost(host);

        for(const auto& entry : plugins) {
            if(entry.meta.id == pluginId) {
                return entry.factory(cppHost)->GetOrCreatePluginObject(&entry.meta);
            }
        }

        return nullptr;
    }

    const clap_plugin_factory_t value = {
        get_plugin_count,
        get_plugin_descriptor,
        create_plugin,
    };
}

namespace EntryPoint {
    bool init(const char* path) {
        return true;
    }
    void deinit() {
    }
    const void* get_factory(const char* factoryId) {
        return std::string_view(CLAP_PLUGIN_FACTORY_ID) == factoryId ? &PluginFactory::value: nullptr;
    }
}

extern "C" const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    EntryPoint::init,
    EntryPoint::deinit,
    EntryPoint::get_factory,
};

