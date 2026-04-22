#include <gtest/gtest.h>
#include <kitdsp/math/approx.h>
#include <kitdsp/osc/oscillatorUtil.h>
#include <kitdsp/pitch/zeroCrossingPitchDetector.h>

TEST(zeroCrossingPitchDetector, detectsSineWaves) {
    float SR = 48000.0f;
    kitdsp::Phasor ph;
    kitdsp::pitch::ZeroCrossingPitchDetector detect(SR);
    size_t len = static_cast<size_t>(SR * 2.0f);

    ph.SetFrequency(440.0f, SR);
    for (size_t i = 0; i < len; ++i) {
        float sinWave = kitdsp::approx::sin2pif_nasty(ph.Process());
        detect.Process(sinWave);
    }
    EXPECT_NEAR(detect.GetFrequency(), 440.0f, 1.0f);

    ph.SetFrequency(220.0f, SR);
    for (size_t i = 0; i < len; ++i) {
        float sinWave = kitdsp::approx::sin2pif_nasty(ph.Process());
        detect.Process(sinWave);
    }
    EXPECT_NEAR(detect.GetFrequency(), 220.0f, 1.0f);

    ph.SetFrequency(137.0f, SR);
    for (size_t i = 0; i < len; ++i) {
        float sinWave = kitdsp::approx::sin2pif_nasty(ph.Process());
        detect.Process(sinWave);
    }
    EXPECT_NEAR(detect.GetFrequency(), 137.0f, 1.0f);

    ph.SetFrequency(924.0f, SR);
    for (size_t i = 0; i < len; ++i) {
        float sinWave = kitdsp::approx::sin2pif_nasty(ph.Process());
        detect.Process(sinWave);
    }
    EXPECT_NEAR(detect.GetFrequency(), 924.0f, 1.0f);
}
