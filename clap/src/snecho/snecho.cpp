#include "snecho/snecho.h"

#include "clap/ext/timer-support.h"
#include "clapeze/ext/parameters.h"
#include "clapeze/ext/state.h"
#include "clapeze/ext/timerSupport.h"
#include "clapeze/gui/imguiExt.h"
#include "clapeze/gui/imguiHelpers.h"
#include "descriptor.h"

using namespace kitdsp;

const PluginEntry Snecho::Entry{
    AudioEffectDescriptor("kitsblips.snecho", "Snecho", "A SNES-inspired mono delay effect"),
    [](PluginHost& host) -> BasePlugin* { return new Snecho(host); }};

void Snecho::Config() {
    EffectPlugin::Config();
    ConfigExtension<SnechoParamsExt>(GetHost(), SnechoParams::Count)
        .configNumeric(SnechoParams::Size, 0.0f, 1.0f, 0.5f, "Size")
        .configNumeric(SnechoParams::Feedback, 0.0f, 1.0f, 0.5f, "Feedback")
        .configNumeric(SnechoParams::FilterPreset, 0.0f, 1.0f, 0.0f, "Filter Preset")
        .configNumeric(SnechoParams::SizeRange, 0.0f, 1.0f, 0.5f, "Size Range")
        .configNumeric(SnechoParams::Mix, 0.0f, 1.0f, 0.5f, "Mix")
        .configNumeric(SnechoParams::FreezeEcho, 0.0f, 1.0f, 0.0f, "Freeze Echo")
        .configNumeric(SnechoParams::EchoDelayMod, 0.0f, 1.0f, 1.0f, "Echo Mod")
        .configNumeric(SnechoParams::FilterMix, 0.0f, 1.0f, 0.5f, "Filter Mix")
        .configNumeric(SnechoParams::ClearBuffer, 0.0f, 1.0f, 0.0f, "Clear Buffer")
        .configNumeric(SnechoParams::ResetHead, 0.0f, 1.0f, 0.0f, "Reset Playhead");

    ConfigExtension<StateExt>();
    if (GetHost().SupportsExtension(CLAP_EXT_TIMER_SUPPORT)) {
        ConfigExtension<TimerSupportExt>(GetHost());
    }
    if (GetHost().SupportsExtension(CLAP_EXT_GUI)) {
        ConfigExtension<ImGuiExt>(GetHost(), ImGuiConfig{[this]() { this->OnGui(); }});
    }
}

void Snecho::OnGui() {
    BaseParamsExt& params = BaseParamsExt::GetFromPlugin<BaseParamsExt>(*this);
    ImGuiHelpers::displayParametersBasic(params);
}

void Snecho::ProcessAudio(const StereoAudioBuffer& in,
                          StereoAudioBuffer& out,
                          SnechoParamsExt::AudioParameters& params) {
    // inputs
    // core
    snes1.cfg.echoBufferSize = params.Get(SnechoParams::Size);

    snes1.cfg.echoFeedback = params.Get(SnechoParams::Feedback);

    size_t filterPreset = static_cast<size_t>(params.Get(SnechoParams::FilterPreset) * SNES::kNumFilterPresets);
    if (filterPreset != mLastFilterPreset) {
        mLastFilterPreset = filterPreset;
        memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
        // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
    }

    size_t range = round(params.Get(SnechoParams::SizeRange));
    if (range == 0) {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
    } else if (range == 1) {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
    } else {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
    }

    float wetDryMix = params.Get(SnechoParams::Mix);

    snes1.cfg.freezeEcho = (params.Get(SnechoParams::FreezeEcho)) > 0.5f;

    // extension
    snes1.cfg.echoDelayMod = params.Get(SnechoParams::EchoDelayMod);

    snes1.cfg.filterMix = params.Get(SnechoParams::FilterMix);

    snes1.mod.clearBuffer = params.Get(SnechoParams::ClearBuffer) > 0.5f;
    snes1.mod.resetHead = params.Get(SnechoParams::ResetHead) > 0.5f;
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

bool Snecho::Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) {
    snesSampler = {SNES::kOriginalSampleRate, static_cast<float>(sampleRate)};
    return true;
}

void Snecho::Reset() {
    snes1.Reset();
}