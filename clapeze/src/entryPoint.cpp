#include "clapeze/entryPoint.h"
#include <algorithm>
#include <cstring>

namespace clapeze {

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

}  // namespace

namespace EntryPoint {
bool _init(const char* path) {
    auto& plugins = RegisterPlugin::sPlugins;
    std::sort(plugins.begin(), plugins.end(),
              [](const auto& left, const auto& right) { return std::strcmp(left.meta.id, right.meta.id) < 0; });
    return true;
}
void _deinit() {}
const void* _get_factory(const char* factoryId) {
    return std::string_view(CLAP_PLUGIN_FACTORY_ID) == factoryId ? &PluginFactory::value : nullptr;
}
}  // namespace EntryPoint
}  // namespace clapeze