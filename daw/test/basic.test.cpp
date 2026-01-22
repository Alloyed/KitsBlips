#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <unordered_map>
#include "clap/entry.h"
#include "clap/factory/plugin-factory.h"

// template for mock clap hosts

extern "C" const clap_plugin_entry_t clap_entry;

static constexpr const char* cPluginId = "kitsblips.kitskeys";

TEST(BasicHarness, CanLoadTests) {
    /* How to interact with mock host data:
     *  add your extension using:
     *     sMockHostData.extensions.insert({std::string(CLAP_EXT_GUI), &sMockGuiHost});
     *  and then get access to it via:
     *     auto data = static_cast<mock_host_data*>(host->host_data);
     */
    struct mock_host_data {
        std::unordered_map<std::string, void*> extensions;
    } sMockHostData;
    clap_host_t sMockHost{
        CLAP_VERSION,
        &sMockHostData,
        "mock host",
        "mock host vendor",
        "https://crouton.net",
        "0.0.0",
        // get_extension
        [](const clap_host_t* host, const char* extension_id) -> const void* {
            printf("get host extension: %s\n", extension_id);
            auto data = static_cast<mock_host_data*>(host->host_data);
            auto iter = data->extensions.find(extension_id);
            if (iter == data->extensions.end()) {
                return nullptr;
            }
            return iter->second;
        },
        // request_restart
        [](const clap_host_t* host) -> void {},
        // request_process
        [](const clap_host_t* host) -> void {},
        // request_callback
        [](const clap_host_t* host) -> void {},
    };
    clap_host_t* host = &sMockHost;
    bool ok = false;

    // find plugin
    printf("clap_entry.init()\n");
    auto& entry = clap_entry;
    clap_entry.init(getenv("PWD"));
    auto factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    EXPECT_TRUE(factory);
    auto pluginCount = factory->get_plugin_count(factory);
    EXPECT_TRUE(pluginCount >= 1);
    printf("factory->create_plugin(%s)\n", cPluginId);
    auto plugin = factory->create_plugin(factory, host, cPluginId);
    EXPECT_TRUE(plugin);
    // TODO: before now calling into host should EXPECT_TRUE(false), after now it's safe
    ok = plugin->init(plugin);
    EXPECT_TRUE(ok);

    // Fully initialized!
    printf("// Fully Initialized!\n");
    {
    }
    // cleanup
    printf("// cleanup \n");
    plugin->destroy(plugin);
    clap_entry.deinit();
}