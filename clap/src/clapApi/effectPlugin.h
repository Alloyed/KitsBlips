#pragma once

#include "clapApi/common.h"
#include "clapApi/basePlugin.h"
#include "clapApi/ext/parameters.h"

/* pre-configured for simple stereo effects */
class EffectPlugin: public BasePlugin {
    public:
    // API
    EffectPlugin(PluginHost& host): BasePlugin(host) {}
    ~EffectPlugin() = default;
    virtual void Config() override;
    virtual void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out, ParametersExt::AudioParameters& params) = 0;

    // impl
    void ProcessEvent(const clap_event_header_t& event) final;
    void ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) final;
};