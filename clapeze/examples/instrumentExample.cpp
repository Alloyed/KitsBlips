#include <clapeze/common.h>
#include <clapeze/entryPoint.h>
#include <clapeze/instrumentPlugin.h>
#include <clapeze/params/dynamicParametersFeature.h>
#include <clapeze/params/parameterTypes.h>
#include <clapeze/state/tomlStateFeature.h>
#include <clapeze/voice.h>

#include <cmath>
#include <numbers>
#include "descriptor.h"

namespace {
enum class Params : clap_id { Fall, Polyphony, Count };
using ParamsFeature = clapeze::params::DynamicParametersFeature;
using ParamsHandle = ParamsFeature::ProcessorHandle;
}  // namespace

struct FallTraits : public clapeze::NumericParam {
    FallTraits() : clapeze::NumericParam("fall", "Fall", 0.0001f, 1.f, .01f) {}
};

struct PolyphonyTraits : public clapeze::IntegerParam {
    PolyphonyTraits() : clapeze::IntegerParam("polyphony", "Polyphony", 1, 16, 8, "voices", "voice") {}
};

namespace instrumentExample {
inline float mtof(float midiNote) {
    return std::exp2((midiNote - 69.0f) / 12.0f) * 440.0f;
}

inline float sinOsc(float phase) {
    return sinf(phase * std::numbers::pi_v<float> * 2.0f);
}

class Processor : public clapeze::InstrumentProcessor<ParamsHandle> {
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
            // This call is the primary difference between the Dynamic and Templated param variants.
            // The templated version knows which id is associated with which type, and also how to turn an enum into an
            // id. here, we do both of those manually.
            float fallRate = params.Get<FallTraits>(static_cast<clap_id>(Params::Fall));

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
    explicit Processor(clapeze::params::DynamicProcessorHandle& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    clapeze::ProcessStatus ProcessAudio(clapeze::StereoAudioBuffer& out) override {
        int32_t numVoices = mParams.Get<PolyphonyTraits>(static_cast<clap_id>(Params::Polyphony));
        mVoices.SetNumVoices(numVoices);
        return mVoices.ProcessAudio(out);
    }

    void ProcessNoteOn(const clapeze::NoteTuple& note, float velocity) override {
        mVoices.ProcessNoteOn(note, velocity);
    }

    void ProcessNoteOff(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const clapeze::NoteTuple& note) override { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() override { mVoices.StopAllVoices(); }

   private:
    clapeze::VoicePool<Processor, Voice, 16> mVoices;
};

class Plugin : public clapeze::InstrumentPlugin {
   public:
    explicit Plugin(const clap_plugin_descriptor_t& meta) : clapeze::InstrumentPlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        clapeze::InstrumentPlugin::Config();
        ParamsFeature& params = ConfigFeature<ParamsFeature>(GetHost(), static_cast<clap_id>(Params::Count))
                                    .Parameter(static_cast<clap_id>(Params::Fall), new FallTraits())
                                    .Parameter(static_cast<clap_id>(Params::Polyphony), new PolyphonyTraits());
        ConfigFeature<clapeze::TomlStateFeature<ParamsFeature>>(*this);
        ConfigProcessor<Processor>(params.GetProcessorHandle());
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioInstrumentDescriptor("clapeze.example.sines", "Sines", "a simple sine wave synth"));
}  // namespace instrumentExample
