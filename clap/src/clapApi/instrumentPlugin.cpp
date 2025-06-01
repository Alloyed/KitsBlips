#include "clapApi/instrumentPlugin.h"
#include "clapApi/ext/all.h"

void InstrumentPlugin::ProcessEvent(const clap_event_header_t& event) {
    if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
        // clap events
        switch (event.type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const clap_event_param_value_t& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                ParametersExt::AudioParameters& params =
                    ParametersExt::GetFromPlugin<ParametersExt>(*this).GetStateForAudioThread();
                params.Set(paramChange.param_id, paramChange.value);
            } break;
            case CLAP_EVENT_NOTE_ON: {
                const clap_event_note_t& noteChange = reinterpret_cast<const clap_event_note_t&>(event);
                NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                ProcessNoteOn(note, static_cast<float>(noteChange.velocity));
            } break;
            case CLAP_EVENT_NOTE_OFF: {
                const clap_event_note_t& noteChange = reinterpret_cast<const clap_event_note_t&>(event);
                NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                ProcessNoteOff(note);
            } break;
            case CLAP_EVENT_NOTE_CHOKE: {
                const clap_event_note_t& noteChange = reinterpret_cast<const clap_event_note_t&>(event);
                NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                ProcessNoteChoke(note);
            } break;
            case CLAP_EVENT_NOTE_EXPRESSION: {
                const clap_event_note_expression_t& noteChange =
                    reinterpret_cast<const clap_event_note_expression_t&>(event);
                NoteTuple note{noteChange.note_id, noteChange.port_index, noteChange.channel, noteChange.key};
                ProcessNoteExpression(note, noteChange.expression_id, static_cast<float>(noteChange.value));
            } break;
        }
    }
}

void InstrumentPlugin::ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) {
    ParametersExt::AudioParameters& params =
        ParametersExt::GetFromPlugin<ParametersExt>(*this).GetStateForAudioThread();
    size_t numSamples = rangeStop - rangeStart;
    StereoAudioBuffer out{
        // outLeft
        {process.audio_outputs[0].data32[0] + rangeStart, numSamples},
        // outRight
        {process.audio_outputs[0].data32[1] + rangeStop, numSamples},
        // isOutLeftConstant
        false,
        // isOutRightConstant
        false,
    };
    // process audio from this frame
    ProcessAudio(out, params);
}

void InstrumentPlugin::Config() {
    ConfigExtension<NotePortsExt<1, 0>>();
    ConfigExtension<StereoAudioPortsExt<0, 1>>();
}