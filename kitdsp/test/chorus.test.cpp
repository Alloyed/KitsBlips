#include "kitdsp/apps/chorus.h"
#include <gtest/gtest.h>
#include <AudioFile.h>
#include "kitdsp/math/util.h"

using namespace kitdsp;

TEST(chorus, works) {

    AudioFile<float> f;
    std::string path =PROJECT_DIR "/test/guitar.wav";
    bool ok = f.load(path);
    ASSERT_TRUE(ok);

    float sampleRate = f.getSampleRate();
    size_t len = f.getNumSamplesPerChannel();

    constexpr size_t snesBufferSize = 41000;
    float snesBuffer[snesBufferSize];
    Chorus chorus(etl::span<float>(snesBuffer, snesBufferSize), sampleRate);

    // test 1 default settings
    chorus.Reset();
    for (size_t i = 0; i < len; ++i) {
        float in = f.samples[0][i];
        float_2 out = chorus.Process(in);
        ASSERT_GE(out.left, -1.0f);
        ASSERT_LE(out.left, 1.0f);
        ASSERT_GE(out.right, -1.0f);
        ASSERT_LE(out.right, 1.0f);
        f.samples[0][i] = out.left;
        f.samples[1][i] = out.right;
    }

    f.save("chorus.wav");
}
