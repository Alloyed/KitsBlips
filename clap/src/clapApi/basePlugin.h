#pragma once

#include <clap/clap.h>
#include <cassert>
#include <cstdio>
#include <memory>
#include <unordered_map>

#include "clapApi/pluginHost.h"

class BasePlugin;

class BaseExt {
    /* abstract interface */
   public:
    virtual ~BaseExt() = default;
    virtual const char* Name() const = 0;
    virtual const void* Extension() const = 0;

    template <typename ExtType = BaseExt>
    static ExtType& GetFromPlugin(BasePlugin& plugin);

   protected:
    template <typename ExtType = BaseExt>
    static ExtType& GetFromPluginObject(const clap_plugin_t* plugin);
};

struct PluginEntry {
    clap_plugin_descriptor_t meta;
    BasePlugin* (*factory)(PluginHost& host);
};

/* Override to create a new plugin */
class BasePlugin {
    /* abstract interface */
   public:
    BasePlugin(PluginHost& host) : mHost(host) {}
    virtual ~BasePlugin() = default;
    // required
    virtual void Config() = 0;
    virtual void ProcessFlush(const clap_process_t& process) = 0;
    virtual void ProcessEvent(const clap_event_header_t& event) = 0;
    virtual void ProcessAudio(const clap_process_t& process, size_t rangeStart, size_t rangeStop) = 0;
    // optional
    virtual bool Init() { return true; }
    virtual bool Activate(double sampleRate, uint32_t minFramesCount, uint32_t maxFramesCount) { return true; }
    virtual void Deactivate() {}
    virtual void Reset() {}
    virtual void OnMainThread() {}

    /* implementation methods */
   protected:
    // TODO: concepts
    template <class BaseExtT, class... Args>
    BaseExtT& ConfigExtension(Args&&... args);

    /* helpers */
   public:
    const clap_plugin_t* GetOrCreatePluginObject(const clap_plugin_descriptor_t* meta);

    template <typename PluginType = BasePlugin>
    static PluginType& GetFromPluginObject(const clap_plugin_t* plugin);

    BaseExt* TryGetExtension(const char* name);
    PluginHost& GetHost();
    double GetSampleRate() const;

    /* internal implementation*/
   private:
    std::unique_ptr<clap_plugin_t> mPlugin;
    std::unordered_map<std::string, std::unique_ptr<BaseExt>> mExtensions;
    PluginHost& mHost;
    double mSampleRate;

    static bool _init(const clap_plugin* plugin);
    static void _destroy(const clap_plugin* plugin);
    static bool _activate(const clap_plugin* plugin,
                          double sampleRate,
                          uint32_t minFramesCount,
                          uint32_t maxFramesCount);
    static void _deactivate(const clap_plugin* plugin);
    static bool _start_processing(const clap_plugin* _plugin);
    static void _stop_processing(const clap_plugin* _plugin);
    static void _reset(const clap_plugin* plugin);
    static clap_process_status _process(const clap_plugin* plugin, const clap_process_t* process);
    static const void* _get_extension(const clap_plugin* plugin, const char* name);
    static void _on_main_thread(const clap_plugin* plugin);
};

// template implementations
template <typename ExtType, typename... Args>
ExtType& BasePlugin::ConfigExtension(Args&&... args) {
    auto ptr = std::make_unique<ExtType>(std::forward<Args>(args)...);
    const char* name = ptr->Name();
    ExtType* out = ptr.get();
    mExtensions.emplace(name, std::move(ptr));
    return *out;
}

template <typename PluginType>
PluginType& BasePlugin::GetFromPluginObject(const clap_plugin_t* plugin) {
    BasePlugin* self = static_cast<BasePlugin*>(plugin->plugin_data);
    return static_cast<PluginType&>(*self);
}

template <typename ExtType>
ExtType& BaseExt::GetFromPluginObject(const clap_plugin_t* plugin) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    BaseExt* ext = self.TryGetExtension(ExtType::NAME);
    assert(ext);
    return static_cast<ExtType&>(*ext);
}

template <typename ExtType>
ExtType& BaseExt::GetFromPlugin(BasePlugin& self) {
    BaseExt* ext = self.TryGetExtension(ExtType::NAME);
    assert(ext);
    return static_cast<ExtType&>(*ext);
}
