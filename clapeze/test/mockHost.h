#pragma once

#include <clap/entry.h>
#include <gtest/gtest.h>
#include "clap/factory/plugin-factory.h"
#include "clap/host.h"
#include "clap/plugin.h"

extern "C" const clap_plugin_entry_t clap_entry;

namespace clapeze {
class MockHost {
   public:
    static void Init() { ASSERT_TRUE(clap_entry.init("")); }
    static void Deinit() { clap_entry.deinit(); }

    static const clap_plugin_t* CreatePlugin(const std::string& pluginId) {
        auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
        auto* plugin = factory->create_plugin(factory, Host(), pluginId.c_str());
        bool ok = plugin->init(plugin);
        EXPECT_TRUE(ok);
        if (!ok) {
            plugin->destroy(plugin);
            return nullptr;
        }
        return plugin;
    }

    static void DestroyPlugin(const clap_plugin_t* plugin) { plugin->destroy(plugin); }

   private:
    static clap_host_t* Host() {
        static clap_host_t sHost{
            .clap_version = CLAP_VERSION_INIT,
            .host_data = nullptr,
            .name = "Mock Host",
            .vendor = "clapeze",
            .url = "https://crouton.net",
            .version = "0.0.0",
            .get_extension = _get_extension,
            .request_restart = _request_restart,
            .request_process = _request_process,
            .request_callback = _request_callback,

        };
        return &sHost;
    }

    static const void* _get_extension(const clap_host_t* host, const char* extension_id) { return nullptr; }
    static void _request_restart(const clap_host_t* host) {}
    static void _request_process(const clap_host_t* host) {}
    static void _request_callback(const clap_host_t* host) {}
};
}  // namespace clapeze