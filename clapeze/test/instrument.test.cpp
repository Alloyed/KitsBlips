#include <gtest/gtest.h>

#include <clapeze/entryPoint.h>
#include <array>
#include <cstddef>
#include "clap/audio-buffer.h"
#include "clap/events.h"
#include "clap/ext/params.h"
#include "clap/process.h"
#include "mockHost.h"

// evil trick
#include "../examples/instrumentExample.cpp"

extern "C" const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();

class InstrumentPlugin : public testing::Test {
    using Host = clapeze::MockHost;

   protected:
    InstrumentPlugin() {}
    ~InstrumentPlugin() override = default;
    void SetUp() override {
        Host::Init();
        plugin = Host::CreatePlugin("clapeze.example.sines");
    }
    void TearDown() override {
        Host::DestroyPlugin(plugin);
        Host::Deinit();
    }
    const clap_plugin_t* plugin;
};

TEST_F(InstrumentPlugin, canBeInitialized) {
    ASSERT_NE(nullptr, plugin);
}

TEST_F(InstrumentPlugin, hasParameters) {
    auto* params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    ASSERT_NE(params, nullptr);
    ASSERT_EQ(params->count(plugin), 2);

    clap_param_info_t info;
    double num = 999.0;
    std::string text(100, 'x');

    ASSERT_TRUE(params->get_info(plugin, 0, &info));
    EXPECT_EQ(info.id, static_cast<clap_id>(Params::Fall));
    EXPECT_STREQ(info.name, "Fall");
    EXPECT_STREQ(info.module, "");
    // numeric param, 0-1
    EXPECT_DOUBLE_EQ(info.min_value, 0.0);
    EXPECT_DOUBLE_EQ(info.max_value, 1.0);
    ASSERT_TRUE(params->get_value(plugin, info.id, &num));
    EXPECT_FLOAT_EQ(num, info.default_value);
    ASSERT_TRUE(params->value_to_text(plugin, info.id, num, text.data(), text.size()));
    EXPECT_STREQ(text.data(), "0.010");

    ASSERT_TRUE(params->get_info(plugin, 1, &info));
    EXPECT_EQ(info.id, static_cast<clap_id>(Params::Polyphony));
    EXPECT_STREQ(info.name, "Polyphony");
    EXPECT_STREQ(info.module, "");
    // integer param, min-max
    EXPECT_FLOAT_EQ(info.min_value, 1.0f);
    EXPECT_FLOAT_EQ(info.max_value, 16.0f);
    EXPECT_FLOAT_EQ(info.default_value, 8.0f);
    ASSERT_TRUE(params->get_value(plugin, info.id, &num));
    EXPECT_FLOAT_EQ(num, info.default_value);
    ASSERT_TRUE(params->value_to_text(plugin, info.id, num, text.data(), text.size()));
    EXPECT_STREQ(text.data(), "8 voices");
}

TEST_F(InstrumentPlugin, canProcess) {
    // TODO: figuring out the bits, move to MockHost later.
    double sampleRate = 100.0;
    uint32_t blockSize = 10;

    clap_input_events_t in_events{
        .ctx = nullptr,
        .size = [](const struct clap_input_events* list) -> uint32_t { return 0; },
        .get = [](const struct clap_input_events* list, uint32_t index) -> const clap_event_header_t* {
            return nullptr;
        },
    };

    clap_output_events_t out_events{
        .ctx = nullptr,
        .try_push = [](const struct clap_output_events* list, const clap_event_header_t* event) -> bool {
            return false;
        },
    };

    std::vector<float> raw(static_cast<size_t>(blockSize * 2), 0.0f);
    std::array<float*, 2> channels{raw.data(), raw.data() + blockSize};
    clap_audio_buffer_t outputs{
        .data32 = channels.data(),
        .channel_count = 2,
        .constant_mask = false,
    };

    clap_process_t process{
        .steady_time = -1,
        .frames_count = blockSize,
        .transport = nullptr,
        .audio_inputs = nullptr,
        .audio_outputs = &outputs,
        .audio_inputs_count = 0,
        .audio_outputs_count = 2,
        .in_events = &in_events,
        .out_events = &out_events,
    };

    ASSERT_TRUE(plugin->activate(plugin, sampleRate, blockSize, blockSize));
    // mock audio thread starts
    ASSERT_TRUE(plugin->start_processing(plugin));
    clap_process_status status = plugin->process(plugin, &process);
    EXPECT_EQ(status, CLAP_PROCESS_CONTINUE_IF_NOT_QUIET);
    plugin->stop_processing(plugin);
    // main thread again
    plugin->deactivate(plugin);
}