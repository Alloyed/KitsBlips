#pragma once

#include <clap/clap.h>
#include <etl/queue_spsc_atomic.h>
#include <kitdsp/string.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "clap/events.h"
#include "clap/ext/params.h"
#include "clapApi/basePlugin.h"
#include "clapApi/pluginHost.h"

// using ParamId = clap_id;

template <typename ParamId>
class ParametersExt : public BaseExt {
   public:
    using Id = ParamId;
    struct ParameterConfig {
        ParamId id;
        double min;
        double max;
        double defaultValue;
        std::string_view name;
        std::string_view unit;
        double displayBase;
        double displayMultiplier;
        double displayOffset;
    };

    struct ParameterChange {
        ParamId id;
        double value;
    };
    using ParameterChangeQueue = etl::queue_spsc_atomic<ParameterChange, 100, etl::memory_model::MEMORY_MODEL_SMALL>;
    using GestureQueue = etl::queue_spsc_atomic<clap_event_param_gesture_t, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

   public:
    static constexpr auto NAME = CLAP_EXT_PARAMS;
    const char* Name() const override { return NAME; }

    const void* Extension() const override {
        static const clap_plugin_params_t value = {
            &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
        };
        return static_cast<const void*>(&value);
    }

    ParametersExt(PluginHost& host, ParamId numParams)
        : mHost(host),
          mNumParams(static_cast<size_t>(numParams)),
          mParams(mNumParams),
          mState(mNumParams, 0.0f),
          mAudioToMain(),
          mMainToAudio(),
          mMainToAudioGestures(),
          mAudioState(mNumParams, mAudioToMain) {}

    /* This intentionally mirrors the configParam method from VCV rack */
    ParametersExt& configParam(ParamId id,
                               double min,
                               double max,
                               double defaultValue,
                               const char* name = "",
                               const char* unit = "",
                               double displayBase = 0.0f,
                               double displayMultiplier = 1.0f,
                               double displayOffset = 0.0f) {
        clap_id index = static_cast<clap_id>(id);
        mParams[index] = {id, min, max, defaultValue, name, unit, displayBase, displayMultiplier, displayOffset};
        Set(id, defaultValue);
        return *this;
    }

    const ParameterConfig* GetConfig(ParamId id) const {
        clap_id index = static_cast<clap_id>(id);
        if (index >= mState.size()) {
            return nullptr;
        }
        return &(mParams[index]);
    }

    double Get(ParamId id) const {
        clap_id index = static_cast<clap_id>(id);
        if (index >= mState.size()) {
            return 0.0f;
        }
        return mState[index];
    }

    void Set(ParamId id, double newValue) {
        clap_id index = static_cast<clap_id>(id);
        if (index < mState.size()) {
            mState[index] = newValue;
            mMainToAudio.push({id, newValue});
        }
    }

    void StartGesture(ParamId id) {
        clap_event_param_gesture_t event;
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
        event.header.flags = 0;
        event.param_id = static_cast<clap_id>(id);

        mMainToAudioGestures.push(std::move(event));
    }

    void StopGesture(ParamId id) {
        clap_event_param_gesture_t event;
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_GESTURE_END;
        event.header.flags = 0;
        event.param_id = static_cast<clap_id>(id);

        mMainToAudioGestures.push(std::move(event));
    }

    void RequestClear(ParamId id, clap_param_clear_flags flags = CLAP_PARAM_CLEAR_ALL) {
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

    bool ProcessEvent(const clap_event_header_t& event) {
        if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
            switch (event.type) {
                case CLAP_EVENT_PARAM_VALUE: {
                    const clap_event_param_value_t& paramChange =
                        reinterpret_cast<const clap_event_param_value_t&>(event);
                    const ParamId id = static_cast<ParamId>(paramChange.param_id);
                    mAudioState.Set(id, paramChange.value);
                    return true;
                }
            }
        }
        return false;
    }

    void ProcessFlushFromMain(BasePlugin& basePlugin, const clap_input_events_t* in, const clap_output_events_t* out) {
        // called from audio thread when active(), main thread when inactive

        // Process events sent from the host to us
        if (in) {
            const uint32_t eventCount = in->size(in);
            for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
                basePlugin.ProcessEvent(*in->get(in, eventIndex));
            }
        }

        // Send events queued from us to the host
        if (out) {
            clap_event_param_gesture_t event;
            while (mMainToAudioGestures.pop(event)) {
                out->try_push(out, &event.header);
            }

            ParameterChange change;
            while (mMainToAudio.pop(change)) {
                clap_id index = static_cast<clap_id>(change.id);
                mAudioState.mState[index] = change.value;

                clap_event_param_value_t param = {};
                param.header.size = sizeof(param);
                param.header.time = 0;
                param.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                param.header.type = CLAP_EVENT_PARAM_VALUE;
                param.header.flags = 0;
                param.param_id = index;
                param.cookie = nullptr;
                param.note_id = -1;
                param.port_index = -1;
                param.channel = -1;
                param.key = -1;
                param.value = change.value;

                out->try_push(out, &param.header);
            }
        }
    }

    void FlushFromAudio() {
        ParameterChange change;
        while (mAudioToMain.pop(change)) {
            clap_id index = static_cast<clap_id>(change.id);
            mState[index] = change.value;
        }
    }

    void RequestFlushToHost() {
        const clap_host_t* rawHost;
        const clap_host_params_t* rawHostParams;
        if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
            rawHostParams->request_flush(rawHost);
        }
    }

    /* A Handle for accessing parameters from the audio thread */
    class AudioParameters {
       public:
        AudioParameters(size_t numParams, ParameterChangeQueue& audioToMain)
            : mState(numParams, 0.0f), mAudioToMain(audioToMain) {}

        double Get(ParamId id) const {
            clap_id index = static_cast<clap_id>(id);
            if (index >= mState.size()) {
                return 0.0f;
            }
            return mState[id];
        }
        void Set(ParamId id, double newValue) {
            clap_id index = static_cast<clap_id>(id);
            if (index < mState.size()) {
                mState[index] = newValue;
                mAudioToMain.push({id, newValue});
            }
        }

        std::vector<double> mState;
        ParameterChangeQueue& mAudioToMain;
    };
    AudioParameters& GetStateForAudioThread() { return mAudioState; }
    size_t GetNumParams() const { return mNumParams; }

    // internal state
   private:
    PluginHost& mHost;
    const size_t mNumParams;
    std::vector<ParameterConfig> mParams;
    std::vector<double> mState;
    ParameterChangeQueue mAudioToMain;
    ParameterChangeQueue mMainToAudio;
    GestureQueue mMainToAudioGestures; // TODO: combine with ParameterChangeQueue
    AudioParameters mAudioState;

    // impl
   private:
    static uint32_t _count(const clap_plugin_t* plugin) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        return self.mNumParams;
    }
    static bool _get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* information) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        if (index < self.mParams.size()) {
            const auto& cfg = self.mParams[index];
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            information->flags = CLAP_PARAM_IS_AUTOMATABLE;
            information->min_value = cfg.min;
            information->max_value = cfg.max;
            information->default_value = cfg.defaultValue;
            kitdsp::stringCopy(information->name, cfg.name);
            return true;
        }
        return false;
    }

    static bool _get_value(const clap_plugin_t* plugin, clap_id id, double* value) {
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        // called from main thread
        if (id < self.mParams.size()) {
            self.FlushFromAudio();
            *value = self.Get(static_cast<ParamId>(id));
            return true;
        }
        return false;
    }

    static bool _value_to_text(const clap_plugin_t* _plugin,
                               clap_id param_id,
                               double value,
                               char* display,
                               uint32_t size) {
        // uint32_t i = (uint32_t)id;
        // if (i >= P_COUNT)
        //     return false;
        // snprintf(display, size, "%f", value);
        return false;
    }

    static bool _text_to_value(const clap_plugin_t* _plugin, clap_id param_id, const char* display, double* value) {
        // TODO Implement this.
        return false;
    }

    static void _flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
        printf("_flush");
        // called from audio thread when active(), main thread when inactive
        BasePlugin& basePlugin = BasePlugin::GetFromPluginObject(plugin);
        ParametersExt& self = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);
        self.ProcessFlushFromMain(basePlugin, in, out);
    }
};