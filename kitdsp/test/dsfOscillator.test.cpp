#include <gtest/gtest.h>
#include "kitdsp/dsfOscillator.h"
#include "kitdsp/wavFile.h"

using namespace kitdsp;

TEST(dsfOscillator, CanBeUsed) {
    DsfOscillator osc;
    osc.Init(44100.0f);
    osc.SetFreqCarrier(1000.0f);
    osc.SetFreqModulator(2000.0f);
    osc.SetFalloff(0.4f);
    
    FILE* fp = fopen("dsf.wav","wb");
    ASSERT_NE(fp, nullptr);
    WavFile f{44100.0f, fp};

    f.Start();
    size_t len = static_cast<size_t>(4.0f * 44100.f);
    for(size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc.SetFalloff(0.9f - (t * 0.9f));

        float out1, out2;
        osc.Process(out1, out2);
        EXPECT_LE(out1, 1.0);
        EXPECT_GE(out1, -1.0);
        EXPECT_LE(out2, 1.0);
        EXPECT_GE(out2, -1.0);

        f.Add(out1);
    }
    f.Finish();

    fclose(fp);
}