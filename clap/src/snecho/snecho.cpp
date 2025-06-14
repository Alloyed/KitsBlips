#include "snecho/snecho.h"

#include "clapeze/effectPlugin.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/ext/parameterConfigs.h"
#include "descriptor.h"

#include <kitdsp/dbMeter.h>
#include <kitdsp/samplerate/resampler.h>
#include <kitdsp/snesEcho.h>
#include <kitdsp/snesEchoFilterPresets.h>

using namespace kitdsp;
namespace snecho {

enum class Params : clap_id {
    Size,
    Feedback,
    FilterPreset,
    SizeRange,
    Mix,
    FreezeEcho,
    EchoDelayMod,
    FilterMix,
    ClearBuffer,
    ResetHead,
    Count
};
using ParamsExt = ParametersExt<Params>;

class Processor : public EffectProcessor<ParamsExt::ProcessParameters> {
   public:
    Processor(ParamsExt::ProcessParameters& params) : EffectProcessor(params) {}
    ~Processor() = default;

    void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) override {
        // inputs
        // core
        snes1.cfg.echoBufferSize = mParams.GetRaw(Params::Size);

        snes1.cfg.echoFeedback = mParams.GetRaw(Params::Feedback);

        size_t filterPreset = static_cast<size_t>(mParams.GetRaw(Params::FilterPreset) * SNES::kNumFilterPresets);
        if (filterPreset != mLastFilterPreset) {
            mLastFilterPreset = filterPreset;
            memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
            // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
        }

        size_t range = round(mParams.GetRaw(Params::SizeRange));
        if (range == 0) {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
        } else if (range == 1) {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
        } else {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
        }

        float wetDryMix = mParams.GetRaw(Params::Mix);

        snes1.cfg.freezeEcho = (mParams.GetRaw(Params::FreezeEcho)) > 0.5f;

        // extension
        snes1.cfg.echoDelayMod = mParams.GetRaw(Params::EchoDelayMod);

        snes1.cfg.filterMix = mParams.GetRaw(Params::FilterMix);

        snes1.mod.clearBuffer = mParams.GetRaw(Params::ClearBuffer) > 0.5f;
        snes1.mod.resetHead = mParams.GetRaw(Params::ResetHead) > 0.5f;
        snes1.cfg.echoBufferIncrementSamples = SNES::kOriginalEchoIncrementSamples;

        // processing
        for (size_t idx = 0; idx < in.left.size(); ++idx) {
            float drySignal = in.left[idx];
            float wetSignal = snesSampler.Process<kitdsp::interpolate::InterpolationStrategy::None>(
                drySignal, [this](float in, float& out) { out = snes1.Process(in * 0.5f) * 2.0f; });

            // outputs
            out.left[idx] = lerpf(drySignal, wetSignal, wetDryMix);
            out.right[idx] = lerpf(drySignal, wetSignal, wetDryMix);
        }
    }

    void ProcessReset() override { snes1.Reset(); }

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        snesSampler = {SNES::kOriginalSampleRate, static_cast<float>(sampleRate)};
    }

   private:
    static constexpr size_t snesBufferSize = 7680UL * 10000;
    int16_t snesBuffer1[snesBufferSize];
    size_t mLastFilterPreset;
    kitdsp::SNES::Echo snes1{snesBuffer1, snesBufferSize};
    kitdsp::Resampler<float> snesSampler{kitdsp::SNES::kOriginalSampleRate, 41000};
};

enum class OnOff {
    On, Off
};
const std::vector<std::string_view> OnOffLabels = {"On", "Off"};

class Plugin : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    Plugin(PluginHost& host) : EffectPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::Config();

        ParamsExt& params = ConfigExtension<ParamsExt>(GetHost(), Params::Count)
                                .configParam(Params::Size, new NumericParam(0.0f, 1.0f, 0.5f, "Size"))
                                .configParam(Params::Feedback, new NumericParam(0.0f, 1.0f, 0.5f, "Feedback"))
                                .configParam(Params::FilterPreset, new IntegerParam(0, 1, 0, "Filter Preset"))
                                .configParam(Params::SizeRange, new NumericParam(0.0f, 1.0f, 0.5f, "Size Range"))
                                .configParam(Params::Mix, new NumericParam(0.0f, 1.0f, 0.5f, "Mix"))
                                .configParam(Params::FreezeEcho, new EnumParam<OnOff>(OnOffLabels, "Freeze Echo", OnOff::Off))
                                .configParam(Params::EchoDelayMod, new NumericParam(0.0f, 1.0f, 0.5f, "Echo Mod"))
                                .configParam(Params::FilterMix, new NumericParam(0.0f, 1.0f, 1.0f, "Filter Mix"))
                                .configParam(Params::ClearBuffer, new EnumParam<OnOff>(OnOffLabels, "Clear Buffer", OnOff::Off))
                                .configParam(Params::ResetHead, new EnumParam<OnOff>(OnOffLabels, "Reset Playhead", OnOff::Off));

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioEffectDescriptor("kitsblips.snecho", "Snecho", "A SNES-inspired mono delay effect"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace snecho