#include "kitdsp/samplePlayer.h"
#include <AudioFile.h>
#include <gtest/gtest.h>
#include "util.h"

using namespace kitdsp;

TEST(samplePlayer, works) {
    AudioFile<float> f;
    bool ok = f.load(PROJECT_DIR "/test/guitar.wav");
    ASSERT_TRUE(ok);

    float sampleRate = f.getSampleRate();
    size_t len = f.getNumSamplesPerChannel();

    std::vector<float> samplesCopy = f.samples[0];
    std::fill(f.samples[0].begin(), f.samples[0].end(), 0.0f);
    std::fill(f.samples[1].begin(), f.samples[1].end(), 0.0f);
    SamplePlayer<float> sampler;
    sampler.SetSampleData({samplesCopy.data(), len}, sampleRate);
    sampler.SetSpeed(1.0f, sampleRate);
    // sampler.SetLoop(SamplePlayer<float>::LoopDirection::Backward, len - (len * 0.25f), len * 0.25f);

    // test 1 default settings
    sampler.Play();
    size_t idx = 0;
    for (size_t i = 0; i < len / 4; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }
    sampler.Choke();
    for (size_t i = 0; i < len * 3 / 4; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    test::Snapshot(f);
}
