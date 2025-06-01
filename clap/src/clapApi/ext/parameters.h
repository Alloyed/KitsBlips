#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <array>
#include <etl/queue_spsc_atomic.h>

#include "clapApi/basePlugin.h"

using ParamId = clap_id;

struct ParameterConfig {
    ParamId id;
    float min;
    float max;
    float initial;
    const char* name;
    const char* unit;
    float displayBase;
    float displayMultiplier;
    float displayOffset;
};

struct ParameterChange {
    ParamId id;
    float value;
};
using ParameterChangeQueue = etl::queue_spsc_atomic<ParameterChange, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

class ParametersExt : public BaseExt {
   public:
    const char* Name() const override {
        return CLAP_EXT_PARAMS;
    }

    const void* Extension() const override {
        static const clap_plugin_params_t value = {
            &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
        };
        return static_cast<const void*>(&value);
    }

    ParametersExt(size_t numParams)
        : mNumParams(numParams),
          mParams(numParams),
          mState(numParams, 0.0f),
          mAudioToMain(),
          mMainToAudio(),
          mAudioState(numParams, mAudioToMain, mMainToAudio) {}

    /* This intentionally mirrors the configParam method from VCV rack */
    ParametersExt& configParam(ParamId id,
                     float min,
                     float max,
                     float defaultValue,
                     const char* name = "",
                     const char* unit = "",
                     float displayBase = 0.0f,
                     float displayMultiplier = 1.0f,
                     float displayOffset = 0.0f) {
        mParams[id] = { id, min, max, defaultValue, name, unit, displayBase, displayMultiplier, displayOffset };
        return *this;
    }

    float Get(ParamId id) const {
        if (id >= mState.size()) {
            return 0.0f;
        }
        return mState[id];
    }

    void Set(ParamId id, float newValue) {
        if (id < mState.size()) {
            mState[id] = newValue;
            mMainToAudio.push({id, newValue});
        }
    }

    void Flush() {
        ParameterChange change;
        while(mAudioToMain.pop(change))
        {
            mState[change.id] = change.value;
        }
    }

    /* A Handle for accessing parameters from the audio thread */
    class AudioParameters {
       public:
        AudioParameters(size_t numParams, ParameterChangeQueue& audioToMain, ParameterChangeQueue& mainToAudio)
            : mState(numParams, 0.0f), mAudioToMain(audioToMain), mMainToAudio(mainToAudio) {}

        float Get(ParamId id) const {
            if (id >= mState.size()) {
                return 0.0f;
            }
            return mState[id];
        }
        void Set(ParamId id, float newValue) {
            if (id < mState.size()) {
                mState[id] = newValue;
                mAudioToMain.push({id, newValue});
            }
        }
        void Flush() {
            ParameterChange change;
            while(mMainToAudio.pop(change))
            {
                mState[change.id] = change.value;
            }
        }

        std::vector<float> mState;
        ParameterChangeQueue& mAudioToMain;
        ParameterChangeQueue& mMainToAudio;
   };
   AudioParameters& GetStateForAudioThread() { return mAudioState; }
   size_t GetNumParams() const { return mNumParams; }

   // internal state
   private:
   const size_t mNumParams;
   std::vector<ParameterConfig> mParams;
   std::vector<float> mState;
   ParameterChangeQueue mAudioToMain;
   ParameterChangeQueue mMainToAudio;
   AudioParameters mAudioState;

   // impl
   private:
    static uint32_t _count(const clap_plugin_t* plugin) {
        ParametersExt& self =
            static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(plugin, CLAP_EXT_PARAMS));
        return self.mNumParams;
    }
    static bool _get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* information) {
        ParametersExt& self =
            static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(plugin, CLAP_EXT_PARAMS));
        if(index < self.mParams.size())
        {
            const auto& cfg = self.mParams[index];
            memset(information, 0, sizeof(clap_param_info_t));
            information->id = index;
            // These flags enable polyphonic modulation.
            information->flags = CLAP_PARAM_IS_AUTOMATABLE;
            information->min_value = cfg.min;
            information->max_value = cfg.max;
            information->default_value = cfg.initial;
            strcpy(information->name, cfg.name);
            return true;
        }
        return false;
    }

    static bool _get_value(const clap_plugin_t* plugin, clap_id id, double* value) {
        ParametersExt& self = static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(plugin, CLAP_EXT_PARAMS));
        // called from main thread
        float valueFloat = self.Get(id);
        *value = static_cast<double>(valueFloat);
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
        // called from audio thread when active(), main thread when inactive
        BasePlugin& basePlugin = BasePlugin::GetFromPluginObject(plugin);
        ParametersExt& self =
            static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(plugin, CLAP_EXT_PARAMS));
        self.mAudioState.Flush();

        // Process events sent to our plugin from the host.
        const uint32_t eventCount = in->size(in);
        for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
            basePlugin.ProcessEvent(*in->get(in, eventIndex));
        }
    }
};