#include "snecho/snecho.h"

#include "clapeze/effectPlugin.h"
#include "clapeze/ext/parameterConfigs.h"
#include "clapeze/ext/parameters.h"
#include "descriptor.h"

#include <kitdsp/dbMeter.h>
#include <kitdsp/samplerate/resampler.h>
#include <kitdsp/snesEcho.h>
#include <kitdsp/snesEchoFilterPresets.h>

using namespace kitdsp;
namespace snecho {

enum class Params : clap_id {
    Mix,
    Size,
    Feedback,
    FilterPreset,
    FreezeEcho,
    ResetHead,
    SizeRange,
    EchoDelayMod,
    FilterMix,
    ClearBuffer,
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
        snes1.cfg.echoBufferSize = mParams.Get<NumericParam>(Params::Size);

        snes1.cfg.echoFeedback = mParams.Get<PercentParam>(Params::Feedback);

        size_t filterPreset = mParams.Get<IntegerParam>(Params::FilterPreset);
        if (filterPreset != mLastFilterPreset) {
            mLastFilterPreset = filterPreset;
            memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
            // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
        }

        int32_t range = mParams.Get<IntegerParam>(Params::SizeRange);
        if (range == 0) {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
        } else if (range == 1) {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
        } else {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
        }

        float wetDryMix = mParams.Get<PercentParam>(Params::Mix);

        snes1.cfg.freezeEcho = mParams.Get<OnOffParam>(Params::FreezeEcho) == OnOff::On;

        // extension
        snes1.cfg.echoDelayMod = mParams.Get<NumericParam>(Params::EchoDelayMod);

        snes1.cfg.filterMix = mParams.Get<PercentParam>(Params::FilterMix);

        snes1.mod.clearBuffer = mParams.Get<OnOffParam>(Params::ClearBuffer) == OnOff::On;
        snes1.mod.resetHead = mParams.Get<OnOffParam>(Params::ResetHead) == OnOff::On;
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

class Plugin : public EffectPlugin {
   public:
    static const PluginEntry Entry;
    Plugin(PluginHost& host) : EffectPlugin(host) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        EffectPlugin::Config();

        ParamsExt& params =
            ConfigExtension<ParamsExt>(GetHost(), Params::Count)
                .ConfigModule("Original")
                .ConfigParam<PercentParam>(Params::Mix, "Mix", 0.5f)
                .ConfigParam<PercentParam>(Params::Size, "Size", 0.5f)
                .ConfigParam<PercentParam>(Params::Feedback, "Feedback", 0.5f)
                .ConfigParam<IntegerParam>(Params::FilterPreset, "Filter Preset", 0, SNES::kNumFilterPresets, 0)
                .ConfigParam<OnOffParam>(Params::FreezeEcho, "Freeze Echo", OnOff::Off)
                .ConfigParam<OnOffParam>(Params::ResetHead, "Reset Playhead", OnOff::Off)
                .ConfigModule("Extensions")
                .ConfigParam<IntegerParam>(Params::SizeRange, "Size Range", 0, 2, 0)
                .ConfigParam<PercentParam>(Params::EchoDelayMod, "Echo Mod", 1.0f)
                .ConfigParam<PercentParam>(Params::FilterMix, "Filter Mix", 1.0f)
                .ConfigParam<OnOffParam>(Params::ClearBuffer, "Clear Buffer", OnOff::Off);

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioEffectDescriptor("kitsblips.snecho", "Snecho", "A SNES-inspired mono delay effect"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace snecho