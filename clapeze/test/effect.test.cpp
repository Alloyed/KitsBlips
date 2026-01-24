#include <gtest/gtest.h>
#include "clapeze/entryPoint.h"
#include "mockHost.h"

// evil trick
#include "../examples/effectExample.cpp"

extern "C" const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();

TEST(effectPlugin, canBeInitialized) {
    using Host = clapeze::MockHost;
    Host::Init();
    auto* plugin = Host::CreatePlugin("clapeze.example.again");
    ASSERT_NE(nullptr, plugin);
    {
        auto* params = plugin->get_extension(plugin, CLAP_EXT_PARAMS);
        ASSERT_NE(nullptr, params);
    }
    Host::DestroyPlugin(plugin);
    Host::Deinit();
}