#pragma once

#include <clap/clap.h>
#include <memory>
#include <unordered_map>

#include "clapApi/pluginHost.h"

using ExtensionMap = std::unordered_map<std::string_view, const void*>;

/* Override to create a new plugin */
class BasePlugin
{
    /* abstract interface */
    public:
        virtual const clap_plugin_descriptor_t& GetPluginDescriptor() const = 0;
        virtual const ExtensionMap& GetExtensions() const = 0;
        virtual void ProcessRaw(const clap_process_t *process) = 0;

    /* public methods */
    public:
        BasePlugin(PluginHost& host):mHost(host) {}
        void Prepare() { CreatePluginObject(); }
        virtual ~BasePlugin() = default;

        const clap_plugin_t* GetPluginObject() const {
            return mPlugin.get();
        }
        static BasePlugin& GetFromPluginObject(const clap_plugin_t* plugin) {
            BasePlugin* self = (BasePlugin*)(plugin->plugin_data);
            return *self;
        }

    private:
        void CreatePluginObject();
        std::unique_ptr<clap_plugin_t> mPlugin;
        PluginHost& mHost;
};
