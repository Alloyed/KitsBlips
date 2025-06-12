#pragma once

#include <clap/clap.h>
#include <etl/queue_spsc_atomic.h>
#include <kitdsp/string.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <variant>

#include "clap/events.h"
#include "clap/ext/params.h"
#include "clapeze/basePlugin.h"
#include "clapeze/pluginHost.h"


namespace ParamHelpers {
inline double toDisplay(double rawValue, double displayBase, double displayMultiplier, double displayOffset) {
    return (rawValue * displayMultiplier) + displayOffset;
}
inline double fromDisplay(double displayValue, double displayBase, double displayMultiplier, double displayOffset) {
    return (displayValue - displayOffset) / displayMultiplier;
}
}  // namespace ParamHelpers

/*
 * TODO this is very complex and deserves to be subclassed a bit so that you can pick between a batteries included
 * "easy" interface, a complex but configurable "hard" interface, or just "here's the basics make your own thing"
 */
template <typename PARAM_ID>
class ParametersExt : public BaseExt {
   public:
    using Id = PARAM_ID;
    struct NumericParam {
        Id id;
        double min;
        double max;
        double defaultValue;
        std::string_view name;
        std::string_view unit;
        double displayBase;
        double displayMultiplier;
        double displayOffset;
        clap_param_info_flags flags;
    };
    struct EnumParam {
        Id id;
        std::vector<std::string_view> labels;
        std::string_view name;
        size_t defaultValue;
    };
    using ParameterConfig = std::variant<NumericParam, EnumParam>;

    enum class ChangeType { SetValue, StartGesture, StopGesture };
    struct Change {
        ChangeType type;
        Id id;
        double value;
    };
    using Queue = etl::queue_spsc_atomic<Change, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

   public:
    static constexpr auto NAME = CLAP_EXT_PARAMS;
    const char* Name() const override { return NAME; }

    const void* Extension() const override {
        static const clap_plugin_params_t value = {
            &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
        };
        return static_cast<const void*>(&value);
    }

    ParametersExt(PluginHost& host, Id numParams)
        : mHost(host),
          mNumParams(static_cast<size_t>(numParams)),
          mParams(mNumParams),
          mState(mNumParams, 0.0f),
          mAudioToMain(),
          mMainToAudio(),
          mAudioState(mNumParams, mMainToAudio, mAudioToMain) {}

    /* This intentionally mirrors the configParam method from VCV rack */
    ParametersExt& configNumeric(Id id,
                                 double min,
                                 double max,
                                 double defaultValue,
                                 const char* name = "",
                                 const char* unit = "",
                                 double displayBase = 0.0f,
                                 double displayMultiplier = 1.0f,
                                 double displayOffset = 0.0f,
                                 clap_param_info_flags flags = 0) {
        clap_id index = static_cast<clap_id>(id);
        if(max < min)
        {
            std::swap(max, min);
        }
        mParams[index] =
            NumericParam{id, min, max, defaultValue, name, unit, displayBase, displayMultiplier, displayOffset, flags};
        Set(id, defaultValue);
        return *this;
    }

    ParametersExt& configCount(Id id,
                               double min,
                               double max,
                               double defaultValue,
                               const char* name,
                               const char* unit = "") {
        return configNumeric(id, min, max, defaultValue, name, unit, 0.0f, 1.0f, 0.0f, CLAP_PARAM_IS_STEPPED);
    }

    ParametersExt& configSwitch(Id id,
                                std::vector<std::string_view>&& labels,
                                size_t defaultValue,
                                const char* name = "") {
        clap_id index = static_cast<clap_id>(id);
        mParams[index] = EnumParam{id, std::move(labels), defaultValue, name};
        Set(id, defaultValue);
        return *this;
    }

    const ParameterConfig* GetConfig(Id id) const {
        clap_id index = static_cast<clap_id>(id);
        if (index >= mState.size()) {
            return nullptr;
        }
        return &(mParams[index]);
    }

    double Get(Id id) const {
        clap_id index = static_cast<clap_id>(id);
        if (index >= mState.size()) {
            return 0.0f;
        }
        return mState[index];
    }

    void Set(Id id, double newValue) {
        clap_id index = static_cast<clap_id>(id);
        if (index < mState.size()) {
            mState[index] = newValue;
            mMainToAudio.push({ChangeType::SetValue, id, newValue});
        }
    }

    void StartGesture(Id id) { mMainToAudio.push({ChangeType::StartGesture, id, 0.0}); }

    void StopGesture(Id id) { mMainToAudio.push({ChangeType::StopGesture, id, 0.0}); }

    void RequestClear(Id id, clap_param_clear_flags flags = CLAP_PARAM_CLEAR_ALL) {
        const clap_host_t* rawHost;
        const clap_host_params_t* rawHostParams;
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            clap_id index = static_cast<clap_id>(id);
            rawHostParams->clear(rawHost, index, flags);
        }
    }

    void RequestRescan(clap_param_rescan_flags flags = CLAP_PARAM_RESCAN_ALL) {
        const clap_host_t* rawHost;
        const clap_host_params_t* rawHostParams;
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            rawHostParams->rescan(rawHost, flags);
        }
    }

    void FlushFromAudio() {
        Change change;
        while (mAudioToMain.pop(change)) {
            clap_id index = static_cast<clap_id>(change.id);
            mState[index] = change.value;
        }
    }

    void RequestFlushIfNotProcessing() {
        const clap_host_t* rawHost;
        const clap_host_params_t* rawHostParams;
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            rawHostParams->request_flush(rawHost);
        }
    }

    class ProcessParameters {
       public:
        ProcessParameters(size_t numParams, Queue& mainToAudio, Queue& audioToMain)
            : mState(numParams, 0.0f), mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}

        double Get(Id id) const {
            clap_id index = static_cast<clap_id>(id);
            if (index >= mState.size()) {
                return 0.0f;
            }
            return mState[index];
        }
        void Set(Id id, double newValue) {
            clap_id index = static_cast<clap_id>(id);
            if (index < mState.size()) {
                mState[index] = newValue;
                mAudioToMain.push({ChangeType::SetValue, id, newValue});
            }
        }

        bool ProcessEvent(const clap_event_header_t& event) {
            if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
                switch (event.type) {
                    case CLAP_EVENT_PARAM_VALUE: {
                        const clap_event_param_value_t& paramChange =
                            reinterpret_cast<const clap_event_param_value_t&>(event);
                        const Id id = static_cast<Id>(paramChange.param_id);
                        Set(id, paramChange.value);
                        return true;
                    }
                }
            }
            return false;
        }

        void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) {
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
                        event.header.type = change.type == ChangeType::StartGesture ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                                    : CLAP_EVENT_PARAM_GESTURE_END;
                        event.header.flags = 0;
                        event.param_id = static_cast<clap_id>(change.id);
                        out->try_push(out, &event.header);
                        break;
                    }
                    case ChangeType::SetValue: {
                        clap_id index = static_cast<clap_id>(change.id);
                        mState[index] = change.value;

                        clap_event_param_value_t event = {};
                        event.header.size = sizeof(event);
                        event.header.time = 0;
                        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                        event.header.type = CLAP_EVENT_PARAM_VALUE;
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
                }
            }
        }

        std::vector<double> mState;
        Queue& mMainToAudio;
        Queue& mAudioToMain;
    };
    ProcessParameters& GetStateForAudioThread() { return mAudioState; }
    size_t GetNumParams() const { return mNumParams; }

    // internal state
   private:
    PluginHost& mHost;
    const size_t mNumParams;
    std::vector<ParameterConfig> mParams;
    std::vector<double> mState;
    Queue mAudioToMain;
    Queue mMainToAudio;
    ProcessParameters mAudioState;

    // impl
   private:
    static uint32_t _count(const clap_plugin_t* plugin) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        return self.mNumParams;
    }
    static bool _get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* information) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        if (index < self.mParams.size()) {
            std::visit(
                [information, index](auto&& cfg) {
                    using T = std::decay_t<decltype(cfg)>;
                    if constexpr (std::is_same_v<T, NumericParam>) {
                        memset(information, 0, sizeof(clap_param_info_t));
                        information->id = index;
                        information->flags = CLAP_PARAM_IS_AUTOMATABLE;
                        information->min_value = cfg.min;
                        information->max_value = cfg.max;
                        information->default_value = cfg.defaultValue;
                        kitdsp::stringCopy(information->name, cfg.name);
                    } else if constexpr (std::is_same_v<T, EnumParam>) {
                        memset(information, 0, sizeof(clap_param_info_t));
                        information->id = index;
                        information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_STEPPED;
                        information->min_value = 0;
                        information->max_value = cfg.labels.size();
                        information->default_value = cfg.defaultValue;
                        kitdsp::stringCopy(information->name, cfg.name);
                    } else {
                        static_assert(false, "non-exhaustive visitor!");
                    }
                },
                self.mParams[index]);
            return true;
        }
        return false;
    }

    static bool _get_value(const clap_plugin_t* plugin, clap_id id, double* value) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        // called from main thread
        if (id < self.mParams.size()) {
            self.FlushFromAudio();
            *value = self.Get(static_cast<Id>(id));
            return true;
        }
        return false;
    }

    static bool _value_to_text(const clap_plugin_t* plugin,
                               clap_id param_id,
                               double value,
                               char* buf,
                               uint32_t bufferSize) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        if (param_id < self.mParams.size()) {
            std::visit(
                [&](auto&& cfg) {
                    using T = std::decay_t<decltype(cfg)>;
                    if constexpr (std::is_same_v<T, NumericParam>) {
                        snprintf(
                            buf, bufferSize, "%f%s",
                            ParamHelpers::toDisplay(value, cfg.displayBase, cfg.displayMultiplier, cfg.displayOffset),
                            cfg.unit.data());
                    } else if constexpr (std::is_same_v<T, EnumParam>) {
                        size_t index = static_cast<size_t>(value);
                        if (index < cfg.labels.size()) {
                            snprintf(buf, bufferSize, "%s", cfg.labels[index]);
                        }
                    } else {
                        static_assert(false, "non-exhaustive visitor!");
                    }
                },
                self.mParams[param_id]);
            return true;
        }
        return false;
    }

    static bool _text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* display, double* out) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        if (param_id < self.mParams.size()) {
            std::visit(
                [&](auto&& cfg) {
                    using T = std::decay_t<decltype(cfg)>;
                    if constexpr (std::is_same_v<T, NumericParam>) {
                        double in = std::strtod(display, nullptr);
                        *out = ParamHelpers::fromDisplay(in, cfg.displayBase, cfg.displayMultiplier, cfg.displayOffset);
                    } else if constexpr (std::is_same_v<T, EnumParam>) {
                        for (size_t index = 0; index < cfg.labels.size(); ++index) {
                            // TODO: trim whitespace, do case-insensitive compare, etc
                            if (cfg.labels[index] == display) {
                                *out = static_cast<double>(index);
                            }
                        }
                    } else {
                        static_assert(false, "non-exhaustive visitor!");
                    }
                },
                self.mParams[param_id]);
            return true;
        }
        return false;
    }

    static void _flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
        // called from audio thread when active(), main thread when inactive
        BasePlugin& basePlugin = BasePlugin::GetFromPluginObject(plugin);
        BaseProcessor& processor = basePlugin.GetProcessor();
        ParametersExt& self = ParametersExt::GetFromPlugin<ParametersExt>(basePlugin);

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

using BaseParamsExt = ParametersExt<clap_id>;