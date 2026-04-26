#include "kitdsp/math/stft.h"
#include <gtest/gtest.h>
#include <kitdsp/math/approx.h>
#include <kitdsp/osc/oscillatorUtil.h>
#include <kitdsp/pitch/zeroCrossingPitchDetector.h>
#include "util.h"

TEST(stft, detectsSineWaves) {
    float SR = 44100.0f;
    kitdsp::Phasor ph;
    std::vector<float> buf;
    buf.resize(kitdsp::STFT<256>::GetDesiredBufferSize());
    kitdsp::STFT<256> fft(buf);
    size_t len = static_cast<size_t>(SR * 2.0f);

    ph.SetFrequency(4000.0f, SR);
    for (size_t i = 0; i < len; ++i) {
        float sinWave = kitdsp::approx::sin2pif_nasty(ph.Process());
        fft.Process(sinWave);
    }
    kitdsp::test::CsvFile<float, float, float> f;
    f.columns = {"frequency", "magnitude", "phase"};
    size_t N = fft.mOut.size() / 2;
    for (size_t i = 0; i < N; ++i) {
        float real = fft.mOut[i];
        float imag = fft.mOut[N + i];
        float freq = static_cast<float>(i) * SR / static_cast<float>(N * 2);
        float magnitude = sqrt(real * real + imag * imag);
        float phase = std::atan2(imag, real);
        f.rows.push_back({freq, magnitude, phase});
    }
    kitdsp::test::Snapshot(f);
}