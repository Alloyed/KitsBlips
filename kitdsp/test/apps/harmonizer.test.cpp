#include <AudioFile.h>
#include <gtest/gtest.h>
#include "kitdsp/harmonizer.h"
#include "kitdsp/math/util.h"

using namespace kitdsp;

TEST(harmonizer, works) {
    AudioFile<float> f;
    bool ok = f.load(PROJECT_DIR "/test/guitar.wav");
    ASSERT_TRUE(ok);

    float sampleRate = f.getSampleRate();
    size_t len = f.getNumSamplesPerChannel();

    constexpr size_t snesBufferSize = 41000;
    float snesBuffer[snesBufferSize];
    Harmonizer harmonizer(etl::span<float>(snesBuffer, snesBufferSize), sampleRate);

    // test 1 default settings
    harmonizer.Reset();
    for (size_t i = 0; i < len; ++i) {
        float in = f.samples[0][i];
        // harmonizer.SetParams();
        float_2 out = float_2(harmonizer.Process(in));
        ASSERT_GE(out.left, -1.0f);
        ASSERT_LE(out.left, 1.0f);
        ASSERT_GE(out.right, -1.0f);
        ASSERT_LE(out.right, 1.0f);
        f.samples[0][i] = fade(in, out.left, 0.5f);
        f.samples[1][i] = fade(in, out.right, 0.5f);
    }

    f.save("grain.wav");
}
