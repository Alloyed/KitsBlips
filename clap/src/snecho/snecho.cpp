#include "snecho/snecho.h"

#include "clapeze/effectPlugin.h"
#include "clapeze/ext/parameters.h"
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
        snes1.cfg.echoBufferSize = mParams.Get(Params::Size);

        snes1.cfg.echoFeedback = mParams.Get(Params::Feedback);

        size_t filterPreset = static_cast<size_t>(mParams.Get(Params::FilterPreset) * SNES::kNumFilterPresets);
        if (filterPreset != mLastFilterPreset) {
            mLastFilterPreset = filterPreset;
            memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
            // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
        }

        size_t range = round(mParams.Get(Params::SizeRange));
        if (range == 0) {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
        } else if (range == 1) {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
        } else {
            snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
        }

        float wetDryMix = mParams.Get(Params::Mix);

        snes1.cfg.freezeEcho = (mParams.Get(Params::FreezeEcho)) > 0.5f;

        // extension
        snes1.cfg.echoDelayMod = mParams.Get(Params::EchoDelayMod);

        snes1.cfg.filterMix = mParams.Get(Params::FilterMix);

        snes1.mod.clearBuffer = mParams.Get(Params::ClearBuffer) > 0.5f;
        snes1.mod.resetHead = mParams.Get(Params::ResetHead) > 0.5f;
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

        ParamsExt& params = ConfigExtension<ParamsExt>(GetHost(), Params::Count)
                                .configNumeric(Params::Size, 0.0f, 1.0f, 0.5f, "Size")
                                .configNumeric(Params::Feedback, 0.0f, 1.0f, 0.5f, "Feedback")
                                .configNumeric(Params::FilterPreset, 0.0f, 1.0f, 0.0f, "Filter Preset")
                                .configNumeric(Params::SizeRange, 0.0f, 1.0f, 0.5f, "Size Range")
                                .configNumeric(Params::Mix, 0.0f, 1.0f, 0.5f, "Mix")
                                .configNumeric(Params::FreezeEcho, 0.0f, 1.0f, 0.0f, "Freeze Echo")
                                .configNumeric(Params::EchoDelayMod, 0.0f, 1.0f, 1.0f, "Echo Mod")
                                .configNumeric(Params::FilterMix, 0.0f, 1.0f, 0.5f, "Filter Mix")
                                .configNumeric(Params::ClearBuffer, 0.0f, 1.0f, 0.0f, "Clear Buffer")
                                .configNumeric(Params::ResetHead, 0.0f, 1.0f, 0.0f, "Reset Playhead");

        ConfigProcessor<Processor>(params.GetStateForAudioThread());
    }
};

const PluginEntry Entry{AudioEffectDescriptor("kitsblips.snecho", "Snecho", "A SNES-inspired mono delay effect"),
                        [](PluginHost& host) -> BasePlugin* { return new Plugin(host); }};

}  // namespace snecho