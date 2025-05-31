#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>
#include <memory>

class ParametersExt {
   public:
    static constexpr auto Name = CLAP_EXT_PARAMS;

    const void* Extension() {
        static const clap_plugin_params_t value = {
            &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
        };
        return static_cast<const void*>(&value);
    }

   private:
    static uint32_t _count(const clap_plugin_t* plugin) { return 0; }
    static bool _get_info(const clap_plugin_t* _plugin, uint32_t index, clap_param_info_t* information) {
        return false;
        // memset(information, 0, sizeof(clap_param_info_t));
        // information->id = index;
        //// These flags enable polyphonic modulation.
        // information->flags =
        //     CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE | CLAP_PARAM_IS_MODULATABLE_PER_NOTE_ID;
        // information->min_value = 0.0f;
        // information->max_value = 1.0f;
        // information->default_value = 0.5f;
        // strcpy(information->name, "Volume");
    }

    static bool _get_value(const clap_plugin_t* _plugin, clap_id id, double* value) {
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