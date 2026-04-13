#pragma once

#include <AudioFile.h>
#include <gtest/gtest.h>

namespace kitdsp::test {
inline void Snapshot(AudioFile<float>& f, std::string_view postfix = "") {
    // pass 1: sanity check
    float SR = f.getSampleRate();
    for (size_t i = 0; i < f.getNumSamplesPerChannel(); ++i) {
        for (size_t c = 0; c < f.getNumChannels(); ++c) {
            float s = f.samples[c][i];
            ASSERT_EQ(s, s) << "NaN sample at " << i << " (" << (i / SR) << " seconds)";
            ASSERT_GE(s, -1.0f) << "clipped sample at " << i << " (" << (i / SR) << " seconds)";
            ASSERT_LE(s, 1.0f) << "clipped sample at " << i << " (" << (i / SR) << " seconds)";
        }
    }

    // time to snapshot
    bool shouldUpdate = getenv("UPDATE_SNAPSHOTS") != nullptr;

    const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream ss;
    ss << PROJECT_DIR << "/test/snapshots/" << test_info->test_suite_name() << "_" << test_info->name() << postfix
       << ".wav";

    std::string filepath{ss.str()};
    AudioFile<float> expected;
    bool loaded = expected.load(filepath);
    if (!loaded || shouldUpdate) {
        // file may not exist, try to write
        ASSERT_TRUE(f.save(filepath)) << "File saving failed";
        return;
    }
    EXPECT_EQ(f.getSampleRate(), expected.getSampleRate());
    EXPECT_EQ(f.getNumChannels(), expected.getNumChannels());
    EXPECT_DOUBLE_EQ(f.getLengthInSeconds(), f.getLengthInSeconds());

    if (f.getNumChannels() != expected.getNumChannels() ||
        f.getNumSamplesPerChannel() != expected.getNumSamplesPerChannel() ||
        f.getSampleRate() != expected.getSampleRate()) {
        return;
    }

    for (size_t i = 0; i < f.getNumSamplesPerChannel(); ++i) {
        for (size_t c = 0; c < f.getNumChannels(); ++c) {
            ASSERT_NEAR(f.samples[c][i], expected.samples[c][i], 0.0001)
                << "Non-matching sample at " << i << " (" << (i / SR) << " seconds)\n"
                << "Set the environment variable UPDATE_SNAPSHOTS to update anyways";
        }
    }
}

}  // namespace kitdsp::test