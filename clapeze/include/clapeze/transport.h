#pragma once

#include "clap/events.h"

namespace clapeze {
struct Transport {
    // Replaces the transport state
    void Update(const clap_event_transport_t& newRaw) { raw = newRaw; }
    // advances the transport by a single sample
    void Process() {
        raw.song_pos_beats += raw.tempo_inc;
    }

    clap_event_transport_t raw;
    float sampleRate;
};
}  // namespace clapeze