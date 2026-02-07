#include "kitdsp/apps/snesEcho.h"
#include <AudioFile.h>
#include <gtest/gtest.h>
#include "kitdsp/apps/snesEchoFilterPresets.h"
#include "kitdsp/math/units.h"
#include "kitdsp/math/util.h"

using namespace kitdsp;

namespace {
void CopyOut(const std::vector<float>& in, AudioFile<float>& f) {
    f.setAudioBufferSize(1, in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        f.samples[0][i] = in[i];
    }
}
}  // namespace

TEST(snesEcho, works) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Echo snes(snesBuffer, snesBufferSize);

    size_t len = static_cast<size_t>(1.0f * SNES::kOriginalSampleRate);

    std::vector<float> buf;

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
        buf.push_back(mixed);
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
        buf.push_back(mixed);
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
        buf.push_back(mixed);
    }

    AudioFile<float> f;
    f.setSampleRate(SNES::kOriginalSampleRate);
    CopyOut(buf, f);
    f.save("snecho.wav");
}

TEST(snesEchoFilter, works) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Echo snes(snesBuffer, snesBufferSize);

    size_t len = static_cast<size_t>(1.0f * SNES::kOriginalSampleRate);
    size_t numFilters = 4;

    std::vector<float> buf;

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

            float mixed = lerp(in, out, 0.5f);
            ASSERT_GE(mixed, -1.0f);
            ASSERT_LE(mixed, 1.0f);
            buf.push_back(mixed);
        }
    }

    AudioFile<float> f;
    f.setSampleRate(SNES::kOriginalSampleRate);
    CopyOut(buf, f);
    f.save("snechoFilter.wav");
}

TEST(snesEchoBufferSize, isLinearQuantized) {
    constexpr size_t snesBufferSize = 7680UL;
    int16_t snesBuffer[snesBufferSize];
    SNES::Echo snes(snesBuffer, snesBufferSize);

    size_t len = static_cast<size_t>(0.5f * SNES::kOriginalSampleRate);

    std::vector<float> buf;

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
            buf.push_back(mixed);
        }
    }

    AudioFile<float> f;
    f.setSampleRate(SNES::kOriginalSampleRate);
    CopyOut(buf, f);
    f.save("snechoDelay.wav");
}
