// helper methods for creating an entry point
#include <clap/clap.h>
#include <cassert>
#include <vector>

#include "clapeze/basePlugin.h"

namespace clapeze {

struct RegisterPlugin {
    explicit RegisterPlugin(PluginEntry plugin) { sPlugins.push_back(plugin); }
    static std::vector<PluginEntry> sPlugins;
};

namespace EntryPoint {
bool _init(const char* path);
void _deinit();
const void* _get_factory(const char* factoryId);
}  // namespace EntryPoint
}  // namespace clapeze

#define CLAPEZE_CREATE_ENTRY_POINT()       \
    (clap_plugin_entry_t{                  \
        CLAP_VERSION_INIT,                 \
        clapeze::EntryPoint::_init,        \
        clapeze::EntryPoint::_deinit,      \
        clapeze::EntryPoint::_get_factory, \
    })