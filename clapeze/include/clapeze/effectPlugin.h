#pragma once

#include "clap/events.h"
#include "clapeze/basePlugin.h"
#include "clapeze/common.h"
#include "clapeze/features/stereoAudioPortsFeature.h"
#include "clapeze/processor/transport.h"

namespace clapeze {

template <typename TParametersFeature>
class EffectProcessor : public BaseProcessor {
   public:
    explicit EffectProcessor(TParametersFeature& params) : BaseProcessor(), mParams(params) {}
    ~EffectProcessor() = default;

    void ProcessEvent(const clap_event_header_t& event) final {
        if (mParams.ProcessEvent(event)) {
            return;
        }

        if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
            // clap events
            switch (event.type) {
                case CLAP_EVENT_TRANSPORT: {
                    const auto& transport = reinterpret_cast<const clap_event_transport_t&>(event);
                    mTransport.ProcessEvent(transport);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

    virtual ProcessStatus ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) = 0;

    const Transport& GetTransport() const { return mTransport; }

    // impl
    void ProcessFlush(const clap_process_t& process) final {
        if (process.transport) {
            mTransport.ProcessEvent(*process.transport);
        }
        mParams.FlushEventsFromMain(*this, process.out_events);
    }
    ProcessStatus ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) final {
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
        return ProcessAudio(in, out);
    }

    TParametersFeature& mParams;
    Transport mTransport{};
};

/* pre-configured for simple stereo effects */
class EffectPlugin : public BasePlugin {
   public:
    explicit EffectPlugin(const clap_plugin_descriptor_t& meta) : BasePlugin(meta) {}
    ~EffectPlugin() = default;

   protected:
    void Config() override { ConfigFeature<StereoAudioPortsFeature>(1, 1); }
};
}  // namespace clapeze
