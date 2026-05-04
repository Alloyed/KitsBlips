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
        float in = f.samples[0][i] * 0.5;
        float t = (float)i / (float)len;
        effect.SetParams(kitdsp::lerp(400.0f, 800.0f, t), sampleRate, kitdsp::lerp(8.0f, 1.0f, t), 32);
        float_2 out = float_2(effect.Process(in));
        // out.left = kitdsp::clamp(out.left, -1.0f, 1.0f);
        // out.right = kitdsp::clamp(out.right, -1.0f, 1.0f);
        f.samples[0][i] = fade(in, out.left, 1.0f);
        f.samples[1][i] = fade(in, out.right, 1.0f);
    }

    test::Snapshot(f);
}
