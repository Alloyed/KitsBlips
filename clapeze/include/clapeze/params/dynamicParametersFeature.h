#pragma once

#include <clap/clap.h>
#include <etl/queue_spsc_atomic.h>
#include <etl/span.h>
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "clap/events.h"
#include "clapeze/params/baseParametersFeature.h"
#include "clapeze/pluginHost.h"

namespace clapeze::params {
class DynamicParametersFeature;

class DynamicProcessorHandle {
   public:
    DynamicProcessorHandle(std::vector<std::unique_ptr<const BaseParam>>& ref,
                           size_t numParams,
                           Queue& mainToAudio,
                           Queue& audioToMain);

    template <class TParam>
    typename TParam::_valuetype Get(clap_id id) const;

    bool ProcessEvent(const clap_event_header_t& event);
    void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out);

   private:
    double GetRawValue(clap_id id) const;
    void SetRawValue(clap_id id, double newValue);

    double GetRawModulation(clap_id id) const;
    void SetRawModulation(clap_id id, double newModulation);

    std::vector<std::unique_ptr<const BaseParam>>& mParamsRef;
    std::vector<double> mValues;
    std::vector<double> mModulations;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
};

class DynamicMainHandle {
   public:
    DynamicMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain);

    double GetRawValue(clap_id id) const;
    void SetRawValue(clap_id id, double newValue);

    void StartGesture(clap_id id);
    void StopGesture(clap_id id);

    void FlushFromAudio();

   private:
    std::vector<double> mValues;
    std::vector<double> mModulations;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
};

class DynamicParametersFeature : public BaseParametersFeature<DynamicMainHandle, DynamicProcessorHandle> {
   public:
    DynamicParametersFeature(PluginHost& host, clap_id numParams) : BaseParametersFeature(host, numParams) {}

    DynamicParametersFeature& Parameter(clap_id id, BaseParam* param) {
        mParams[id].reset(param);
        mMain.SetRawValue(id, mParams[id]->GetRawDefault());
        return *this;
    }
};

// impl
template <class TParam>
typename TParam::_valuetype DynamicProcessorHandle::Get(clap_id id) const {
    typename TParam::_valuetype out{};
    if (id < mValues.size()) {
        double raw = GetRawValue(id);
        double mod = GetRawModulation(id);
        static_cast<const TParam*>(mParamsRef[id].get())->ToValue(raw + mod, out);
    }
    return out;
}

}  // namespace clapeze::params
