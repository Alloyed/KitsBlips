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
        naive::RampUpOscillator osc1;
        blep::RampUpOscillator osc2;
        OversampledOscillator<naive::RampUpOscillator, 8> osc3;
        OversampledOscillator<blep::RampUpOscillator, 8> osc4;
        OversampledOscillator<TestPolyOsc, 8> osc5;

        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float> buf;

        Sweep(buf, f.getSampleRate(), osc1);
        Sweep(buf, f.getSampleRate(), osc2);
        Sweep(buf, f.getSampleRate(), osc3);
        Sweep(buf, f.getSampleRate(), osc4);
        Sweep(buf, f.getSampleRate(), osc5);
        CopyOut(buf, f);
        f.save("saw_sweeps");
    }

    {
        naive::PulseOscillator osc2;
        blep::PulseOscillator osc2b;

        AudioFile<float> f;
        f.setSampleRate(SR);
        std::vector<float_2> buf;

        Sweep(buf, SR, osc2, osc2b);
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

        size_t len = static_cast<size_t>(4.0f * SR);
        for (size_t i = 0; i < len; ++i) {
            float t = i / static_cast<float>(len);
            osc.SetFreqCarrier(t * SR * 0.5f);
            osc.SetFreqModulator(t * SR * 0.5f);
            osc.SetFalloff(0.5f);
            osc.Process();
            buf.emplace_back(osc.Formula1(), osc.Formula2());
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
