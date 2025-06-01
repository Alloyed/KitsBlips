#pragma once

#include <etl/vector.h>

#include "clapApi/common.h"
#include "clapApi/basePlugin.h"
#include "clapApi/ext/parameters.h"

/* pre-configured for simple stereo instruments */
class InstrumentPlugin: public BasePlugin {
    public:
    // API
    InstrumentPlugin(PluginHost& host): BasePlugin(host) {}
    virtual void Config() override;
    virtual void ProcessAudio(StereoAudioBuffer& out, ParametersExt::AudioParameters& params) = 0;
    virtual void ProcessNoteOn(const NoteTuple& note, float velocity) = 0;
    virtual void ProcessNoteOff(const NoteTuple& note) = 0;
    virtual void ProcessNoteChoke(const NoteTuple& note) = 0;
    virtual void ProcessNoteExpression(const NoteTuple& note, clap_note_expression expression, float value) {};

    // impl
    void ProcessEvent(const clap_event_header_t& event) final;
    void ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) final;
};