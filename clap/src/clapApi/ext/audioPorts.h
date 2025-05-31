#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>

#include "clapApi/basePlugin.h"

template <size_t NUM_INPUTS, size_t NUM_OUTPUTS>
class StereoAudioPortsExt: public BaseExt {
   public:
    const char* Name() const override {
        return CLAP_EXT_AUDIO_PORTS;
    }

    const void* Extension() const override {
        static const clap_plugin_audio_ports_t value = {
            &_count,
            &_get,
        };
        return static_cast<const void*>(&value);
    }

   private:
    static uint32_t _count(const clap_plugin_t* plugin, bool isInput) {
        if (isInput) {
            return NUM_INPUTS;
        } else {
            return NUM_OUTPUTS;
        }
    }

    static bool _get(const clap_plugin_t* plugin, uint32_t index, bool isInput, clap_audio_port_info_t* info) {
        if (isInput && index < NUM_INPUTS) {
            // input
            info->id = index;
            info->channel_count = 2;
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;
            snprintf(info->name, sizeof(info->name), "%s %d", "Audio Input", index);
            return true;
        } else if (index < NUM_OUTPUTS) {
            // output
            info->id = index;
            info->channel_count = 2;
            info->flags = CLAP_AUDIO_PORT_IS_MAIN;
            info->port_type = CLAP_PORT_STEREO;
            info->in_place_pair = CLAP_INVALID_ID;  // TODO: might be nice to support for effects
            snprintf(info->name, sizeof(info->name), "%s %d", "Audio Output", index);
            return true;
        }
        // not a port
        return false;
    }
};