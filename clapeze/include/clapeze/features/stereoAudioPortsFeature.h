#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>
#include "clapeze/basePlugin.h"
#include "clapeze/features/baseFeature.h"

namespace clapeze {

class StereoAudioPortsFeature : public BaseFeature {
   public:
    StereoAudioPortsFeature(uint32_t numInputs, uint32_t numOutputs) : mNumInputs(numInputs), mNumOutputs(numOutputs) {}
    static constexpr auto NAME = CLAP_EXT_AUDIO_PORTS;
    const char* Name() const override { return NAME; }
    void Configure(BasePlugin& self) override {
        static const clap_plugin_audio_ports_t value = {
            &_count,
            &_get,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

   private:
    uint32_t mNumInputs;
    uint32_t mNumOutputs;
    static uint32_t _count([[maybe_unused]] const clap_plugin_t* plugin, bool isInput) {
        StereoAudioPortsFeature& self = StereoAudioPortsFeature::GetFromPluginObject<StereoAudioPortsFeature>(plugin);
        if (isInput) {
            return self.mNumInputs;
        } else {
            return self.mNumOutputs;
        }
    }

    static bool _get([[maybe_unused]] const clap_plugin_t* plugin,
                     uint32_t index,
                     bool isInput,
                     clap_audio_port_info_t* info) {
        StereoAudioPortsFeature& self = StereoAudioPortsFeature::GetFromPluginObject<StereoAudioPortsFeature>(plugin);
        if (isInput && index < self.mNumInputs) {
            // input
            info->id = index;
            info->channel_count = 2;
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;
            snprintf(info->name, sizeof(info->name), "%s %u", "Audio Input", index);
            return true;
        } else if (index < self.mNumOutputs) {
            // output
            info->id = index;
            info->channel_count = 2;
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;  // TODO: might be nice to support for effects
            snprintf(info->name, sizeof(info->name), "%s %u", "Audio Output", index);
            return true;
        }
        // not a port
        return false;
    }
};
}  // namespace clapeze