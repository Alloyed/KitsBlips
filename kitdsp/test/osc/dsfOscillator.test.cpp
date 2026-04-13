#include "kitdsp/osc/dsfOscillator.h"
#include <AudioFile.h>
#include <gtest/gtest.h>
#include "kitdsp/math/vector.h"
#include "util.h"

using namespace kitdsp;

static const float SR = 48000.0f;

TEST(dsfOscillator, works) {
    DsfOscillator osc;
    osc.Init(SR);
    osc.SetFreqCarrier(1000.0f);
    osc.SetFreqModulator(2000.0f);
    osc.SetFalloff(0.4f);

    size_t len = static_cast<size_t>(4.0f * SR);

    AudioFile<float> f;
    f.setAudioBufferSize(1, len);
    f.setSampleRate(SR);

    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc.SetFalloff(0.9f - (t * 0.9f));

        osc.Process();
        float out1 = osc.Formula1();
        float out2 = osc.Formula2();
        float out3 = osc.Formula3();
        float out4 = osc.Formula4();

        /*
        EXPECT_LE(out1, 1.0);
        EXPECT_GE(out1, -1.0);
        EXPECT_LE(out2, 1.0);
        EXPECT_GE(out2, -1.0);
        */
        EXPECT_LE(out3, 1.0);
        EXPECT_GE(out3, -1.0);
        EXPECT_LE(out4, 1.0);
        EXPECT_GE(out4, -1.0);

        f.samples[0][i] = out3;
    }
    test::Snapshot(f);
}

TEST(dsfOscillator, sweep12) {
    DsfOscillator osc;
    AudioFile<float> f;
    f.setSampleRate(SR);
    std::vector<float_2> buf;

    size_t len = static_cast<size_t>(SR);
    f.setAudioBufferSize(2, len);
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc.SetFreqCarrier(t * SR * 0.5f);
        osc.SetFreqModulator(0);
        osc.SetFalloff(0.0f);
        osc.Process();
        f.samples[0][i] = osc.Formula1();
        f.samples[1][i] = osc.Formula2();
    }
    test::Snapshot(f);
}

TEST(dsfOscillator, sweep34) {
    DsfOscillator osc;
    AudioFile<float> f;
    f.setSampleRate(SR);
    std::vector<float_2> buf;

    size_t len = static_cast<size_t>(4.0f * SR);
    f.setAudioBufferSize(2, len);
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc.SetFreqCarrier(t * SR * 0.5f);
        osc.SetFreqModulator(t * SR * 0.5f);
        osc.SetFalloff(0.5f);
        osc.Process();
        f.samples[0][i] = osc.Formula3() * 0.5f;
        f.samples[1][i] = osc.Formula4() * 0.5f;
    }
    test::Snapshot(f);
}
