#include "clapApi/effectPlugin.h"
#include "clapApi/ext/parameters.h"
#include "clapApi/ext/audioPorts.h"

void EffectPlugin::ProcessEvent(const clap_event_header_t& event) {
    if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
        // clap events
        switch (event.type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const clap_event_param_value_t& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                ParametersExt::AudioParameters& params =
                    ParametersExt::GetFromPlugin<ParametersExt>(*this).GetStateForAudioThread();
                params.Set(paramChange.param_id, paramChange.value);
            } break;
        }
    }
}

void EffectPlugin::ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) {
    size_t numSamples = rangeStop - rangeStart;
    const StereoAudioBuffer in{
        // inLeft
        {process.audio_inputs[0].data32[0] + rangeStart, numSamples},
        // inRight
        {process.audio_inputs[0].data32[1] + rangeStart, numSamples},
        // isInLeftConstant
        (process.audio_inputs[0].constant_mask & (1 << 0)) != 0,
        // isInRightConstant
        (process.audio_inputs[0].constant_mask & (1 << 1)) != 0,
    };
    StereoAudioBuffer out{
        // outLeft
        {process.audio_outputs[0].data32[0] + rangeStart, numSamples},
        // outRight
        {process.audio_outputs[0].data32[1] + rangeStart, numSamples},
        // isOutLeftConstant
        false,
        // isOutRightConstant
        false,
    };
    ParametersExt::AudioParameters& params =
        ParametersExt::GetFromPlugin<ParametersExt>(*this).GetStateForAudioThread();
    // process audio from this frame
    ProcessAudio(in, out, params);
}

void EffectPlugin::Config() {
    ConfigExtension<StereoAudioPortsExt<1, 1>>();
}