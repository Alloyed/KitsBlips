#pragma once

#include <etl/vector.h>

#include "clapApi/basePlugin.h"
#include "clapApi/ext/parameters.h"

struct StereoAudioBuffer
{
    etl::vector_ext<float> left;
    etl::vector_ext<float> right;
    bool isLeftConstant;
    bool isRightConstant;
};

/* pre-configured for simple stereo effects */
class EffectPlugin: public BasePlugin {
    public:
    // API
    EffectPlugin(PluginHost& host): BasePlugin(host) {}
    virtual void Config() override;
    virtual void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out, ParametersExt::AudioParameters& params) = 0;

    // helpers

    // impl
    void ProcessEvent(const clap_event_header_t& event) override;
    void ProcessRaw(const clap_process_t *process) override;
};