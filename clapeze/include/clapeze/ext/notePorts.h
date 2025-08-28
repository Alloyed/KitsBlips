#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>

#include "clapeze/ext/baseFeature.h"

namespace clapeze {

template <size_t NUM_INPUTS, size_t NUM_OUTPUTS>
class NotePortsFeature : public BaseFeature {
   public:
    static constexpr auto NAME = CLAP_EXT_NOTE_PORTS;
    const char* Name() const override { return NAME; }

    const void* Extension() const override {
        static const clap_plugin_note_ports_t value = {
            &_count,
            &_get,
        };
        return static_cast<const void*>(&value);
    }

   private:
    static uint32_t _count([[maybe_unused]] const clap_plugin_t* plugin, bool isInput) {
        if (isInput) {
            return NUM_INPUTS;
        } else {
            return NUM_OUTPUTS;
        }
    }

    static bool _get([[maybe_unused]] const clap_plugin_t* plugin, uint32_t index, bool isInput, clap_note_port_info_t* info) {
        if (isInput && index < NUM_INPUTS) {
            // input
            info->id = index;
            info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
            info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
            snprintf(info->name, sizeof(info->name), "%s %d", "Note Input", index);
            return true;
        } else if (index < NUM_OUTPUTS) {
            // output
            info->id = index;
            info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
            info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
            snprintf(info->name, sizeof(info->name), "%s %d", "Note Output", index);
            return true;
        }
        // not a port
        return false;
    }
};
}  // namespace clapeze