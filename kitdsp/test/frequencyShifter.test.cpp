#include "kitdsp/frequencyShifter.h"
#include <gtest/gtest.h>
#include <AudioFile.h>
#include "kitdsp/math/util.h"
#include "kitdsp/math/vector.h"

using namespace kitdsp;

TEST(shift, works) {

    AudioFile<float> f;
    bool ok = f.load(PROJECT_DIR "/test/guitar.wav");
    ASSERT_TRUE(ok);

    float sampleRate = f.getSampleRate();
    size_t len = f.getNumSamplesPerChannel();

    FrequencyShifter shift(sampleRate);

    // test 1 default settings
    shift.Reset();
    for (size_t i = 0; i < len; ++i) {
        float in = f.samples[0][i];
        float out = shift.Process(in);
        ASSERT_GE(out, -1.0f);
        ASSERT_LE(out, 1.0f);
        f.samples[0][i] = out;
        f.samples[1][i] = out;
    }

    f.save("shift.wav");
}
