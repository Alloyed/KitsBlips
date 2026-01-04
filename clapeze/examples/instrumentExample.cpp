#include <clapeze/common.h>
#include <clapeze/entryPoint.h>
#include <clapeze/ext/parameterConfigs.h>
#include <clapeze/ext/parameters.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/voice.h>

#include <cmath>
#include <numbers>
#include "clapeze/basePlugin.h"
#include "descriptor.h"

namespace {
enum class Params : clap_id { Fall, Polyphony, Count };
using ParamsFeature = clapeze::ParametersFeature<Params>;
}  // namespace

template <>
struct clapeze::ParamTraits<Params, Params::Fall> : public clapeze::NumericParam {
    ParamTraits() : clapeze::NumericParam("Fall", 0.0001f, 1.f, .01f, "ms") {}
};

template <>
struct clapeze::ParamTraits<Params, Params::Polyphony> : public clapeze::IntegerParam {
    ParamTraits() : clapeze::IntegerParam("Polyphony", 1, 16, 8, "voices", "voice") {}
};

namespace sines {
inline float mtof(float midiNote) {
    return std::exp2((midiNote - 69.0f) / 12.0f) * 440.0f;
}

inline float sinOsc(float phase) {
    return std::sinf(phase * std::numbers::pi_v<float> * 2.0f);
}

class Processor : public clapeze::InstrumentProcessor<ParamsFeature::ProcessParameters> {
    class Voice {
       public:
        explicit Voice(Processor& p) : mProcessor(p) {}
        void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) {
            mFrequencyHz = mtof(note.key);
            mCurrentAmplitude = 1.0f;
            mTargetAmplitude = 1.0f;
        }
        void ProcessNoteOff() { mTargetAmplitude = 0.0f; }
        void ProcessChoke() { mCurrentAmplitude = 0.0f; }
        bool ProcessAudio(clapeze::StereoAudioBuffer& out) {
            const auto& params = mProcessor.mParams;
            const float sampleRate = static_cast<float>(mProcessor.GetSampleRate());
            float fallRate = params.Get<Params::Fall>();

            for (uint32_t index = 0; index < out.left.size(); index++) {
                // NOTE: this is sample-rate dependent! do something smarter in your own plugins
                mCurrentAmplitude = std::lerp(mCurrentAmplitude, mTargetAmplitude, fallRate);

                mPhase += mFrequencyHz / sampleRate;
                if (mPhase > 1.0f) {
                    mPhase -= 1.0f;
                }
                float wave = sinOsc(mPhase);
                out.left[index] = wave * mCurrentAmplitude;
                out.right[index] = out.left[index];
            }
            if (mCurrentAmplitude < 0.0001f && mTargetAmplitude == 0.0f) {
                // audio settled, time to sleep
                return false;
            }
            return true;
        }

       private:
        Processor& mProcessor;
        float mFrequencyHz{};
        float mPhase{};
        float mCurrentAmplitude{};
        float mTargetAmplitude{};
    };

   public:
    explicit Processor(ParamsFeature::ProcessParameters& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    void ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        mVoices.SetNumVoices(mParams.Get<Params::Polyphony>());
        mVoices.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mVoices.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() override { mVoices.StopAllVoices(); }

   private:
    clapeze::PolyphonicVoicePool<Processor, Voice, 16> mVoices;
};

class Plugin : public clapeze::InstrumentPlugin {
   public:
    explicit Plugin(clapeze::PluginHost& host) : clapeze::InstrumentPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        clapeze::InstrumentPlugin::Config();
        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), Params::Count)
                                    .Parameter<Params::Fall>()
                                    .Parameter<Params::Polyphony>();
        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin, AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"));
}  // namespace sines
