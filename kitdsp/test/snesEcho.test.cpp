#include "kitdsp/snesEcho.h"
#include <gtest/gtest.h>
#include "kitdsp/math/units.h"
#include "kitdsp/math/util.h"
#include "kitdsp/snesEchoFilterPresets.h"
#include "kitdsp/wavFile.h"

using namespace kitdsp;

TEST(snesEcho, works) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Echo snes(snesBuffer, snesBufferSize);

    FILE* fp = fopen("snecho.wav", "wb");
    ASSERT_NE(fp, nullptr);
    WavFile<1> f{SNES::kOriginalSampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(1.0f * SNES::kOriginalSampleRate);

    // test 1 simple feedback
    snes.Reset();
    snes.cfg.echoBufferSize = 0.5f;
    snes.cfg.echoFeedback = 0.5f;
    snes.cfg.filterMix = 0.0f;
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);

        // simple saw
        float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);

        float out = snes.Process(in);
        float mixed = in + out;
        ASSERT_GE(mixed, -1.0f);
        ASSERT_LE(mixed, 1.0f);
        f.Add(mixed);
    }

    // test 2 no feedback
    snes.Reset();
    snes.cfg.echoBufferSize = 1.0f;
    snes.cfg.echoFeedback = 0.0f;
    snes.cfg.filterMix = 0.0f;

    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);

        // simple saw
        float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);

        float out = snes.Process(in);
        float mixed = in + out;
        ASSERT_GE(mixed, -1.0f);
        ASSERT_LE(mixed, 1.0f);
        f.Add(mixed);
    }

    // test 3 freeze
    snes.Reset();
    snes.cfg.echoBufferSize = 0.5f;
    snes.cfg.echoFeedback = 0.5f;
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);

        // simple saw
        float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);
        snes.mod.freezeEcho = t > 0.1f;

        float out = snes.Process(in);
        float mixed = in + out;
        ASSERT_GE(mixed, -1.0f);
        ASSERT_LE(mixed, 1.0f);
        f.Add(mixed);
    }

    f.Finish();

    fclose(fp);
}

TEST(snesEchoFilter, works) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Echo snes(snesBuffer, snesBufferSize);

    FILE* fp = fopen("snechoFilter.wav", "wb");
    ASSERT_NE(fp, nullptr);
    WavFile<1> f{SNES::kOriginalSampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(1.0f * SNES::kOriginalSampleRate);
    // test 4 filter
    size_t numFilters = 4;
    for (size_t filter = 0; filter < numFilters; ++filter) {
        snes.Reset();
        snes.cfg.echoBufferSize = 0.2f;
        snes.cfg.echoFeedback = 0.5f;
        memcpy(snes.cfg.filterCoefficients, SNES::kFilterPresets[filter].data, SNES::kFIRTaps);
        snes.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filter].maxGainDb);
        snes.cfg.filterMix = 1.0f;
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);

            // simple saw
            float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);
            float out = snes.Process(in);
            float mixed = lerpf(in, out, 0.5f);
            ASSERT_GE(mixed, -1.0f);
            ASSERT_LE(mixed, 1.0f);
            f.Add(mixed);
        }
    }

    f.Finish();

    fclose(fp);
}

TEST(snesEchoBufferSize, isLinearQuantized) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Echo snes(snesBuffer, snesBufferSize);

    FILE* fp = fopen("snechoDelay.wav", "wb");
    ASSERT_NE(fp, nullptr);
    WavFile<1> f{SNES::kOriginalSampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(0.5f * SNES::kOriginalSampleRate);
    for (float size = 0.0f; size < 1.0f; size += 0.05) {
        snes.Reset();
        snes.cfg.echoBufferSize = size;
        snes.cfg.echoFeedback = 0.4f;
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);

            // simple saw
            float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);
            float out = snes.Process(in);
            float mixed = -in + out;
            ASSERT_GE(mixed, -1.0f);
            ASSERT_LE(mixed, 1.0f);
            f.Add(mixed);
        }
    }

    f.Finish();

    fclose(fp);
}