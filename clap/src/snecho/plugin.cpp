#include "snecho/plugin.h"
#include "descriptor.h"

// clang-format off
const PluginEntry Snecho::Entry{
    // meta
    AudioEffectDescriptor(
        "alloyed.me/snecho",
        "Snecho",
        "A SNES-inspired mono delay effect"
    ),
    // factory
    [](PluginHost& host) -> BasePlugin* {
        static Snecho plugin(host);
        return &plugin;
    }
};
// clang-format on

void Snecho::ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) {
    /*
    // inputs
    // core
    snes1.cfg.echoBufferSize = params[SIZE_PARAM].getValue();
    snes1.mod.echoBufferSize = inputs[SIZE_CV_INPUT].getVoltage() * 0.1f;

    snes1.cfg.echoFeedback = params[FEEDBACK_PARAM].getValue();
    snes1.mod.echoFeedback = inputs[FEEDBACK_CV_INPUT].getVoltage() * 0.1f;

    if (nextFilter.process(params[FILTER_PRESET_PARAM].getValue())) {
        filterPreset = (filterPreset + 1) % SNES::kNumFilterPresets;
        memcpy(snes1.cfg.filterCoefficients, SNES::kFilterPresets[filterPreset].data, SNES::kFIRTaps);
        // snes1.cfg.filterGain = dbToRatio(-SNES::kFilterPresets[filterPreset].maxGainDb);
    }
    lights[FILTER_1_LIGHT].setBrightness(filterPreset & 1);
    lights[FILTER_2_LIGHT].setBrightness((filterPreset & 2) >> 1);
    lights[FILTER_3_LIGHT].setBrightness((filterPreset & 4) >> 2);
    lights[FILTER_4_LIGHT].setBrightness((filterPreset & 8) >> 3);

    size_t range = round(params[SIZE_RANGE_PARAM].getValue());
    if (range == 0) {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::kOriginalMaxEchoSamples;
    } else if (range == 1) {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::kExtremeMaxEchoSamples;
    } else {
        snes1.cfg.echoBufferRangeMaxSamples = SNES::MsToSamples(10000.0f);
    }

    float wetDryMix = clamp(params[MIX_PARAM].getValue() + inputs[MIX_CV_INPUT].getVoltage() * 0.1f, 0.0f, 1.0f);

    snes1.cfg.freezeEcho = (params[FREEZE_PARAM].getValue()) > 0.5f;
    freeze.process(inputs[FREEZE_GATE_INPUT].getVoltage(), .1f, 1.f);
    snes1.mod.freezeEcho = freeze.isHigh();

    // extension
    snes1.cfg.echoDelayMod = params[MOD_PARAM].getValue();
    snes1.mod.echoDelayMod = inputs[MOD_CV_INPUT].getVoltage() * 0.1f;

    snes1.cfg.filterMix = params[FILTER_MIX_PARAM].getValue();
    snes1.mod.filterMix = inputs[FILTER_MIX_CV_INPUT].getVoltage() * 0.1f;

    snes1.mod.clearBuffer =
        clear.process(params[CLEAR_PARAM].getValue() + inputs[CLEAR_TRIG_INPUT].getVoltage(), .1f, 1.f);
    snes1.mod.resetHead =
        clear.process(params[RESET_PARAM].getValue() + inputs[RESET_TRIG_INPUT].getVoltage(), .1f, 1.f);

    if (inputs[CLOCK_INPUT].isConnected()) {
        clockTime.process(args.sampleTime);
        if (clockIn.process(inputs[CLOCK_INPUT].getVoltage())) {
            snes1.cfg.echoBufferIncrementSamples = SNES::MsToSamples(clockTime.getTime() * 1000.0f);
            clockTime.reset();
        }
    } else {
        snes1.cfg.echoBufferIncrementSamples = SNES::kOriginalEchoIncrementSamples;
    }

    // processing
    float drySignal = inputs[AUDIO_INPUT].getVoltage() / 5.0f;
    float wetSignal = snesSampler.Process<kitdsp::interpolate::InterpolationStrategy::None>(
        drySignal, [this](float in, float& out) { out = snes1.Process(in * 0.5f) * 2.0f; });

    // outputs
    outputs[AUDIO_L_OUTPUT].setVoltage(5.0f * lerpf(drySignal, wetSignal, wetDryMix));
    outputs[AUDIO_R_OUTPUT].setVoltage(-5.0f * lerpf(drySignal, wetSignal, wetDryMix));
    */
}