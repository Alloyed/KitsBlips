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
#include "clapeze/features/params/baseParametersFeature.h"
#include "clapeze/pluginHost.h"

namespace clapeze::params {
class DynamicParametersFeature;

class DynamicProcessorHandle : public BaseProcessorHandle {
   public:
    DynamicProcessorHandle(std::vector<std::unique_ptr<const BaseParam>>& ref,
                           size_t numParams,
                           Queue& mainToAudio,
                           Queue& audioToMain);

    template <class TParam>
    typename TParam::_valuetype Get(clap_id id) const;

    bool ProcessEvent(const clap_event_header_t& event) override;
    void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) override;

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

class DynamicMainHandle : public BaseMainHandle {
   public:
    DynamicMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain);

    double GetRawValue(clap_id id) const override;
    void SetRawValue(clap_id id, double newValue) override;

    void StartGesture(clap_id id) override;
    void StopGesture(clap_id id) override;

    void FlushFromAudio() override;

   private:
    std::vector<double> mValues;
    std::vector<double> mModulations;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
};

class DynamicParametersFeature : public BaseParametersFeature {
   public:
    using ProcessorHandle = DynamicProcessorHandle;
    using MainHandle = DynamicMainHandle;
    DynamicParametersFeature(PluginHost& host, clap_id numParams) : BaseParametersFeature(host, numParams) {
        mAudio.reset(new DynamicProcessorHandle(mParams, mNumParams, mMainToAudio, mAudioToMain));
        mMain.reset(new DynamicMainHandle(mNumParams, mMainToAudio, mAudioToMain));
    }

    DynamicParametersFeature& Parameter(clap_id id, BaseParam* param) {
        mParams[id].reset(param);
        mParamKeyToId.insert_or_assign(param->GetKey(), id);
        mMain->SetRawValue(id, mParams[id]->GetRawDefault());
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
        impl::down_cast<const TParam*>(mParamsRef[id].get())->ToValue(raw + mod, out);
    }
    return out;
}

}  // namespace clapeze::params
