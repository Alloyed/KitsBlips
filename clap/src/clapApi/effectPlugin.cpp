#include "clapApi/effectPlugin.h"
#include "clapApi/ext/all.h"
#include "effectPlugin.h"

void EffectPlugin::ProcessRaw(const clap_process_t* process) {
    ParametersExt::AudioParameters& params =
        static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(GetPluginObject(), CLAP_EXT_PARAMS))
            .GetStateForAudioThread();

    const uint32_t frameCount = process->frames_count;
    const uint32_t inputEventCount = process->in_events->size(process->in_events);
    uint32_t eventIndex = 0;
    uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;


    params.Flush();

    for (uint32_t frameIndex = 0; frameIndex < frameCount;) {
        // process events that occurred this frame
        while (eventIndex < inputEventCount && nextEventFrame == frameIndex) {
            const clap_event_header_t* event = process->in_events->get(process->in_events, eventIndex);
            if (event->time != frameIndex) {
                nextEventFrame = event->time;
                break;
            }

            ProcessEvent(*event);
            eventIndex++;

            if (eventIndex == inputEventCount) {
                nextEventFrame = frameCount;
                break;
            }
        }
        size_t sampleStart = frameIndex;
        size_t numSamples = nextEventFrame - sampleStart;
        const StereoAudioBuffer in{
            // inLeft
            {process->audio_inputs[0].data32[0] + sampleStart, numSamples},
            // inRight
            {process->audio_inputs[0].data32[1] + sampleStart, numSamples},
            // isInLeftConstant
            (process->audio_inputs[0].constant_mask & (1 << 0)) != 0,
            // isInRightConstant
            (process->audio_inputs[0].constant_mask & (1 << 1)) != 0,
        };
        StereoAudioBuffer out{
            // outLeft
            {process->audio_outputs[0].data32[0] + sampleStart, numSamples},
            // outRight
            {process->audio_outputs[0].data32[1] + sampleStart, numSamples},
            // isOutLeftConstant
            false,
            // isOutRightConstant
            false,
        };
        // process audio from this frame
        ProcessAudio(in, out, params);
        frameIndex = nextEventFrame;
    }
}

void EffectPlugin::ProcessEvent(const clap_event_header_t& event) {
    if(event.space_id == CLAP_CORE_EVENT_SPACE_ID)
    {
        // clap events
        switch(event.type)
        {
            case CLAP_EVENT_PARAM_VALUE:
            {
                const clap_event_param_value_t& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                ParametersExt::AudioParameters& params =
                    static_cast<ParametersExt&>(
                        BasePlugin::GetExtensionFromPluginObject(GetPluginObject(), CLAP_EXT_PARAMS))
                        .GetStateForAudioThread();
                params.Set(paramChange.param_id, static_cast<float>(paramChange.value));
            }
            break;
        }
    }
}

void EffectPlugin::Config() {
    ConfigExtension<StereoAudioPortsExt<1, 1>>();
}