// helper methods for creating an entry point
#include <clap/clap.h>
#include <cassert>

#include "clapeze/basePlugin.h"

namespace clapeze {
/**
 * Explictly registers a new plugin entry. Call this during entry point init, or earlier.
 */
void registerPlugin(PluginEntry plugin);
/**
 * Removes existing plugin entries. for testing.
 */
void clearPlugins();
/**
 * Returns the path to the shared library that this plugin is run from.
 */
const char* getPluginPath();
namespace EntryPoint {
bool _init(const char* path);
void _deinit();
const void* _get_factory(const char* factoryId);
}  // namespace EntryPoint
}  // namespace clapeze

// inspired by https://artificial-mind.net/blog/2020/10/17/static-registration-macro

// using concat as defined here to make semi-hygenic symbols with __LINE__
#define CLAPEZE_DETAIL_CAT_INNER(a, b) a##b
#define CLAPEZE_DETAIL_CAT(a, b) CLAPEZE_DETAIL_CAT_INNER(a, b)

/**
 * Automatically registers a new plugin at global/file scope. As the macro implies, this uses a slightly advanced C++
 * trick, and it will _not_ work when used in a static library target. In that case, use the `clapeze::registerPlugin()`
 * syntax instead.
 * @param TPlugin the class used to define the plugin, must be of type clapeze::BasePlugin
 * @param descriptor extra metadata about the plugin. of type clap_plugin_descriptor_t
 * @example CLAPEZE_REGISTER_PLUGIN(MyPluginType, (clap_plugin_descriptor_t{ ... }));
 */
#define CLAPEZE_REGISTER_PLUGIN(TPlugin, descriptor)                                                              \
    static bool CLAPEZE_DETAIL_CAT(_plugin_register_, __LINE__) =                                                 \
        (clapeze::registerPlugin(clapeze::PluginEntry{                                                            \
             descriptor, ([](clapeze::PluginHost& host) -> clapeze::BasePlugin* { return new TPlugin(host); })}), \
         true)

/**
 * Constructs a CLAP compatible entry point. this should be used exactly once in your plugin library to provide hosts
 * with your plugin data. It should be assigned to an extern "C" variable named clap_entry!
 * @example extern "C" DLL_EXPORT const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();
 */
#define CLAPEZE_CREATE_ENTRY_POINT()       \
    (clap_plugin_entry_t{                  \
        CLAP_VERSION_INIT,                 \
        clapeze::EntryPoint::_init,        \
        clapeze::EntryPoint::_deinit,      \
        clapeze::EntryPoint::_get_factory, \
    })
