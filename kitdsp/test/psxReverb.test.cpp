#include "kitdsp/apps/psxReverb.h"
#include <AudioFile.h>
#include <gtest/gtest.h>
#include "kitdsp/apps/psxReverbPresets.h"
#include "kitdsp/math/util.h"

using namespace kitdsp;

TEST(psxEcho, works) {
    constexpr size_t psxBufferSize = 65536;  // PSX::GetBufferDesiredSizeFloats(PSX::kOriginalSampleRate);
    float psxBuffer[psxBufferSize];
    PSX::Reverb psx(PSX::kOriginalSampleRate, psxBuffer, psxBufferSize);

    size_t len = static_cast<size_t>(1.0f * PSX::kOriginalSampleRate);

    AudioFile<float> f;
    f.setAudioBufferSize(2, len);
    f.setSampleRate(PSX::kOriginalSampleRate);

    // test 4 filter
    for (size_t filter = 0; filter < PSX::kNumPresets; ++filter) {
        psx.Reset();
        psx.cfg.preset = filter;
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);

            // simple saw
            float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);

            float_2 out = psx.Process(float_2{in, in});
            float_2 mixed = (out + float_2{in, in}) * 0.5f;

            f.samples[0][i] = mixed.left;
            f.samples[1][i] = mixed.right;
        }
    }

    f.save("psxverb.wav");
}