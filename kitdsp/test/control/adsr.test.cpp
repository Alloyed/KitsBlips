#include "kitdsp/control/adsr.h"
#include <AudioFile.h>
#include <gtest/gtest.h>

using namespace kitdsp;

constexpr float kOneSamplePerMs = 1000.0f;

TEST(adsr, hasAttackRelease) {
    ApproachAdsr adsr;
    adsr.SetParams(1.0f, 1.0f, 1.0f, 1.0f, kOneSamplePerMs);
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
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    for (uint32_t i = 0; i < 10; i++) {
        adsr.Process();
    }
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    // release
    adsr.TriggerClose();
    for (uint32_t i = 0; i < 10; i++) {
        adsr.Process();
    }
    adsr.Process();
    EXPECT_FALSE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
}

TEST(adsr, canDecayAndBeRetriggered) {
    ApproachAdsr adsr;
    adsr.SetParams(0.9f, 0.9f, 0.5f, 0.9f, kOneSamplePerMs);
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
    EXPECT_NEAR(0.5f, adsr.GetValue(), 0.01f);
}

TEST(adsr, canBeChoked) {
    ApproachAdsr adsr;
    adsr.SetParams(0.9f, 0.9f, 1.0f, 0.9f, kOneSamplePerMs);
    // attack
    adsr.TriggerOpen();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
    adsr.Process();
    // sustain
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    adsr.Process();
    EXPECT_TRUE(adsr.IsProcessing());
    EXPECT_NEAR(1.0f, adsr.GetValue(), 0.01f);
    // choke (should be complete in 1ms)
    adsr.TriggerChoke();
    adsr.Process();
    adsr.Process();
    EXPECT_FALSE(adsr.IsProcessing());
    EXPECT_NEAR(0.0f, adsr.GetValue(), 0.01f);
}

TEST(adsr, canBeReset) {
    ApproachAdsr adsr;
    adsr.SetParams(0.1f, 0.1f, 1.0f, 0.1f, 10.0f);
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
