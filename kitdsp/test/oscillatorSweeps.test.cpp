#include <gtest/gtest.h>
#include <AudioFile.h>
#include "kitdsp/math/vector.h"
#include "kitdsp/osc/blepOscillator.h"
#include "kitdsp/osc/dsfOscillator.h"
#include "kitdsp/osc/naiveOscillator.h"
#include "kitdsp/osc/oscillatorUtil.h"

using namespace kitdsp;

static const float SR = 48000.0f;

namespace {
    void CopyOut(const std::vector<float_2>& in, AudioFile<float>& f) {
        f.setAudioBufferSize(2, in.size());
        for(size_t i = 0; i < in.size(); ++i) {
            f.samples[0][i] = in[i].left;
            f.samples[1][i] = in[i].right;
        }
    }

    void CopyOut(const std::vector<float>& in, AudioFile<float>& f) {
        f.setAudioBufferSize(1, in.size());
        for(size_t i = 0; i < in.size(); ++i) {
            f.samples[0][i] = in[i];
        }
    }

    template <typename OSC1, typename OSC2>
        void Sweep(std::vector<float_2>& in, float sampleRate, OSC1& osc1, OSC2& osc2) {
            size_t len = static_cast<size_t>(4.0f * sampleRate);
            osc1.Reset();
            osc2.Reset();
            for (size_t i = 0; i < len; ++i) {
                float t = i / static_cast<float>(len);
                osc1.SetFrequency(t * 13000.0f, sampleRate);
                osc2.SetFrequency(t * 13000.0f, sampleRate);
                in.emplace_back(osc1.Process() * 0.5f, osc2.Process() * 0.5f);
            }
        }

    template <typename OSC1>
        void Sweep(std::vector<float>& in, float sampleRate, OSC1& osc1) {
            size_t len = static_cast<size_t>(4.0f * sampleRate);
            osc1.Reset();
            for (size_t i = 0; i < len; ++i) {
                float t = i / static_cast<float>(len);
                osc1.SetFrequency(t * 13000.0f, sampleRate);
                in.push_back(osc1.Process() * 0.5f);
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
        OversampledOscillator<TestPolyOsc, 8> oscRef;
        naive::RampUpOscillator oscNaive;
        blep::RampUpOscillator oscBlep;
        OversampledOscillator<naive::RampUpOscillator, 8> oscOSNaive;
        OversampledOscillator<blep::RampUpOscillator, 8> oscOSBlep;

        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float_2> buf;

        Sweep(buf, f.getSampleRate(), oscRef, oscNaive);
        Sweep(buf, f.getSampleRate(), oscRef, oscOSNaive);
        Sweep(buf, f.getSampleRate(), oscRef, oscBlep);
        Sweep(buf, f.getSampleRate(), oscRef, oscOSBlep);
        CopyOut(buf, f);
        f.save("saw_sweeps.wav");
    }

    {
        naive::PulseOscillator oscNaive;
        blep::PulseOscillator oscBlep;
        OversampledOscillator<naive::PulseOscillator, 8> oscOSNaive;
        OversampledOscillator<blep::PulseOscillator, 8> oscOSBlep;

        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float> buf;

        Sweep(buf, f.getSampleRate(), oscNaive);
        Sweep(buf, f.getSampleRate(), oscOSNaive);
        Sweep(buf, f.getSampleRate(), oscBlep);
        Sweep(buf, f.getSampleRate(), oscOSBlep);
        CopyOut(buf, f);
        f.save("pulse_sweeps.wav");
    }

    {
        naive::TriangleOscillator osc3;
        blep::TriangleOscillator osc3b;

        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float_2> buf;

        Sweep(buf, SR, osc3, osc3b);
        CopyOut(buf, f);
        f.save("tri_sweeps.wav");
    }
}

TEST(dsfOscillator, sweep) {
    DsfOscillator osc;

    {
        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float_2> buf;

        size_t len = static_cast<size_t>(SR);
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);
            osc.SetFreqCarrier(t * SR * 0.5f);
            osc.SetFreqModulator(0);
            osc.SetFalloff(0.0f);
            osc.Process();
            float f1 = osc.Formula1();
            float f2 = osc.Formula2();
            ASSERT_EQ(f1, f1);
            ASSERT_EQ(f2, f2);
            buf.emplace_back(f1, f2);
        }
        CopyOut(buf, f);
        f.save("dsf_sweeps_up.wav");

    }

    {
        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float_2> buf;

        size_t len = static_cast<size_t>(4.0f * SR);
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);
            osc.SetFreqCarrier(t * SR * 0.5f);
            osc.SetFreqModulator(t * SR * 0.5f);
            osc.SetFalloff(0.5f);
            osc.Process();
            buf.emplace_back(osc.Formula3(), osc.Formula4());
        }
        CopyOut(buf, f);
        f.save("dsf_sweeps_updown.wav");
    }
}
