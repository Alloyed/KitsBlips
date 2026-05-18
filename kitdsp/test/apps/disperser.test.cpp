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

    float snesBuffer[Disperser::GetNeededSampleMemory()];
    Disperser effect({snesBuffer, Disperser::GetNeededSampleMemory()});

    // test 1 default settings
    effect.Reset();
    for (size_t i = 0; i < len; ++i) {
        float_2 in = {f.samples[0][i] * 0.5f, f.samples[1][i] * 0.5f};
        float t = (float)i / (float)len;
        effect.SetParams(kitdsp::lerp(400.0f, 800.0f, t), sampleRate, kitdsp::lerp(8.0f, 1.0f, t), 32);
        float out = effect.Process(in.left);
        // out.left = kitdsp::clamp(out.left, -1.0f, 1.0f);
        // out.right = kitdsp::clamp(out.right, -1.0f, 1.0f);
        f.samples[0][i] = fade(in.left, out, 1.0f);
        f.samples[1][i] = fade(in.right, out, 1.0f);
    }

    test::Snapshot(f);
}
