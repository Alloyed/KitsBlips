#include "kitdsp/apps/chorus.h"
#include <gtest/gtest.h>
#include "kitdsp/math/util.h"
#include "kitdsp/wavFile.h"

using namespace kitdsp;

TEST(chorus, works) {
    constexpr float sampleRate = 44100;
    constexpr size_t snesBufferSize = 7680UL;
    float snesBuffer[snesBufferSize];
    Chorus<2> chorus(etl::span<float>(snesBuffer, snesBufferSize), sampleRate);

    FILE* fp = fopen("chorus.wav", "wb");
    ASSERT_NE(fp, nullptr);
    WavFile<1> f{sampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(1.0f * sampleRate);

    // test 1 default settings
    chorus.Reset();
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);

        // simple saw
        float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);

        // float_2 out = chorus.Process(in);
        float_2 out = {in, in};
        ASSERT_GE(out.left, -1.0f);
        ASSERT_LE(out.left, 1.0f);
        ASSERT_GE(out.right, -1.0f);
        ASSERT_LE(out.right, 1.0f);
        f.Add(out.left);
    }

    f.Finish();

    fclose(fp);
}
