#include "sines/sines.h"

#include <kitdsp/control/approach.h>
#include <kitdsp/control/lfo.h>
#include <kitdsp/filters/svf.h>
#include <kitdsp/math/util.h>
#include <kitdsp/osc/blepOscillator.h>
#include "clapeze/common.h"
#include "clapeze/ext/parameterConfigs.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/gui/imguiExt.h"
#include "clapeze/gui/imguiHelpers.h"
#include "clapeze/instrumentPlugin.h"
#include "clapeze/voice.h"
#include "descriptor.h"
#include "gui/knob.h"

using namespace clapeze;

namespace sines {
enum class Params : clap_id { Rise, Fall, VibratoRate, VibratoDepth, Portamento, Polyphony, Count };
using ParamsExt = ParametersExt<Params>;

class Processor : public InstrumentProcessor<ParamsExt::ProcessParameters> {
    class Voice {
       public:
        Voice(Processor& p) : mProcessor(p) {}
        void ProcessNoteOn(const NoteTuple& note, float velocity) {
            mAmplitude.target = 1.0f;
            mPitch.target = note.key;
        }
        void ProcessNoteOff() { mAmplitude.target = 0.0f; }
        void ProcessChoke() {
            mOsc.Reset();
            mFilter.Reset();
            mAmplitude.Reset();
            mPitch.Reset();
            mVibrato.Reset();
        }
        bool ProcessAudio(StereoAudioBuffer& out) {
            // TODO: it'd be neat to adopt a signals based workflow for these, it'd need to be allocation-free though
            const auto params = mProcessor.mParams;
            const float sampleRate = mProcessor.GetSampleRate();
            mPitch.SetHalfLife(params.Get<NumericParam>(Params::Portamento), sampleRate);
            mAmplitude.SetHalfLife(
                params.Get<NumericParam>(mAmplitude.target > mAmplitude.current ? Params::Rise : Params::Fall),
                sampleRate);
            mVibrato.SetFrequency(params.Get<NumericParam>(Params::VibratoRate), sampleRate);
            float vibratoDepth = params.Get<NumericParam>(Params::VibratoDepth) * 0.01f;

            for (uint32_t index = 0; index < out.left.size(); index++) {
                // doing midiToFrequency last to ensure all effects get exponential fm
                float freq = kitdsp::midiToFrequency(mPitch.Process() + (mVibrato.Process() * vibratoDepth));
                mOsc.SetFrequency(freq, sampleRate);
                mFilter.SetFrequency(freq * 1.5, sampleRate, 10.0f);

                float s = mFilter.Process<kitdsp::FilterMode::LowPass>(mOsc.Process() * mAmplitude.Process() * 0.3f);
                out.left[index] += s;
                out.right[index] += s;
            }
            if (mAmplitude.current < 0.0001f && !mAmplitude.IsChanging()) {
                // audio settled, time to sleep
                return false;
            }
            return true;
        }

       private:
        Processor& mProcessor;
        kitdsp::blep::RampUpOscillator mOsc{};
        kitdsp::EmileSvf mFilter{};
        kitdsp::Approach mAmplitude{};
        kitdsp::Approach mPitch{};
        kitdsp::lfo::SineOscillator mVibrato{};
    };

   public:
    Processor(ParamsExt::ProcessParameters& params) : InstrumentProcessor(params), mVoices(*this) {}
    ~Processor() = default;

    void ProcessAudio(StereoAudioBuffer& out) override {
        mVoices.SetNumVoices(mParams.Get<IntegerParam>(Params::Polyphony));
        mVoices.ProcessAudio(out);
    }

    void ProcessNoteOn(const NoteTuple& note, float velocity) override { mVoices.ProcessNoteOn(note, velocity); }

    void ProcessNoteOff(const NoteTuple& note) override { mVoices.ProcessNoteOff(note); }

    void ProcessNoteChoke(const NoteTuple& note) override { mVoices.ProcessNoteChoke(note); }

    void ProcessReset() override { mVoices.StopAllVoices(); }

   private:
    PolyphonicVoicePool<Processor, Voice, 16> mVoices;
};

class Plugin : public InstrumentPlugin {
   public:
    Plugin(PluginHost& host) : InstrumentPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        InstrumentPlugin::Config();
        ParamsExt& params =
            ConfigExtension<ParamsExt>(GetHost(), Params::Count)
                .ConfigParam<NumericParam>(Params::Rise, "Rise", 1.0f, 1000.f, 500.0f, "ms")
                .ConfigParam<NumericParam>(Params::Fall, "Fall", 1.0f, 1000.f, 500.0f, "ms")
                .ConfigParam<NumericParam>(Params::VibratoRate, "Vibrato Rate", .1f, 10.0f, 1.0f, "hz")
                .ConfigParam<NumericParam>(Params::VibratoDepth, "Vibrato Depth", 0.0f, 200.0f, 50.0f, "cents")
                .ConfigParam<NumericParam>(Params::Portamento, "Portamento", 1.0f, 100.0f, 10.0f, "ms")
                .ConfigParam<IntegerParam>(Params::Polyphony, "Polyphony", 1, 16, 8, "voices", "voice");
        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }

#ifdef KITSBLIPS_ENABLE_GUI
    void OnGui() override {
        ParamsExt& params = BaseParamsExt::GetFromPlugin<ParamsExt>(*this);
        ImGuiHelpers::beginMain([&]() {
            ImGui::Text("Oh yeah, gamer time!");
            double inout = params.GetRaw(Params::Rise);
            if (kitgui::knob("Rise", static_cast<const NumericParam&>(*params.GetConfig(Params::Rise)), {}, inout)) {
                params.SetRaw(Params::Rise, inout);
            }
        });
    };
#endif
};

const PluginEntry Entry{AudioInstrumentDescriptor("kitsblips.sines", "Sines", "a simple sine wave synth"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace sines
