#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <array>

#include "clapApi/basePlugin.h"

using ParamId = clap_id;

struct ParameterConfig {
    ParamId id;
    float min;
    float max;
    float initial;
    std::string name;
    std::string unit;
    float displayBase;
    float displayMultiplier;
    float displayOffset;
};

template<size_t NUM_PARAMS>
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

    /* This intentionally mirrors the configParam method from VCV rack */
    void configParam(ParamId id,
                     float min,
                     float max,
                     float default,
                     std::string_view name = "",
                     std::string_view unit = "",
                     float displayBase = 0.0f,
                     float displayMultiplier = 1.0f,
                     float displayOffset = 0.0f) {
        mParams[id] = { id, min, max, initial, name, unit, displayBase, displayMultiplier, displayOffset };
    }

   // internal state
   private:
   std::array<ParameterConfig, NUM_PARAMS> mParams;
   std::array<double, NUM_PARAMS> mParamState;

   // impl
   private:
    static uint32_t _count(const clap_plugin_t* plugin) { return NUM_PARAMS; }
    static bool _get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* information) {
        ParametersExt& self = static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(plugin, CLAP_EXT_PARAMS));
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
            strcpy(information->name, cfg.name.c_str());
            return true;
        }
        return false;
    }

    static bool _get_value(const clap_plugin_t* plugin, clap_id id, double* value) {
        ParametersExt& self = static_cast<ParametersExt&>(BasePlugin::GetExtensionFromPluginObject(plugin, CLAP_EXT_PARAMS));
        // get_value is called on the main thread, but should return the value of the parameter according to the
        // audio thread, since the value on the audio thread is the one that host communicates with us via
        // CLAP_EVENT_PARAM_VALUE events. Since we're accessing the opposite thread's arrays, we must acquire the
        // syncParameters mutex. And although we need to check the mainChanged array, we mustn't actually modify the
        // parameters array, since that can only be done on the audio thread. Don't worry -- it'll pick up the
        // changes eventually.
        // MutexAcquire(plugin->syncParameters);
        //*value = plugin->mainChanged[i] ? plugin->mainParameters[i] : plugin->parameters[i];
        // MutexRelease(plugin->syncParameters);
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

    static void _flush(const clap_plugin_t* _plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
        // MyPlugin* plugin = (MyPlugin*)_plugin->plugin_data;
        // const uint32_t eventCount = in->size(in);

        //// For parameters that have been modified by the main thread, send CLAP_EVENT_PARAM_VALUE events to the
        //// host.
        // PluginSyncMainToAudio(plugin, out);

        //// Process events sent to our plugin from the host.
        // for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
        //     PluginProcessEvent(plugin, in->get(in, eventIndex));
        // }
    }
};