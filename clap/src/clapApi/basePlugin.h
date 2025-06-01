#pragma once

#include <clap/clap.h>
#include <memory>
#include <unordered_map>

#include "clapApi/pluginHost.h"

using ExtensionMap = std::unordered_map<std::string_view, const void*>;

class BaseExt {
    /* abstract interface */
    public:
    virtual const char* Name() const = 0;
    virtual const void* Extension() const = 0;
};

/* Override to create a new plugin */
class BasePlugin
{
    /* abstract interface */
    public:
        virtual void Config() = 0;
        virtual void ProcessRaw(const clap_process_t *process) = 0;
        virtual void ProcessEvent(const clap_event_header_t& event) = 0;

    /* public methods */
    public:
        BasePlugin(PluginHost& host):mHost(host) {}
        virtual ~BasePlugin() = default;

        const clap_plugin_t* GetOrCreatePluginObject(const clap_plugin_descriptor_t* meta);
        const clap_plugin_t* GetPluginObject() const {
            return mPlugin.get();
        }
        static BasePlugin& GetFromPluginObject(const clap_plugin_t* plugin) {
            BasePlugin* self = (BasePlugin*)(plugin->plugin_data);
            return *self;
        }
        static BaseExt& GetExtensionFromPluginObject(const clap_plugin_t* plugin, const char* name) {
            BasePlugin* self = (BasePlugin*)(plugin->plugin_data);
            return *(self->mExtensions.at(name));
        }
        BaseExt* TryGetExtension(const char* name) {
            if (auto search = mExtensions.find(name); search != mExtensions.end()) {
                return search->second.get();
            } else {
                return nullptr;
            }
        }

        // TODO: concepts
        template<class BaseExtT, class... Args>
        BaseExtT& ConfigExtension(Args&&... args) {
            auto ptr = std::make_unique<BaseExtT>(std::forward<Args>(args)...);
            const char* name = ptr->Name();
            BaseExtT* out = ptr.get();
            mExtensions.emplace(name, std::move(ptr));
            return *out;
        }

    private:
        std::unique_ptr<clap_plugin_t> mPlugin;
        std::unordered_map<const char*, std::unique_ptr<BaseExt>> mExtensions;
        PluginHost& mHost;
};

struct PluginEntry {
    clap_plugin_descriptor_t meta;
    BasePlugin* (*factory)(PluginHost& host);
};
