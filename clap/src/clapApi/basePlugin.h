#pragma once

#include <clap/clap.h>
#include <memory>
#include <unordered_map>
#include <cassert>
#include <cstdio>

#include "clapApi/pluginHost.h"

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
        // required
        virtual void Config() = 0;
        virtual void ProcessRaw(const clap_process_t *process) = 0;
        virtual void ProcessEvent(const clap_event_header_t& event) = 0;
        // optional
        virtual bool Init() {return true;}
        virtual bool Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) {return true;}
        virtual void Deactivate() {}
        virtual void Reset() {}
        virtual void OnMainThread() {}

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
            BaseExt* ext = self->TryGetExtension(name);
            assert(ext);
            return *ext;
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
        std::unordered_map<std::string, std::unique_ptr<BaseExt>> mExtensions;
        PluginHost& mHost;
        private:
    static bool _init(const clap_plugin *plugin) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        self.Config();
        return self.Init();
    }

    static void _destroy(const clap_plugin *plugin) {
        BasePlugin* self = &BasePlugin::GetFromPluginObject(plugin);
        delete self;
    }

    static bool _activate(const clap_plugin *plugin, double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        return self.Activate(sampleRate, minFramesCount, maxFramesCount);
    }

    static void _deactivate(const clap_plugin *plugin) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        self.Deactivate();
    }

    static bool _start_processing(const clap_plugin *_plugin) {
        return true;
    }

    static void _stop_processing(const clap_plugin *_plugin) {
    }

    static void _reset(const clap_plugin *plugin) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        self.Reset();
    }

    static clap_process_status _process(const clap_plugin *plugin, const clap_process_t *process) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);

        self.ProcessRaw(process);

        return CLAP_PROCESS_CONTINUE;
    }

    static const void* _get_extension(const clap_plugin *plugin, const char *name) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        const BaseExt* extension = self.TryGetExtension(name);
        return extension != nullptr ? extension->Extension() : nullptr;
    }

    static void _on_main_thread(const clap_plugin *plugin) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        self.OnMainThread();
    }
};

struct PluginEntry {
    clap_plugin_descriptor_t meta;
    BasePlugin* (*factory)(PluginHost& host);
};
