#include <gtest/gtest.h>
#include "kitdsp/snesEcho.h"
#include "kitdsp/wavFile.h"
#include "kitdsp/util.h"

using namespace kitdsp;

TEST(snesEcho, works) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Model snes(SNES::kOriginalSampleRate, snesBuffer, snesBufferSize);

    FILE* fp = fopen("snecho.wav","wb");
    ASSERT_NE(fp, nullptr);
    WavFile f{SNES::kOriginalSampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(1.0f * SNES::kOriginalSampleRate);
    float out1, out2;

    // test 1 simple feedback
    snes.Reset();
    snes.cfg.echoBufferSize = 0.5f;
    snes.cfg.echoFeedback = 0.5f;
    snes.cfg.filterMix = 0.0f;
    for(size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        
        // simple saw
        float in = fmodf(t*440.f, 1.0f) * clampf(1.0f - t*10.0f, 0.0f, 1.0f);

        out1, out2;
        snes.Process(in, in, out1, out2);
        f.Add(in + out1);
    }
    
    // test 2 no feedback
    snes.Reset();
    snes.cfg.echoBufferSize = 1.0f;
    snes.cfg.echoFeedback = 0.0f;
    snes.cfg.filterMix = 0.0f;

    for(size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        
        // simple saw
        float in = fmodf(t*440.f, 1.0f) * clampf(1.0f - t*10.0f, 0.0f, 1.0f);

        snes.Process(in, in, out1, out2);
        f.Add(in + out1);
    }
    
    // test 3 freeze
    snes.Reset();
    snes.cfg.echoBufferSize = 0.5f;
    snes.cfg.echoFeedback = 0.5f;
    snes.cfg.filterMix = 0.0f;
    for(size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        
        // simple saw
        float in = fmodf(t*440.f, 1.0f) * clampf(1.0f - t*10.0f, 0.0f, 1.0f);
        snes.mod.freezeEcho = t > 0.1f;

        out1, out2;
        snes.Process(in, in, out1, out2);
        f.Add(in + out1);
    }
    

    f.Finish();

    fclose(fp);
}

TEST(snesEchoFilter, works) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Model snes(SNES::kOriginalSampleRate, snesBuffer, snesBufferSize);

    FILE* fp = fopen("snechoFilter.wav","wb");
    ASSERT_NE(fp, nullptr);
    WavFile f{SNES::kOriginalSampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(1.0f * SNES::kOriginalSampleRate);
    float out1, out2;

    // test 4 filter
    for (size_t filter = 0; filter < SNES::kNumFilterSettings; ++filter)
    {
        snes.Reset();
        snes.cfg.echoBufferSize = 0.5f;
        snes.cfg.echoFeedback = 0.5f;
        snes.cfg.filterSetting = filter;
        snes.cfg.filterMix = 1.0f;
        for (size_t i = 0; i < len; ++i)
        {
            float t = i / static_cast<float>(len);

            // simple saw
            float in = fmodf(t * 440.f, 1.0f) * clampf(1.0f - t * 10.0f, 0.0f, 1.0f);

            out1, out2;
            snes.Process(in, in, out1, out2);
            f.Add(in + out1);
        }
    }

    f.Finish();

    fclose(fp);
}