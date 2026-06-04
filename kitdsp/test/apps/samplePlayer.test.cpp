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
    size_t idx = 0;

    // test playback
    sampler.Play();
    sampler.SetSpeed(13.0f / 12.0f, sampleRate);
    for (size_t i = 0; i < len / 4; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    // test mid-sample reverse
    sampler.SetSpeed(-13.0f / 12.0f, sampleRate);
    for (size_t i = 0; i < len / 8; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    // jump to middle
    sampler.SetSpeed(13.0f / 12.0f, sampleRate);
    sampler.Play(narrow_cast<float>(len / 2));
    for (size_t i = 0; i < len / 8; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    // test choke
    sampler.Choke();
    for (size_t i = 0; i < len / 2; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    test::Snapshot(f);
}

TEST(samplePlayer, canLoop) {
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
    sampler.SetLoop(SamplePlayer<float>::LoopDirection::Forward, len / 8, len / 4);
    size_t idx = 0;

    // test playback
    sampler.SetSpeed(11.0f / 12.0f, sampleRate);
    sampler.Play();
    for (size_t i = 0; i < len / 2; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    // extra backwards
    sampler.SetSpeed(-11.0f / 12.0f, sampleRate);
    sampler.Play();
    for (size_t i = 0; i < len / 2; ++i) {
        float out = sampler.Process();
        f.samples[0][idx] = out;
        f.samples[1][idx] = out;
        idx++;
    }

    test::Snapshot(f);
}
