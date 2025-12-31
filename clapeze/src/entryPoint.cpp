#include "clapeze/entryPoint.h"
#include <algorithm>
#include <cstring>
#include "clap/factory/preset-discovery.h"
#include "clapeze/ext/presets.h"
#include "physfs.h"

namespace clapeze {
std::vector<PluginEntry> RegisterPlugin::sPlugins{};

namespace {
namespace PluginFactory {

uint32_t get_plugin_count(const clap_plugin_factory* factory) {
    return RegisterPlugin::sPlugins.size();
}

const clap_plugin_descriptor_t* get_plugin_descriptor(const clap_plugin_factory* factory, uint32_t index) {
    if (index < RegisterPlugin::sPlugins.size()) {
        const PluginEntry& entry = RegisterPlugin::sPlugins[index];
        return &entry.meta;
    }
    return nullptr;
}

const clap_plugin_t* create_plugin(const clap_plugin_factory* factory, const clap_host_t* host, const char* pluginId) {
    if (!clap_version_is_compatible(host->clap_version)) {
        return nullptr;
    }

    static PluginHost cppHost(host);

    for (const auto& entry : RegisterPlugin::sPlugins) {
        if (std::string_view(entry.meta.id) == pluginId) {
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
}  // namespace PluginFactory

namespace PresetFactory {
uint32_t _count(const clap_preset_discovery_factory* factory) {
    return 1;
}

const clap_preset_discovery_provider_descriptor_t* _get_descriptor(const clap_preset_discovery_factory* factory,
                                                                   uint32_t index) {
    if (index == 0) {
        return PresetProvider::Descriptor();
    }
    return nullptr;
}

const clap_preset_discovery_provider_t* _create(const clap_preset_discovery_factory* factory,
                                                const clap_preset_discovery_indexer_t* indexer,
                                                const char* provider_id) {
    return PresetProvider::Create(indexer);
}

const clap_preset_discovery_factory_t value = {_count, _get_descriptor, _create};
}  // namespace PresetFactory

}  // namespace

namespace EntryPoint {
bool _init(const char* path) {
    PHYSFS_init(path);
    // add dll to mount path
    PHYSFS_mount(path, nullptr, 0);
    auto& plugins = RegisterPlugin::sPlugins;
    // TODO: is this really necessary? plugins will be loaded on dll-init in an arbitrary order, but does anything
    // downstream actually care about plugin order?
    std::sort(plugins.begin(), plugins.end(),
              [](const auto& left, const auto& right) { return std::strcmp(left.meta.id, right.meta.id) < 0; });
    return true;
}
void _deinit() {
    PHYSFS_deinit();
}
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