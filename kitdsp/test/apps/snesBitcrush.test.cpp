#include "kitdsp/apps/snesBitcrush.h"
#include <AudioFile.h>
#include <gtest/gtest.h>
#include "util.h"

using namespace kitdsp;

TEST(snesGaussFilter, works) {
    AudioFile<float> f;
    bool ok = f.load(PROJECT_DIR "/test/guitar.wav");
    ASSERT_TRUE(ok);

    float sampleRate = f.getSampleRate();
    size_t len = f.getNumSamplesPerChannel();

    constexpr size_t snesBufferSize = 41000;
    SNES::Bitcrush filter{sampleRate};

    // test 1 default settings
    filter.Reset();
    for (size_t i = 0; i < len; ++i) {
        float in = f.samples[0][i];
        float out = filter.Process(in);
        ASSERT_GE(out, -1.0f);
        ASSERT_LE(out, 1.0f);
        f.samples[0][i] = out;
        f.samples[1][i] = out;
    }

    test::Snapshot(f);
}
