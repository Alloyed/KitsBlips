#include <AudioFile.h>
#include <gtest/gtest.h>
#include "kitdsp/control/dx7Envelope.h"

using namespace kitdsp;

constexpr float kOneSamplePerMs = 1000.0f;

TEST(dx7env, hasAttackRelease) {
    Dx7Envelope adsr;
    adsr.SetSegment(0, 1.0f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(1, 0.25f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(2, 0.5f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(3, 0.0f, 1.0f, kOneSamplePerMs);
    // attack
    adsr.TriggerOpen();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_EQ(0.0f, adsr.GetValue());
    for (uint32_t i = 0; i < 10; i++) {
        adsr.Process();
    }
    // sustain
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(.5f, adsr.GetValue(), 0.01f);
    for (uint32_t i = 0; i < 10; i++) {
        adsr.Process();
    }
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(.5f, adsr.GetValue(), 0.01f);
    // release
    adsr.TriggerClose();
    for (uint32_t i = 0; i < 10; i++) {
        adsr.Process();
    }
    adsr.Process();
    EXPECT_FALSE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
}

TEST(dx7env, canDecayAndBeRetriggered) {
    Dx7Envelope adsr;
    adsr.SetSegment(0, 1.0f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(1, 0.25f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(2, 0.5f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(3, 0.0f, 1.0f, kOneSamplePerMs);
    // attack
    adsr.TriggerOpen();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_EQ(0.0f, adsr.GetValue());
    adsr.Process();
    // peak
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    // decay
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.25f, adsr.GetValue(), 0.01f);
    // decay 2
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.5f, adsr.GetValue(), 0.01f);

    // retrigger
    // attack
    adsr.TriggerOpen();
    adsr.Process();
    // peak
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    // decay
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.25f, adsr.GetValue(), 0.01f);
    // decay 2
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.5f, adsr.GetValue(), 0.01f);
}

TEST(dx7env, canBeChoked) {
    Dx7Envelope adsr;
    adsr.SetSegment(0, 1.0f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(1, 0.25f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(2, 0.5f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(3, 0.0f, 1.0f, kOneSamplePerMs);
    // attack
    adsr.TriggerOpen();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    // sustain
    adsr.Process();
    adsr.Process();
    adsr.Process();
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.5f, adsr.GetValue(), 0.01f);
    // choke (should be complete in 1ms)
    adsr.TriggerChoke();
    adsr.Process();
    adsr.Process();
    EXPECT_FALSE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
}

TEST(dx7env, canBeReset) {
    Dx7Envelope adsr;
    adsr.SetSegment(0, 1.0f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(1, 0.25f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(2, 0.5f, 1.0f, kOneSamplePerMs);
    adsr.SetSegment(3, 0.0f, 1.0f, kOneSamplePerMs);
    adsr.Process();
    EXPECT_FALSE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);

    adsr.TriggerOpen();
    EXPECT_TRUE(adsr.IsProcessing());
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);

    adsr.Reset();
    EXPECT_FALSE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
}
