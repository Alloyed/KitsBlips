#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>

#include "clapeze/basePlugin.h"
#include "clapeze/ext/baseFeature.h"

namespace clapeze {

class NotePortsFeature : public BaseFeature {
   public:
    NotePortsFeature(uint32_t numInputs, uint32_t numOutputs) : NUM_INPUTS(numInputs), NUM_OUTPUTS(numOutputs) {}
    static constexpr auto NAME = CLAP_EXT_NOTE_PORTS;
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override {
        static const clap_plugin_note_ports_t value = {
            &_count,
            &_get,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

   private:
    uint32_t NUM_INPUTS;
    uint32_t NUM_OUTPUTS;
    static uint32_t _count([[maybe_unused]] const clap_plugin_t* plugin, bool isInput) {
        NotePortsFeature& self = NotePortsFeature::GetFromPluginObject<NotePortsFeature>(plugin);
        if (isInput) {
            return self.NUM_INPUTS;
        } else {
            return self.NUM_OUTPUTS;
        }
    }

    static bool _get([[maybe_unused]] const clap_plugin_t* plugin,
                     uint32_t index,
                     bool isInput,
                     clap_note_port_info_t* info) {
        NotePortsFeature& self = NotePortsFeature::GetFromPluginObject<NotePortsFeature>(plugin);
        if (isInput && index < self.NUM_INPUTS) {
            // input
            info->id = index;
            info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
            info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
            snprintf(info->name, sizeof(info->name), "%s %u", "Note Input", index);
            return true;
        } else if (index < self.NUM_OUTPUTS) {
            // output
            info->id = index;
            info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
            info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
            snprintf(info->name, sizeof(info->name), "%s %u", "Note Output", index);
            return true;
        }
        // not a port
        return false;
    }
};
}  // namespace clapeze