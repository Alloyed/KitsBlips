#pragma once

#include "clap/events.h"

namespace clapeze {
struct Transport {
    // Replaces the transport state
    void Update(const clap_event_transport_t& newRaw) {
        raw = newRaw;
        beatAccumulator = 0.0f;
    }
    // advances the transport by a single sample
    void Process() {
        beatAccumulator += raw.tempo_inc;
        while (beatAccumulator >= 1.0) {
            beatAccumulator -= 1.0;
            raw.song_pos_beats += 1;
        }
    }

    clap_event_transport_t raw;
    double beatAccumulator;
    float sampleRate;
};
}  // namespace clapeze