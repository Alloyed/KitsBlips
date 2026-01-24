#pragma once

#include <clap/clap.h>
#include <etl/queue_spsc_atomic.h>
#include <etl/span.h>
#include <fmt/format.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>

#include "clap/events.h"
#include "clap/ext/params.h"
#include "clapeze/basePlugin.h"
#include "clapeze/pluginHost.h"

namespace clapeze {

struct BaseParam {
   public:
    virtual ~BaseParam() = default;
    virtual bool FillInformation(clap_id id, clap_param_info_t* information) const = 0;
    virtual bool ToText(double rawValue, etl::span<char>& outTextBuf) const = 0;
    virtual bool FromText(std::string_view text, double& outRawValue) const = 0;
    virtual double GetRawDefault() const = 0;

    const std::string& GetModule() const { return mModule; }
    void SetModule(std::string_view module) { mModule = module; }
    std::string mModule;
};

/** specialize by inheriting from baseparam */
template <class TParamId, auto id>
    requires std::is_same_v<std::underlying_type_t<decltype(id)>, clap_id>
struct ParamTraits;

/*
 * TODO this is very complex and deserves to be subclassed a bit so that you can pick between a batteries included
 * "easy" interface, a complex but configurable "hard" interface, or just "here's the basics make your own thing"
 *
 * More features we should eventually support:
 * - per-voice modulation (assignable by note)
 * - observer architecture (onParamChanged())
 * - dynamic parameter modification
 */
template <typename TParamId>
    requires std::is_same_v<std::underlying_type_t<TParamId>, clap_id> || std::is_same_v<TParamId, clap_id>
class ParametersFeature : public BaseFeature {
   public:
    using Id = TParamId;

    using ParamConfigs = std::vector<std::unique_ptr<BaseParam>>;
    enum class ChangeType : uint8_t { SetValue, SetModulation, StartGesture, StopGesture };
    struct Change {
        ChangeType type;
        Id id;
        double value;
    };
    using Queue = etl::queue_spsc_atomic<Change, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

   public:
    static constexpr auto NAME = CLAP_EXT_PARAMS;
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override {
        static const clap_plugin_params_t value = {
            &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

    bool Validate(const BasePlugin& self) const override {
        for (size_t index = 0; index < mNumParams; index++) {
            if (mParams[index].get() == nullptr) {
                self.GetHost().Log(LogSeverity::Fatal, fmt::format("Missing configuration for parameter {}", index));
                return false;
            }
        }
        return true;
    }

    ParametersFeature(PluginHost& host, Id numParams)
        : mHost(host),
          mNumParams(static_cast<size_t>(numParams)),
          mParams(mNumParams),
          mValues(mNumParams, 0.0f),
          mModulations(mNumParams, 0.0f),
          mAudioToMain(),
          mMainToAudio(),
          mAudioState(mParams, mNumParams, mMainToAudio, mAudioToMain) {}

    template <Id id>
    ParametersFeature& Parameter() {
        using ParamType = ParamTraits<TParamId, id>;
        clap_id index = static_cast<clap_id>(id);
        mParams[index].reset(new ParamType());
        mParams[index]->SetModule(mNextModule);
        SetRawValue(id, mParams[index]->GetRawDefault());
        return *this;
    }

    ParametersFeature& Module(std::string_view moduleName) {
        mNextModule = moduleName;
        return *this;
    }

    const BaseParam* GetBaseParam(Id id) const {
        clap_id index = static_cast<clap_id>(id);
        if (index >= mValues.size()) {
            return nullptr;
        }
        return mParams[index].get();
    }

    template <Id id>
    const ParamTraits<TParamId, id>* GetSpecificParam() const {
        using TParam = ParamTraits<TParamId, id>;
        clap_id index = static_cast<clap_id>(id);
        if (index >= mValues.size()) {
            return nullptr;
        }
        return static_cast<const TParam*>(mParams[index].get());
    }

    double GetRawValue(Id id) const {
        clap_id index = static_cast<clap_id>(id);
        if (index >= mValues.size()) {
            return 0.0f;
        }
        return mValues[index];
    }

    void SetRawValue(Id id, double newValue) {
        clap_id index = static_cast<clap_id>(id);
        if (index < mValues.size()) {
            mValues[index] = newValue;
            mMainToAudio.push({ChangeType::SetValue, id, newValue});
        }
    }

    void StartGesture(Id id) { mMainToAudio.push({ChangeType::StartGesture, id, 0.0}); }

    void StopGesture(Id id) { mMainToAudio.push({ChangeType::StopGesture, id, 0.0}); }

    void RequestClear(Id id, clap_param_clear_flags flags = CLAP_PARAM_CLEAR_ALL) {
        const clap_host_t* rawHost = nullptr;
        const clap_host_params_t* rawHostParams = nullptr;
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            clap_id index = static_cast<clap_id>(id);
            rawHostParams->clear(rawHost, index, flags);
        }
    }

    void RequestRescan(clap_param_rescan_flags flags = CLAP_PARAM_RESCAN_ALL) {
        const clap_host_t* rawHost = nullptr;
        const clap_host_params_t* rawHostParams = nullptr;
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            rawHostParams->rescan(rawHost, flags);
        }
    }

    void FlushFromAudio() {
        Change change;
        while (mAudioToMain.pop(change)) {
            clap_id index = static_cast<clap_id>(change.id);
            switch (change.type) {
                case ChangeType::SetValue: {
                    mValues[index] = change.value;
                    break;
                }
                case ChangeType::SetModulation: {
                    mModulations[index] = change.value;
                    break;
                }
                case ChangeType::StartGesture:
                case ChangeType::StopGesture: {
                    break;
                }
            }
        }
    }

    void RequestFlushIfNotProcessing() {
        const clap_host_t* rawHost{};
        const clap_host_params_t* rawHostParams{};
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            rawHostParams->request_flush(rawHost);
        }
    }

    class ProcessParameters {
       public:
        ProcessParameters(const ParamConfigs& paramConfigs, size_t numParams, Queue& mainToAudio, Queue& audioToMain)
            : mParamsRef(paramConfigs),
              mValues(numParams, 0.0f),
              mModulations(numParams, 0.0f),
              mMainToAudio(mainToAudio),
              mAudioToMain(audioToMain) {}

        template <Id id>
        typename ParamTraits<TParamId, id>::_valuetype Get() const {
            using ParamType = ParamTraits<TParamId, id>;
            typename ParamType::_valuetype out{};
            clap_id index = static_cast<clap_id>(id);
            if (index < mValues.size()) {
                double raw = GetRawValue(id);
                double mod = GetRawModulation(id);
                static_cast<const ParamType*>(mParamsRef[index].get())->ToValue(raw + mod, out);
            }
            return out;
        }

        template <Id id>
        void Send(typename ParamTraits<TParamId, id>::_valuetype in) {
            using ParamType = ParamTraits<TParamId, id>;
            clap_id index = static_cast<clap_id>(id);
            double raw{};
            if (index < mValues.size()) {
                if (static_cast<const ParamType*>(mParamsRef[index].get())->FromValue(in, raw)) {
                    SetRawValue(id, raw);
                    // tricksy: we're sending our own update from the audio thread to be processed in
                    // FlushEventsFromMain. probably smarter to ask for the event queue and push ourselves in there
                    // instead.
                    mMainToAudio.push({ChangeType::SetValue, id, raw});
                }
            }
        }

        double GetRawValue(Id id) const {
            clap_id index = static_cast<clap_id>(id);
            if (index < mValues.size()) {
                return mValues[index];
            }
            return 0.0f;
        }
        void SetRawValue(Id id, double newValue) {
            clap_id index = static_cast<clap_id>(id);
            if (index < mValues.size()) {
                mValues[index] = newValue;
                mAudioToMain.push({ChangeType::SetValue, id, newValue});
            }
        }

        double GetRawModulation(Id id) const {
            clap_id index = static_cast<clap_id>(id);
            if (index < mModulations.size()) {
                return mModulations[index];
            }
            return 0.0f;
        }
        void SetRawModulation(Id id, double newModulation) {
            clap_id index = static_cast<clap_id>(id);
            if (index < mModulations.size()) {
                mModulations[index] = newModulation;
                mAudioToMain.push({ChangeType::SetModulation, id, newModulation});
            }
        }

        bool ProcessEvent(const clap_event_header_t& event) {
            if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
                switch (event.type) {
                    case CLAP_EVENT_PARAM_VALUE: {
                        const auto& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                        const Id id = static_cast<Id>(paramChange.param_id);
                        SetRawValue(id, paramChange.value);
                        return true;
                    }
                    case CLAP_EVENT_PARAM_MOD: {
                        const auto& paramChange = reinterpret_cast<const clap_event_param_mod_t&>(event);
                        const Id id = static_cast<Id>(paramChange.param_id);
                        SetRawModulation(id, paramChange.amount);
                        return true;
                    }
                    default: {
                        break;
                    }
                }
            }
            return false;
        }

        void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) {
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
                        event.header.type = static_cast<uint16_t>(change.type == ChangeType::StartGesture
                                                                      ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                      : CLAP_EVENT_PARAM_GESTURE_END);
                        event.header.flags = 0;
                        event.param_id = static_cast<clap_id>(change.id);
                        out->try_push(out, &event.header);
                        break;
                    }
                    case ChangeType::SetValue: {
                        clap_id index = static_cast<clap_id>(change.id);
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

        const ParamConfigs& mParamsRef;
        std::vector<double> mValues;
        std::vector<double> mModulations;
        Queue& mMainToAudio;
        Queue& mAudioToMain;
    };
    ProcessParameters& GetStateForAudioThread() { return mAudioState; }
    size_t GetNumParams() const { return mNumParams; }

    // internal state
   private:
    PluginHost& mHost;
    const size_t mNumParams;
    ParamConfigs mParams;
    std::vector<double> mValues;
    std::vector<double> mModulations;
    std::string_view mNextModule = "";
    Queue mAudioToMain;
    Queue mMainToAudio;
    ProcessParameters mAudioState;

    // impl
   private:
    static uint32_t _count(const clap_plugin_t* plugin) {
        ParametersFeature& self = ParametersFeature::GetFromPluginObject<ParametersFeature>(plugin);
        return static_cast<uint32_t>(self.mNumParams);
    }
    static bool _get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* information) {
        ParametersFeature& self = ParametersFeature::GetFromPluginObject<ParametersFeature>(plugin);

        const BaseParam* param = self.GetBaseParam(static_cast<Id>(index));
        if (param != nullptr) {
            return param->FillInformation(index, information);
        }
        return false;
    }

    static bool _get_value(const clap_plugin_t* plugin, clap_id id, double* value) {
        ParametersFeature& self = ParametersFeature::GetFromPluginObject<ParametersFeature>(plugin);
        // called from main thread
        if (id < self.mParams.size()) {
            self.FlushFromAudio();
            *value = self.GetRawValue(static_cast<Id>(id));
            return true;
        }
        return false;
    }

    static bool _value_to_text(const clap_plugin_t* plugin,
                               clap_id param_id,
                               double value,
                               char* buf,
                               uint32_t bufferSize) {
        ParametersFeature& self = ParametersFeature::GetFromPluginObject<ParametersFeature>(plugin);

        const BaseParam* param = self.GetBaseParam(static_cast<Id>(param_id));
        if (param != nullptr) {
            auto span = etl::span<char>(buf, bufferSize);
            return param->ToText(value, span);
        }
        return false;
    }

    static bool _text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* display, double* out) {
        ParametersFeature& self = ParametersFeature::GetFromPluginObject<ParametersFeature>(plugin);

        const BaseParam* param = self.GetBaseParam(static_cast<Id>(param_id));
        if (param != nullptr) {
            return param->FromText(display, *out);
        }
        return false;
    }

    static void _flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
        // called from audio thread when active(), main thread when inactive
        BasePlugin& basePlugin = BasePlugin::GetFromPluginObject(plugin);
        BaseProcessor& processor = basePlugin.GetProcessor();
        ParametersFeature& self = ParametersFeature::GetFromPlugin<ParametersFeature>(basePlugin);

        // Process events sent from the host to us
        if (in) {
            const uint32_t eventCount = in->size(in);
            for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
                processor.ProcessEvent(*in->get(in, eventIndex));
            }
        }

        self.mAudioState.FlushEventsFromMain(processor, out);
    }
};

using BaseParamsFeature = ParametersFeature<clap_id>;
}  // namespace clapeze