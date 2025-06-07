#pragma once

#include "clapApi/basePlugin.h"
#include "clapApi/common.h"
#include "clapApi/ext/audioPorts.h"

/* pre-configured for simple stereo effects */
template <typename PARAMS>
class EffectPlugin : public BasePlugin {
   public:
    // API
    EffectPlugin(PluginHost& host) : BasePlugin(host) {}
    ~EffectPlugin() = default;
    virtual void Config() override { ConfigExtension<StereoAudioPortsExt<1, 1>>(); }
    virtual void ProcessAudio(const StereoAudioBuffer& in,
                              StereoAudioBuffer& out,
                              typename PARAMS::AudioParameters& params) = 0;

    // impl
    void ProcessEvent(const clap_event_header_t& event) final {
        PARAMS& params = PARAMS::template GetFromPlugin<PARAMS>(*this);
        if(params.ProcessEvent(event))
        {
            return;
        }
    }
    void ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) final {
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
        typename PARAMS::AudioParameters& params = PARAMS::template GetFromPlugin<PARAMS>(*this).GetStateForAudioThread();
        // process audio from this frame
        ProcessAudio(in, out, params);
    }
};