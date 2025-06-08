#pragma once

#include <etl/vector.h>

#include "clapeze/basePlugin.h"
#include "clapeze/common.h"

#include "clapeze/ext/audioPorts.h"
#include "clapeze/ext/notePorts.h"

/* pre-configured for simple stereo instruments */
template <typename PARAMS>
class InstrumentPlugin : public BasePlugin {
   public:
    // API
    InstrumentPlugin(PluginHost& host) : BasePlugin(host) {}
    ~InstrumentPlugin() = default;
    virtual void ProcessAudio(StereoAudioBuffer& out, typename PARAMS::AudioParameters& params) = 0;
    virtual void ProcessNoteOn(const NoteTuple& note, float velocity) = 0;
    virtual void ProcessNoteOff(const NoteTuple& note) = 0;
    virtual void ProcessNoteChoke(const NoteTuple& note) = 0;
    virtual void ProcessNoteExpression(const NoteTuple& note, clap_note_expression expression, float value) {};

    // impl
    virtual void Config() override {
        ConfigExtension<NotePortsExt<1, 0>>();
        ConfigExtension<StereoAudioPortsExt<0, 1>>();
    }
    void ProcessFlush(const clap_process_t& process) final {
        PARAMS& params = PARAMS::template GetFromPlugin<PARAMS>(*this);
        params.ProcessFlushFromMain(*this, nullptr, process.out_events);
    }
    void ProcessEvent(const clap_event_header_t& event) final {
        PARAMS& params = PARAMS::template GetFromPlugin<PARAMS>(*this);
        if (params.ProcessEvent(event)) {
            return;
        }

        if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
            // clap events
            switch (event.type) {
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
    void ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) final {
        typename PARAMS::AudioParameters& params =
            PARAMS::template GetFromPlugin<PARAMS>(*this).GetStateForAudioThread();
        size_t numSamples = rangeStop - rangeStart;
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
        // process audio from this frame
        ProcessAudio(out, params);
    }
};