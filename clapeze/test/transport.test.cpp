#include <clapeze/transport.h>
#include <gtest/gtest.h>
#include "clap/events.h"
#include "clap/fixedpoint.h"

TEST(Transport, tempoWorks) {
    clapeze::Transport t{};
    clap_event_transport_t raw{};
    raw.flags |= CLAP_TRANSPORT_HAS_TEMPO;

    raw.tempo = 60.0;  // 1 beat per second
    t.ProcessEvent(raw);
    EXPECT_FLOAT_EQ(t.BeatsToMs(1.0f), 1000.0f);
    EXPECT_FLOAT_EQ(t.MsToBeats(1000.0f), 1.0f);
    EXPECT_FLOAT_EQ(t.BeatsToMs(0.5f), 500.0f);
    EXPECT_FLOAT_EQ(t.MsToBeats(500.0f), 0.5f);

    raw.tempo = 120.0;
    t.ProcessEvent(raw);
    EXPECT_FLOAT_EQ(t.BeatsToMs(1.0f), 500.0f);
    EXPECT_FLOAT_EQ(t.MsToBeats(500.0f), 1.0f);
    EXPECT_FLOAT_EQ(t.BeatsToMs(0.5f), 250.0f);
    EXPECT_FLOAT_EQ(t.MsToBeats(250.0f), 0.5f);

    float SR = 48000;
    EXPECT_FLOAT_EQ(t.BeatsToSamples(1.0f, SR), SR * 0.5f);
    EXPECT_FLOAT_EQ(t.SamplesToBeats(SR * 0.5f, SR), 1.0f);
    EXPECT_FLOAT_EQ(t.BeatsToSamples(0.5f, SR), SR * 0.25f);
    EXPECT_FLOAT_EQ(t.SamplesToBeats(SR * 0.25f, SR), 0.5f);
}

TEST(Transport, beatsWork) {
    clapeze::Transport t{};
    clap_event_transport_t raw{};
    raw.flags |= CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_IS_PLAYING;

    raw.tempo = 60.0;
    raw.song_pos_beats = 0;
    t.ProcessEvent(raw);

    int32_t beats{};
    float subBeats{};
    t.GetSongPosBeats(beats, subBeats);
    EXPECT_EQ(beats, 0);
    EXPECT_FLOAT_EQ(subBeats, 0.0f);

    t.Process(1.0f);  // advance by 1 sample == 1 second
    t.GetSongPosBeats(beats, subBeats);
    EXPECT_EQ(beats, 1);
    EXPECT_FLOAT_EQ(subBeats, 0.0f);

    raw.tempo = 120.0;
    raw.song_pos_beats = 0;
    t.ProcessEvent(raw);

    t.GetSongPosBeats(beats, subBeats);
    EXPECT_EQ(beats, 0);
    EXPECT_FLOAT_EQ(subBeats, 0.0f);

    t.Process(1.0f);  // advance by 1 sample == 1 second
    t.GetSongPosBeats(beats, subBeats);
    EXPECT_EQ(beats, 2);
    EXPECT_FLOAT_EQ(subBeats, 0.0f);
}

TEST(Transport, secondsWork) {
    clapeze::Transport t{};
    clap_event_transport_t raw{};
    raw.flags |= CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_IS_PLAYING;

    raw.song_pos_seconds = 0;
    t.ProcessEvent(raw);

    int32_t seconds{};
    float subSeconds{};
    t.GetSongPosSeconds(seconds, subSeconds);
    EXPECT_EQ(seconds, 0);
    EXPECT_FLOAT_EQ(subSeconds, 0.0f);

    t.Process(1.0f);  // advance by 1 sample == 1 second
    t.GetSongPosSeconds(seconds, subSeconds);
    EXPECT_EQ(seconds, 1);
    EXPECT_FLOAT_EQ(subSeconds, 0.0f);

    t.Process(2.0f);  // advance by 1 sample == 0.5 seconds
    t.GetSongPosSeconds(seconds, subSeconds);
    EXPECT_EQ(seconds, 1);
    EXPECT_FLOAT_EQ(subSeconds, 0.5f);

    raw.flags &= ~CLAP_TRANSPORT_IS_PLAYING;
    raw.song_pos_seconds = 1 * CLAP_SECTIME_FACTOR;
    t.ProcessEvent(raw);

    t.GetSongPosSeconds(seconds, subSeconds);
    EXPECT_EQ(seconds, 1);
    EXPECT_FLOAT_EQ(subSeconds, 0.0f);
    t.Process(1.0f);  // we aren't playing so this should do nothing
    t.GetSongPosSeconds(seconds, subSeconds);
    EXPECT_EQ(seconds, 1);
    EXPECT_FLOAT_EQ(subSeconds, 0.0f);
}