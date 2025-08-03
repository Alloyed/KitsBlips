#include "kitdsp/apps/psxReverb.h"
#include <gtest/gtest.h>
#include "kitdsp/apps/psxReverbPresets.h"
#include "kitdsp/math/util.h"
#include "kitdsp/wavFile.h"

using namespace kitdsp;

TEST(psxEcho, works) {
    constexpr size_t psxBufferSize = 65536;  // PSX::GetBufferDesiredSizeFloats(PSX::kOriginalSampleRate);
    float psxBuffer[psxBufferSize];
    PSX::Reverb psx(PSX::kOriginalSampleRate, psxBuffer, psxBufferSize);

    FILE* fp = fopen("psxverb.wav", "wb");
    ASSERT_NE(fp, nullptr);
    WavFileWriter<2> f{PSX::kOriginalSampleRate, fp};

    f.Start();

    size_t len = static_cast<size_t>(1.0f * PSX::kOriginalSampleRate);
    float out1, out2;

    // test 4 filter
    for (size_t filter = 0; filter < PSX::kNumPresets; ++filter) {
        psx.Reset();
        psx.cfg.preset = filter;
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);

            // simple saw
            float in = fmodf(t * 440.f, 1.0f) * clamp(1.0f - t * 10.0f, 0.0f, 1.0f);

            float_2 out = psx.Process(float_2{in, in});
            f.Add((out + in) * 0.5f);
        }
    }

    f.Finish();

    fclose(fp);
}