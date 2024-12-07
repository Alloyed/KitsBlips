#include "kitdsp/osc/blepOscillator.h"
#include <gtest/gtest.h>
#include "kitdsp/wavFile.h"

using namespace kitdsp;

namespace {
template <typename OSC>
void Sweep(WavFile<1>& f, OSC& osc) {
    size_t len = static_cast<size_t>(4.0f * f.GetSampleRate());
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc.SetFrequency(t * f.GetSampleRate() * 0.25f, f.GetSampleRate());
        f.Add(osc.Process());
    }
}
}  // namespace

TEST(blepOscillator, works) {
    blep::RampUpOscillator osc1;
    blep::PulseOscillator osc2;
    blep::TriangleOscillator osc3;

    FILE* fp = fopen("blep_sweeps.wav", "wb");
    ASSERT_NE(fp, nullptr);
    WavFile<1> f{44100.0f, fp};

    f.Start();
    Sweep(f, osc1);
    Sweep(f, osc2);
    Sweep(f, osc3);
    f.Finish();

    fclose(fp);
}