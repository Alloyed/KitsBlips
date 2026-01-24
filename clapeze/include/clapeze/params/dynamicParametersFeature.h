#pragma once

#include <clap/clap.h>
#include <etl/queue_spsc_atomic.h>
#include <etl/span.h>
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>

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
    typename TParam::_valuetype Get(Id id) const;

    bool ProcessEvent(const clap_event_header_t& event);
    void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out);

   private:
    double GetRawValue(Id id) const;
    void SetRawValue(Id id, double newValue);

    double GetRawModulation(Id id) const;
    void SetRawModulation(Id id, double newModulation);

    std::vector<std::unique_ptr<const BaseParam>>& mParamsRef;
    std::vector<double> mValues;
    std::vector<double> mModulations;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
};

class DynamicMainHandle {
   public:
    DynamicMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain);

    double GetRawValue(Id id) const;
    void SetRawValue(Id id, double newValue);

    void FlushFromAudio();

   private:
    std::vector<double> mValues;
    std::vector<double> mModulations;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
};

class DynamicParametersFeature : public BaseParametersFeature<DynamicMainHandle, DynamicProcessorHandle> {
   public:
    DynamicParametersFeature(PluginHost& host, Id numParams) : BaseParametersFeature(host, numParams) {}

    DynamicParametersFeature& Parameter(clap_id id, BaseParam* param) {
        clap_id index = static_cast<clap_id>(id);
        param->SetModule(mNextModule);
        mParams[index].reset(param);
        mMain.SetRawValue(id, mParams[index]->GetRawDefault());
        return *this;
    }

    DynamicParametersFeature& Module(std::string_view moduleName) {
        mNextModule = moduleName;
        return *this;
    }

    std::string_view mNextModule = "";
};

// impl
template <class TParam>
typename TParam::_valuetype DynamicProcessorHandle::Get(Id id) const {
    typename TParam::_valuetype out{};
    clap_id index = static_cast<clap_id>(id);
    if (index < mValues.size()) {
        double raw = GetRawValue(id);
        double mod = GetRawModulation(id);
        static_cast<const TParam*>(mParamsRef[index].get())->ToValue(raw + mod, out);
    }
    return out;
}

}  // namespace clapeze::params