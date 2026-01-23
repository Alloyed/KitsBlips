#include "kitdsp/frequencyShifter.h"
#include <AudioFile.h>
#include <gtest/gtest.h>

using namespace kitdsp;

TEST(shift, works) {
    AudioFile<float> f;
    bool ok = f.load(PROJECT_DIR "/test/guitar.wav");
    ASSERT_TRUE(ok);

    float sampleRate = static_cast<float>(f.getSampleRate());
    size_t len = f.getNumSamplesPerChannel();

    FrequencyShifter shift(sampleRate);

    // test 1 default settings
    shift.Reset();
    shift.SetFrequencyOffset(40, sampleRate);
    for (size_t i = 0; i < len; ++i) {
        float in = f.samples[0][i] * 0.6f;
        float out = shift.Process(in);
        ASSERT_GE(out, -1.0f);
        ASSERT_LE(out, 1.0f);
        f.samples[0][i] = out;
        f.samples[1][i] = out;
    }

    f.save("shift.wav");
}
