#include "clapApi/basePlugin.h"

#include <cassert>

const clap_plugin_t* BasePlugin::GetOrCreatePluginObject(const clap_plugin_descriptor_t* meta) {
    if(mPlugin.get() == nullptr)
    {
        clap_plugin_t pluginObject = {
            meta,
            this,
            &_init,
            &_destroy,
            &_activate,
            &_deactivate,
            &_start_processing,
            &_stop_processing,
            &_reset,
            &_process,
            &_get_extension,
            &_on_main_thread
        };
        mPlugin = std::make_unique<clap_plugin_t>(pluginObject);
    }
    setbuf(stdout, NULL);
    return mPlugin.get();
}