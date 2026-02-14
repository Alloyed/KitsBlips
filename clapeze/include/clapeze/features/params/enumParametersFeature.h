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
#include "clapeze/impl/casts.h"
#include "clapeze/pluginHost.h"

namespace clapeze::params {

template <typename T>
concept ParamEnum = std::is_enum_v<T> && std::is_same_v<std::underlying_type_t<T>, clap_id>;

/** specialize by inheriting from baseparam */
template <ParamEnum TParamId, TParamId id>
struct ParamTraits;

template <ParamEnum TParamId>
class EnumProcessorHandle : public BaseProcessorHandle {
   public:
    EnumProcessorHandle(std::vector<std::unique_ptr<const BaseParam>>& ref,
                        size_t numParams,
                        Queue& mainToAudio,
                        Queue& audioToMain);

    template <class TParam>
    typename TParam::_valuetype Get(clap_id id) const;

    template <TParamId id>
    typename ParamTraits<TParamId, id>::_valuetype Get() const {
        using ParamType = ParamTraits<TParamId, id>;
        clap_id index = clapeze::impl::integral_cast<clap_id>(id);
        return Get<ParamType>(index);
    }

    template <TParamId id>
    void Send(typename ParamTraits<TParamId, id>::_valuetype in) {
        using ParamType = ParamTraits<TParamId, id>;
        clap_id index = clapeze::impl::integral_cast<clap_id>(id);
        double raw{};
        if (index < mValues.size()) {
            if (impl::down_cast<const ParamType*>(mParamsRef[index].get())->FromValue(in, raw)) {
                SetRawValue(index, raw);
                // tricksy: we're sending our own update from the audio thread to be processed in
                // FlushEventsFromMain. probably smarter to ask for the event queue and push ourselves in there
                // instead.
                mMainToAudio.push({ChangeType::SetValue, index, raw});
            }
        }
    }

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

class EnumMainHandle : public BaseMainHandle {
   public:
    EnumMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain);

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

template <ParamEnum TParamId>
class EnumParametersFeature : public BaseParametersFeature {
    // Reminder: to use Base anything you need to prefix with BaseType::!
    // yeah it's annoying sorry
    using BaseType = BaseParametersFeature;

   public:
    using ProcessorHandle = EnumProcessorHandle<TParamId>;
    using MainHandle = EnumMainHandle;
    using Id = TParamId;
    EnumParametersFeature(PluginHost& host, TParamId numParams) : BaseType(host, static_cast<clap_id>(numParams)) {
        mAudio.reset(new ProcessorHandle(mParams, mNumParams, mMainToAudio, mAudioToMain));
        mMain.reset(new MainHandle(mNumParams, mMainToAudio, mAudioToMain));
    }

    template <TParamId id>
    EnumParametersFeature& Parameter() {
        using ParamType = ParamTraits<TParamId, id>;
        clap_id index = clapeze::impl::integral_cast<clap_id>(id);
        ParamType* param = new ParamType();
        BaseType::mParams[index].reset(param);
        BaseType::mParamKeyToId.insert_or_assign(param->GetKey(), index);
        BaseType::mMain->SetRawValue(index, param->GetRawDefault());
        return *this;
    }

    template <TParamId id>
    const ParamTraits<TParamId, id>* GetSpecificParam() const {
        using TParam = ParamTraits<TParamId, id>;
        clap_id index = impl::integral_cast<clap_id>(id);
        return impl::down_cast<const TParam*>(BaseType::GetBaseParam(index));
    }
};

// impl
template <ParamEnum TParamId>
template <class TParam>
typename TParam::_valuetype EnumProcessorHandle<TParamId>::Get(clap_id id) const {
    typename TParam::_valuetype out{};
    clap_id index = id;
    if (index < mValues.size()) {
        double raw = GetRawValue(id);
        double mod = GetRawModulation(id);
        impl::down_cast<const TParam*>(mParamsRef[index].get())->ToValue(raw + mod, out);
    }
    return out;
}

template <ParamEnum TParamId>
EnumProcessorHandle<TParamId>::EnumProcessorHandle(std::vector<std::unique_ptr<const BaseParam>>& ref,
                                                   size_t numParams,
                                                   Queue& mainToAudio,
                                                   Queue& audioToMain)
    : mParamsRef(ref),
      mValues(numParams, 0.0f),
      mModulations(numParams, 0.0f),
      mMainToAudio(mainToAudio),
      mAudioToMain(audioToMain) {}

template <ParamEnum TParamId>
bool EnumProcessorHandle<TParamId>::ProcessEvent(const clap_event_header_t& event) {
    if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
        switch (event.type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const auto& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                SetRawValue(paramChange.param_id, paramChange.value);
                return true;
            }
            case CLAP_EVENT_PARAM_MOD: {
                const auto& paramChange = reinterpret_cast<const clap_event_param_mod_t&>(event);
                SetRawModulation(paramChange.param_id, paramChange.amount);
                return true;
            }
            default: {
                break;
            }
        }
    }
    return false;
}

template <ParamEnum TParamId>
void EnumProcessorHandle<TParamId>::FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) {
    (void)processor;
    // Send events queued from us to the host
    // Since these all happened on an independent thread, they do not have sample-accurate timing; we'll just
    // send them at the front of the queue.
    Change change;
    while (mMainToAudio.pop(change)) {
        switch (change.type) {
            case ChangeType::StartGesture:
            case ChangeType::StopGesture: {
                clap_event_param_gesture_t event = {};
                event.header.size = sizeof(event);
                event.header.time = 0;
                event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                event.header.type =
                    static_cast<uint16_t>(change.type == ChangeType::StartGesture ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                                  : CLAP_EVENT_PARAM_GESTURE_END);
                event.header.flags = 0;
                event.param_id = change.id;
                out->try_push(out, &event.header);
                break;
            }
            case ChangeType::SetValue: {
                clap_id index = change.id;
                mValues[index] = change.value;

                clap_event_param_value_t event = {};
                event.header.size = sizeof(event);
                event.header.time = 0;
                event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                event.header.type = static_cast<uint16_t>(CLAP_EVENT_PARAM_VALUE);
                event.header.flags = 0;
                event.param_id = index;
                event.cookie = nullptr;
                event.note_id = -1;
                event.port_index = -1;
                event.channel = -1;
                event.key = -1;
                event.value = change.value;

                out->try_push(out, &event.header);
                break;
            }
            case ChangeType::SetModulation: {
                // this is if the plugin sets the modulation, i don't think this happens??
                assert(false);
                break;
            }
        }
    }
}
template <ParamEnum TParamId>
double EnumProcessorHandle<TParamId>::GetRawValue(clap_id id) const {
    clap_id index = id;
    if (index < mValues.size()) {
        return mValues[index];
    }
    return 0.0f;
}
template <ParamEnum TParamId>
void EnumProcessorHandle<TParamId>::SetRawValue(clap_id id, double newValue) {
    clap_id index = id;
    if (index < mValues.size()) {
        mValues[index] = newValue;
        mAudioToMain.push({ChangeType::SetValue, id, newValue});
    }
}

template <ParamEnum TParamId>
double EnumProcessorHandle<TParamId>::GetRawModulation(clap_id id) const {
    clap_id index = id;
    if (index < mModulations.size()) {
        return mModulations[index];
    }
    return 0.0f;
}
template <ParamEnum TParamId>
void EnumProcessorHandle<TParamId>::SetRawModulation(clap_id id, double newModulation) {
    clap_id index = id;
    if (index < mModulations.size()) {
        mModulations[index] = newModulation;
        mAudioToMain.push({ChangeType::SetModulation, id, newModulation});
    }
}
}  // namespace clapeze::params
