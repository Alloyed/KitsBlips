#pragma once

#include "clapeze/basePlugin.h"
#include "clapeze/common.h"
#include "clapeze/ext/audioPorts.h"
#include "clapeze/ext/state.h"
#include "clapeze/ext/timerSupport.h"
#include "clapeze/gui/imguiExt.h"
#include "clapeze/gui/imguiHelpers.h"

namespace clapeze {

template <typename ParamsType>
class EffectProcessor : public BaseProcessor {
   public:
    EffectProcessor(ParamsType& params) : BaseProcessor(), mParams(params) {}
    ~EffectProcessor() = default;

    void ProcessEvent(const clap_event_header_t& event) final {
        if (mParams.ProcessEvent(event)) {
            return;
        }
    }

    virtual void ProcessAudio(const StereoAudioBuffer& in, StereoAudioBuffer& out) = 0;
    // impl
    void ProcessFlush(const clap_process_t& process) final { mParams.FlushEventsFromMain(*this, process.out_events); }
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
        ProcessAudio(in, out);
    }

    ParamsType& mParams;
};

/* pre-configured for simple stereo effects */
class EffectPlugin : public BasePlugin {
   public:
    EffectPlugin(PluginHost& host) : BasePlugin(host) {}
    ~EffectPlugin() = default;

   protected:
#ifdef KITSBLIPS_ENABLE_GUI
    virtual void OnGui() {
        // Basic OnGui implementation. override as desired.
        BaseParamsExt& params = BaseParamsExt::GetFromPlugin<BaseParamsExt>(*this);
        ImGuiHelpers::displayParametersBasic(params);
    };
#endif
    virtual void Config() override {
        ConfigExtension<StereoAudioPortsExt<1, 1>>();
        ConfigExtension<StateExt>();
#ifdef KITSBLIPS_ENABLE_GUI
        TryConfigExtension<TimerSupportExt>(GetHost());
        TryConfigExtension<ImGuiExt>(GetHost(), ImGuiConfig{[this]() { this->OnGui(); }});
#endif
    }
};
}  // namespace clapeze