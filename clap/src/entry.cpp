/* Main clap entry point. enumerate plugins here! */

#include <clap/clap.h>
#include <memory>
#include <string_view>
#include <vector>
#include <array>

#include "template/plugin.h"


namespace PluginFactory {
    struct PluginEntry {
        std::string_view id;
        clap_plugin_descriptor_t* meta;
        const clap_plugin_t* (*factory)(const clap_host_t* host);
    };

    // Add your plugin here!
    const std::array<PluginEntry, 0> plugins = {
        //{Template::Id, &Template::Meta, Template::create},
    };

    uint32_t get_plugin_count(const clap_plugin_factory *factory) { 
        return plugins.size();
    }

    const clap_plugin_descriptor_t* get_plugin_descriptor(const clap_plugin_factory *factory, uint32_t index) {
        if(index > plugins.size()) {
            return nullptr;
        }
        const PluginEntry& entry = plugins[index];
        return entry.meta;
    }

    const clap_plugin_t* create_plugin(const clap_plugin_factory *factory, const clap_host_t *host, const char *pluginId) {
        if (!clap_version_is_compatible(host->clap_version)) {
            return nullptr;
        }

        for(const auto& entry : plugins) {
            if(entry.id == pluginId) {
                return entry.factory(host);
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

