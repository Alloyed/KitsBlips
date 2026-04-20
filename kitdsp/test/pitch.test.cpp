#include <gtest/gtest.h>
#include <kitdsp/pitch/zeroCrossingPitchDetector.h>
#include <kitdsp/osc/oscillatorUtil.h>
#include <kitdsp/math/approx.h>

TEST(zeroCrossingPitchDetector, detectsSineWaves) {
    float SR = 48000.0f;
    kitdsp::Phasor ph;
    kitdsp::pitch::ZeroCrossingPitchDetector detect;
    ph.SetFrequency(440.0f, SR);
    size_t len = static_cast<size_t>(SR * 1.0f);
    for(size_t i = 0; i < len; ++i) {
        float sinWave = kitdsp::approx::sin2pif_nasty(ph.Process());
        detect.Process(sinWave);
    }
}
