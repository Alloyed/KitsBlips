#include <gtest/gtest.h>
#include "kitdsp/osc/blepOscillator.h"
#include "kitdsp/osc/dsfOscillator.h"
#include "kitdsp/osc/naiveOscillator.h"
#include "kitdsp/osc/oscillatorUtil.h"
#include "kitdsp/wavFile.h"

using namespace kitdsp;

static const float SR = 48000.0f;

namespace {
template <typename OSC1, typename OSC2>
void Sweep(WavFile<2>& f, OSC1& osc1, OSC2& osc2) {
    size_t len = static_cast<size_t>(4.0f * f.GetSampleRate());
    osc1.Reset();
    osc2.Reset();
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc1.SetFrequency(t * 13000.0f, f.GetSampleRate());
        osc2.SetFrequency(t * 13000.0f, f.GetSampleRate());
        f.Add(osc1.Process() * 0.5f);
        f.Add(osc2.Process() * 0.5f);
    }
}

template <typename OSC1>
void Sweep(WavFile<1>& f, OSC1& osc1) {
    size_t len = static_cast<size_t>(4.0f * f.GetSampleRate());
    osc1.Reset();
    for (size_t i = 0; i < len; ++i) {
        float t = i / static_cast<float>(len);
        osc1.SetFrequency(t * 13000.0f, f.GetSampleRate());
        f.Add(osc1.Process() * 0.5f);
    }
}

class TestPolyOsc : public Phasor {
    // float t = 0.; // 0 <= t < 1
    // float dt = freq / sample_rate;

   public:
    float poly_blep(float t, float dt) {
        // 0 <= t < 1
        if (t < dt) {
            t /= dt;
            // 2 * (t - t^2/2 - 0.5)
            return t + t - t * t - 1.;
        }

        // -1 < t < 0
        else if (t > 1. - dt) {
            t = (t - 1.) / dt;
            // 2 * (t^2/2 + t + 0.5)
            return t * t + t + t + 1.;
        }

        // 0 otherwise
        else {
            return 0.;
        }
    }

    float poly_saw(float t, float dt) {
        float naive_saw = 2. * t - 1.;
        return naive_saw - poly_blep(t, dt);
    }

    float Process() {
        mPhase += mAdvance;
        if (mPhase >= 1.)
            mPhase -= 1.;
        return poly_saw(mPhase, mAdvance);
    }
};

}  // namespace

TEST(blepOscillator, sweep) {
    {
        naive::RampUpOscillator osc1;
        blep::RampUpOscillator osc2;
        OversampledOscillator<naive::RampUpOscillator, 8> osc3;
        OversampledOscillator<blep::RampUpOscillator, 8> osc4;
        OversampledOscillator<TestPolyOsc, 8> osc5;

        FILE* fp = fopen("saw_sweeps.wav", "wb");
        ASSERT_NE(fp, nullptr);
        WavFile<1> f{SR, fp};

        f.Start();
        Sweep(f, osc1);
        Sweep(f, osc2);
        Sweep(f, osc3);
        Sweep(f, osc4);
        Sweep(f, osc5);
        f.Finish();

        fclose(fp);
    }

    {
        naive::PulseOscillator osc2;
        blep::PulseOscillator osc2b;
        FILE* fp = fopen("square_sweeps.wav", "wb");
        ASSERT_NE(fp, nullptr);
        WavFile<2> f{SR, fp};

        f.Start();
        Sweep(f, osc2, osc2b);
        f.Finish();

        fclose(fp);
    }

    {
        naive::TriangleOscillator osc3;
        blep::TriangleOscillator osc3b;
        FILE* fp = fopen("tri_sweeps.wav", "wb");
        ASSERT_NE(fp, nullptr);
        WavFile<2> f{SR, fp};

        f.Start();
        Sweep(f, osc3, osc3b);
        f.Finish();

        fclose(fp);
    }
}

TEST(dsfOscillator, sweep) {
    DsfOscillator osc;

    {
        FILE* fp = fopen("dsf_sweeps_up.wav", "wb");
        ASSERT_NE(fp, nullptr);
        WavFile<2> f{SR, fp};

        f.Start();
        size_t len = static_cast<size_t>(4.0f * f.GetSampleRate());
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);
            osc.SetFreqCarrier(t * f.GetSampleRate() * 0.5f);
            osc.SetFreqModulator(t * f.GetSampleRate() * 0.5f);
            osc.SetFalloff(0.5f);
            osc.Process();
            f.Add(osc.Formula1());
            f.Add(osc.Formula2());
        }
        f.Finish();

        fclose(fp);
    }

    {
        FILE* fp = fopen("dsf_sweeps_updown.wav", "wb");
        ASSERT_NE(fp, nullptr);
        WavFile<2> f{SR, fp};

        f.Start();
        size_t len = static_cast<size_t>(4.0f * f.GetSampleRate());
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);
            osc.SetFreqCarrier(t * f.GetSampleRate() * 0.5f);
            osc.SetFreqModulator(t * f.GetSampleRate() * 0.5f);
            osc.SetFalloff(0.5f);
            osc.Process();
            f.Add(osc.Formula3());
            f.Add(osc.Formula4());
        }
        f.Finish();

        fclose(fp);
    }
}