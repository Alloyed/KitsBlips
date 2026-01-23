#include <gtest/gtest.h>
#include <cstddef>
#include "clap/factory/plugin-factory.h"
#include "clap/plugin.h"
#include "clap/version.h"
#include "clapeze/basePlugin.h"
#include "clapeze/entryPoint.h"

extern "C" const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();

TEST(clap_entry, hasAllFields) {
    EXPECT_EQ(clap_entry.clap_version.major, CLAP_VERSION_MAJOR);
    EXPECT_EQ(clap_entry.clap_version.minor, CLAP_VERSION_MINOR);
    EXPECT_EQ(clap_entry.clap_version.revision, CLAP_VERSION_REVISION);
    EXPECT_NE(clap_entry.init, nullptr);
    EXPECT_NE(clap_entry.get_factory, nullptr);
    EXPECT_NE(clap_entry.deinit, nullptr);
}

TEST(clap_entry, canInit) {
    clapeze::clearPlugins();
    ASSERT_TRUE(clap_entry.init(""));
    clap_entry.deinit();
}

TEST(clap_factory, canBeQueriedWhileEmpty) {
    clapeze::clearPlugins();
    ASSERT_TRUE(clap_entry.init(""));
    auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    auto numPlugins = factory->get_plugin_count(factory);
    EXPECT_EQ(numPlugins, 0);
    clap_host_t mockHost{};
    EXPECT_EQ(nullptr, factory->create_plugin(factory, &mockHost, "fake_id"));
    clap_entry.deinit();
}

namespace {
class MockPlugin : public clapeze::BasePlugin {
   public:
    explicit MockPlugin(clapeze::PluginHost& host) : BasePlugin(host) {}
    void Config() override {};
};
}  // namespace

TEST(clap_factory, canCreatePlugins) {
    static clapeze::BasePlugin* sPlugin{};
    clapeze::clearPlugins();
    clapeze::registerPlugin({clap_plugin_descriptor_t{
                                 .clap_version = CLAP_VERSION_INIT,
                                 .id = "my_id",
                                 .name = "My Name",
                             },
                             ([](clapeze::PluginHost& host) -> clapeze::BasePlugin* {
                                 sPlugin = new MockPlugin(host);
                                 return sPlugin;
                             })});
    ASSERT_TRUE(clap_entry.init(""));
    auto* factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));

    auto numPlugins = factory->get_plugin_count(factory);
    EXPECT_EQ(numPlugins, 1);
    auto* desc = factory->get_plugin_descriptor(factory, 0);
    EXPECT_EQ(desc->clap_version.major, CLAP_VERSION_MAJOR);
    EXPECT_EQ(desc->clap_version.minor, CLAP_VERSION_MINOR);
    EXPECT_EQ(desc->clap_version.revision, CLAP_VERSION_REVISION);
    EXPECT_EQ(desc->id, "my_id");
    EXPECT_EQ(desc->name, "My Name");

    clap_host_t mockHost{
        .clap_version = CLAP_VERSION_INIT,
        .get_extension = [](const struct clap_host* host, const char* extension_id) -> const void* { return nullptr; },
    };
    EXPECT_EQ(nullptr, factory->create_plugin(factory, &mockHost, "fake_id"));
    auto* pluginObject = factory->create_plugin(factory, &mockHost, "my_id");
    EXPECT_EQ(sPlugin->GetOrCreatePluginObject(desc), pluginObject);

    clap_entry.deinit();
}