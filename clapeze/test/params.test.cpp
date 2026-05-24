#include <clapeze/processor/transport.h>
#include <gtest/gtest.h>
#include "clap/events.h"
#include "clapeze/features/params/dynamicParametersFeature.h"
#include "clapeze/features/params/parameterTypes.h"
#include "clapeze/impl/casts.h"
#include "clapeze/pluginHost.h"

TEST(DynamicParameters, canSortModulation) {
    clap_host_t mockHost{
        .clap_version = CLAP_VERSION_INIT,
        .get_extension = [](const struct clap_host* host, const char* extension_id) -> const void* { return nullptr; },
    };
    clapeze::PluginHost host(&mockHost);
    clapeze::params::DynamicParametersFeature params(host, 2);
    params.Parameter(0, new clapeze::NumericParam("a", "a", 0.0f, 1.0f, 0.0f));
    params.Parameter(1, new clapeze::NumericParam("b", "b", 0.0f, 1.0f, 0.0f));

    auto& audio = clapeze::impl::down_cast<clapeze::params::DynamicAudioHandle&>(params.GetAudioHandle());
    EXPECT_STREQ(audio.GetKey(0).c_str(), "a");
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0), 0.0f);

    clap_event_param_mod_t ev;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_MOD;
    ev.param_id = 0;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.amount = 0.5;
    audio.ProcessEvent(ev.header);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0), 0.5f);

    ev.param_id = 0;
    ev.note_id = 12;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.amount = 1.0;
    audio.ProcessEvent(ev.header);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0, {12, -1, -1, -1}), 1.0f);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0, {13, -1, -1, -1}), 0.5f);

    ev.param_id = 0;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = 2;
    ev.key = -1;
    ev.amount = 0.75;
    audio.ProcessEvent(ev.header);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0, {12, -1, 2, -1}), 1.0f);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0, {13, -1, 2, -1}), 0.75f);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0, {13, -1, 3, -1}), 0.5f);
    EXPECT_FLOAT_EQ(audio.Get<clapeze::NumericParam>(0), 0.5f);
}
