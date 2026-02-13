#include "clapeze/entryPoint.h"

#include <clap/factory/preset-discovery.h>
#include "clapeze/features/presetFeature.h"

namespace clapeze {

namespace {

std::vector<PluginEntry>& sPlugins() {
    static std::vector<PluginEntry> inner;
    return inner;
};

std::string& sPluginPath() {
    static std::string inner;
    return inner;
};

namespace PluginFactory {

uint32_t get_plugin_count(const clap_plugin_factory* factory) {
    (void)factory;
    return static_cast<uint32_t>(sPlugins().size());
}

const clap_plugin_descriptor_t* get_plugin_descriptor(const clap_plugin_factory* factory, uint32_t index) {
    (void)factory;
    if (index < sPlugins().size()) {
        const PluginEntry& entry = sPlugins()[index];
        return &entry.meta;
    }
    return nullptr;
}

const clap_plugin_t* create_plugin(const clap_plugin_factory* factory, const clap_host_t* host, const char* pluginId) {
    (void)factory;
    if (!clap_version_is_compatible(host->clap_version)) {
        return nullptr;
    }

    static PluginHost cppHost(host);

    for (const auto& entry : sPlugins()) {
        if (std::string_view(entry.meta.id) == pluginId) {
            BasePlugin* plugin = entry.factory(entry.meta);
            // intentionally using a setter here to hide the host from the factory function
            plugin->SetHost(host);
            return plugin->GetPluginObject();
        }
    }

    return nullptr;
}

const clap_plugin_factory_t value = {
    get_plugin_count,
    get_plugin_descriptor,
    create_plugin,
};
}  // namespace PluginFactory

namespace PresetFactory {
uint32_t _count(const clap_preset_discovery_factory* factory) {
    (void)factory;
    return 1;
}

const clap_preset_discovery_provider_descriptor_t* _get_descriptor(const clap_preset_discovery_factory* factory,
                                                                   uint32_t index) {
    (void)factory;
    if (index == 0) {
        return PresetProvider::Descriptor();
    }
    return nullptr;
}

const clap_preset_discovery_provider_t* _create(const clap_preset_discovery_factory* factory,
                                                const clap_preset_discovery_indexer_t* indexer,
                                                const char* provider_id) {
    (void)factory;
    (void)provider_id;
    return PresetProvider::Create(indexer);
}

const clap_preset_discovery_factory_t value = {_count, _get_descriptor, _create};
}  // namespace PresetFactory

}  // namespace

void registerPlugin(PluginEntry plugin) {
    sPlugins().push_back(plugin);
}

void clearPlugins() {
    sPlugins().clear();
}

const char* getPluginPath() {
    return sPluginPath().c_str();
}

namespace EntryPoint {
bool _init(const char* path) {
    // add dll to mount path
    sPluginPath() = path;
    return true;
}
void _deinit() {}
const void* _get_factory(const char* factoryId) {
    if (std::string_view(CLAP_PLUGIN_FACTORY_ID) == factoryId) {
        return &PluginFactory::value;
    }
    if (std::string_view(CLAP_PRESET_DISCOVERY_FACTORY_ID) == factoryId) {
        return &PresetFactory::value;
    }
    return nullptr;
}
}  // namespace EntryPoint
}  // namespace clapeze