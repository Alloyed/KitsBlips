#include "kitdsp/apps/disperser.h"
#include <AudioFile.h>
#include <gtest/gtest.h>
#include "kitdsp/math/util.h"
#include "kitdsp/math/vector.h"
#include "util.h"

using namespace kitdsp;

TEST(disperser, works) {
    AudioFile<float> f;
    bool ok = f.load(PROJECT_DIR "/test/guitar.wav");
    ASSERT_TRUE(ok);

    float sampleRate = f.getSampleRate();
    size_t len = f.getNumSamplesPerChannel();

    constexpr size_t snesBufferSize = 255 * 300;
    float snesBuffer[snesBufferSize];
    Disperser effect({snesBuffer, snesBufferSize});

    // test 1 default settings
    effect.Reset();
    for (size_t i = 0; i < len; ++i) {
        float in = f.samples[0][i] * 0.6;
        float t = (float)i / (float)len;
        effect.SetParams(12, kitdsp::lerp(0.0f, 1.0f, t), 200);
        float_2 out = float_2(effect.Process(in));
        // ASSERT_GE(out.left, -1.0f);
        // ASSERT_LE(out.left, 1.0f);
        // ASSERT_GE(out.right, -1.0f);
        // ASSERT_LE(out.right, 1.0f);
        f.samples[0][i] = fade(in, out.left, 1.0f);
        f.samples[1][i] = fade(in, out.right, 1.0f);
    }

    test::Snapshot(f);
}
