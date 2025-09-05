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

}  // namespace clapeze

namespace EntryPoint {
bool _init(const char* path);
void _deinit();
const void* _get_factory(const char* factoryId);
}  // namespace EntryPoint

#define CLAPEZE_CREATE_ENTRY_POINT() \
    (clap_plugin_entry_t{            \
        CLAP_VERSION_INIT,           \
        EntryPoint::_init,           \
        EntryPoint::_deinit,         \
        EntryPoint::_get_factory,    \
    })