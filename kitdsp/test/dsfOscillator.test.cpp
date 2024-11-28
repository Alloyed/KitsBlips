#include <gtest/gtest.h>
#include "kitdsp/osc/dsfOscillator.h"
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

        f.Add(out3);
    }
    f.Finish();

    fclose(fp);
}