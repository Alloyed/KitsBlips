#pragma once

#include <cassert>
#include "clap/events.h"
#include "clap/fixedpoint.h"

namespace clapeze {
struct Transport {
    void ProcessEvent(const clap_event_transport_t& newRaw) { mRaw = newRaw; }

    /**
     * Optional: If your plugin benefits from sample accurate tempo and time ramps, you can call this in your Process()
     * loop to get smooth updates. if you don't need it, you don't need it, though.
     */
    void Process(float sampleRate) {
        if (IsPlaying()) {
            // advance each transport element by a sample
            if (HasTempo()) {
                mRaw.tempo += mRaw.tempo_inc;
                if (HasBeatsTimeline()) {
                    double beatsPerSample = mRaw.tempo / 60.0 / sampleRate;
                    mRaw.song_pos_beats += static_cast<int64_t>(CLAP_BEATTIME_FACTOR * beatsPerSample);
                }
            }
            if (HasSecondsTimeline()) {
                double secondsPerSample = static_cast<double>(1.0 / sampleRate);  // :p
                mRaw.song_pos_seconds += static_cast<int64_t>(CLAP_SECTIME_FACTOR * secondsPerSample);
            }
        }
    }

    bool HasTempo() const { return (mRaw.flags & CLAP_TRANSPORT_HAS_TEMPO) != 0; }
    float GetTempoBpm() const { return static_cast<float>(mRaw.tempo); }

    bool HasBeatsTimeline() const { return (mRaw.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0; }
    void GetSongPosBeats(int32_t& beat, float& subBeat) const {
        assert(HasBeatsTimeline());
        beat = static_cast<int32_t>(mRaw.song_pos_beats / CLAP_BEATTIME_FACTOR);
        subBeat =
            static_cast<float>(mRaw.song_pos_beats % CLAP_BEATTIME_FACTOR) / static_cast<float>(CLAP_BEATTIME_FACTOR);
    }

    bool HasSecondsTimeline() const { return (mRaw.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) != 0; }
    void GetSongPosSeconds(int32_t& second, float& subSecond) const {
        assert(HasSecondsTimeline());
        second = static_cast<int32_t>(mRaw.song_pos_seconds / CLAP_SECTIME_FACTOR);
        subSecond =
            static_cast<float>(mRaw.song_pos_seconds % CLAP_SECTIME_FACTOR) / static_cast<float>(CLAP_SECTIME_FACTOR);
    }

    bool IsPlaying() const { return (mRaw.flags & CLAP_TRANSPORT_IS_PLAYING) != 0; }
    bool IsRecording() const { return (mRaw.flags & CLAP_TRANSPORT_IS_RECORDING) != 0; }
    bool IsWithinPreRoll() const { return (mRaw.flags & CLAP_TRANSPORT_IS_WITHIN_PRE_ROLL) != 0; }

    float BeatsToMs(float beats) const {
        assert(HasTempo());
        double beatsPerMs = mRaw.tempo / 60.0 / 1000.0;
        return static_cast<float>(beats / beatsPerMs);
    }
    float MsToBeats(float ms) const {
        assert(HasTempo());
        double beatsPerMs = mRaw.tempo / 60.0 / 1000.0;
        return static_cast<float>(ms * beatsPerMs);
    }

    float BeatsToSamples(float beats, float sampleRate) const {
        assert(HasTempo());
        double beatsPerSample = mRaw.tempo / 60.0 / sampleRate;
        return static_cast<float>(beats / beatsPerSample);
    }
    float SamplesToBeats(float samples, float sampleRate) const {
        assert(HasTempo());
        double beatsPerSample = mRaw.tempo / 60.0 / sampleRate;
        return static_cast<float>(samples * beatsPerSample);
    }

   private:
    clap_event_transport_t mRaw{};
};
}  // namespace clapeze