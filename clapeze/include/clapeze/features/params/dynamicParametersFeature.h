#pragma once

#include <clap/clap.h>
#include <etl/flat_map.h>
#include <etl/queue_spsc_atomic.h>
#include <etl/span.h>
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "clap/events.h"
#include "clapeze/common.h"
#include "clapeze/features/params/baseParametersFeature.h"
#include "clapeze/impl/casts.h"
#include "clapeze/pluginHost.h"

namespace clapeze::params {
class DynamicParametersFeature;

class DynamicAudioHandle : public BaseAudioHandle {
   public:
    DynamicAudioHandle(std::vector<std::unique_ptr<BaseParam>>& ref,
                       size_t numParams,
                       Queue& mainToAudio,
                       Queue& audioToMain);

    void RegisterHandler(std::function<void(clap_id)> handler) { mHandleChange = std::move(handler); }

    template <class TParam>
    typename TParam::_valuetype Get(clap_id id) const;
    template <class TParam>
    typename TParam::_valuetype Get(clap_id id, const NoteTuple& note) const;

    void SetModulationMask(const NoteTuple& note);
    void ClearModulation(const NoteTuple& note);
    void OnNoteStart(const NoteTuple& note) override;
    void OnNoteEnd(const NoteTuple& note) override;

    bool ProcessEvent(const clap_event_header_t& event) override;
    void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) override;
    void NotifyAllChanged() {
        if (mHandleChange) {
            for (clap_id idx = 0; idx < mValues.size(); ++idx) {
                mHandleChange(idx);
            }
        }
    }
    const std::string& GetKey(clap_id id) { return mParamsRef[id]->GetKey(); }
    double GetRawValue(clap_id id) const;

   private:
    void SetRawValue(clap_id id, double newValue);

    double GetRawModulation(clap_id id, const std::optional<NoteTuple>& note) const;
    void SetRawModulation(clap_id id, const NoteTuple& note, double newModulation);

    std::vector<std::unique_ptr<BaseParam>>& mParamsRef;
    std::vector<double> mValues;
    etl::flat_map<etl::pair<clap_id, NoteTuple>, double, 255> mModulations;
    etl::flat_map<NoteTuple, size_t, 64> mActiveNotes;
    std::function<void(clap_id)> mHandleChange;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
    NoteTuple mModulationMask{};
};

class DynamicMainHandle : public BaseMainHandle {
   public:
    DynamicMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain);

    void RegisterHandler(std::function<void(clap_id)> handler) { mHandleChange = std::move(handler); }

    double GetRawValue(clap_id id) const override;
    void SetRawValue(clap_id id, double newValue) override;

    void StartGesture(clap_id id) override;
    void StopGesture(clap_id id) override;

    void FlushFromAudio() override;
    void NotifyAllChanged() {
        if (mHandleChange) {
            for (clap_id idx = 0; idx < mValues.size(); ++idx) {
                mHandleChange(idx);
            }
        }
    }

   private:
    std::vector<double> mValues;
    std::vector<double> mModulations;
    std::function<void(clap_id)> mHandleChange;
    Queue& mMainToAudio;
    Queue& mAudioToMain;
};

class DynamicParametersFeature : public BaseParametersFeature {
   public:
    using AudioHandle = DynamicAudioHandle;
    using MainHandle = DynamicMainHandle;
    DynamicParametersFeature(PluginHost& host, clap_id numParams) : BaseParametersFeature(host, numParams) {
        mAudio.reset(new DynamicAudioHandle(mParams, mNumParams, mMainToAudio, mAudioToMain));
        mMain.reset(new DynamicMainHandle(mNumParams, mMainToAudio, mAudioToMain));
    }

    DynamicParametersFeature& Parameter(clap_id id, BaseParam* param) {
        mParams[id].reset(param);
        mParamKeyToId.insert_or_assign(param->GetKey(), id);
        mMain->SetRawValue(id, mParams[id]->GetRawDefault());
        return *this;
    }

    void OnActivated() override { impl::down_cast<DynamicAudioHandle*>(mAudio.get())->NotifyAllChanged(); }
};

// impl
template <class TParam>
typename TParam::_valuetype DynamicAudioHandle::Get(clap_id id) const {
    typename TParam::_valuetype out{};
    if (id < mValues.size()) {
        double raw = GetRawValue(id);
        double mod = GetRawModulation(id, mModulationMask);
        impl::down_cast<const TParam*>(mParamsRef[id].get())->ToValue(raw + mod, out);
    }
    return out;
}

template <class TParam>
typename TParam::_valuetype DynamicAudioHandle::Get(clap_id id, const NoteTuple& note) const {
    typename TParam::_valuetype out{};
    if (id < mValues.size()) {
        double raw = GetRawValue(id);
        double mod = GetRawModulation(id, note);
        impl::down_cast<const TParam*>(mParamsRef[id].get())->ToValue(raw + mod, out);
    }
    return out;
}

}  // namespace clapeze::params
