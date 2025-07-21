#pragma once

#include <clap/clap.h>
#include <clap/events.h>
#include <cassert>
#include <memory>
#include <unordered_map>

#include "clapeze/baseProcessor.h"
#include "clapeze/ext/baseFeature.h"
#include "clapeze/pluginHost.h"

namespace clapeze {

class BasePlugin;

struct PluginEntry {
    clap_plugin_descriptor_t meta;
    BasePlugin* (*factory)(PluginHost& host);
};

/**
 * A plugin is the unit of functionality exposed to users in their DAW host.
 * To create your own plugin, extend this base class, implement Plugin::Config(), then register it to a PluginFactory.
 * As a starting point, clapeze::InstrumentPlugin and clapeze::EffectPlugin define reasonable defaults for basic
 * instruments and effects.
 */
class BasePlugin {
   public:
    BasePlugin(PluginHost& host) : mHost(host) {}
    virtual ~BasePlugin() = default;

   protected:
    // required
    virtual void Config() = 0;
    // optional
    virtual bool Init();
    virtual bool Activate(double sampleRate, uint32_t minBlockSize, uint32_t maxBlockSize);
    virtual void Deactivate();
    virtual void Reset();
    virtual void OnMainThread();
    virtual bool ValidateConfig();

    /* implementation methods */
    template <typename TFeature, typename... TArgs>
    TFeature& ConfigFeature(TArgs&&... args);

    template <typename TFeature, typename... TArgs>
    TFeature* TryConfigFeature(TArgs&&... args);

    template <typename TProcessor, typename... TArgs>
    TProcessor& ConfigProcessor(TArgs&&... args);

    /* helpers */
   public:
    const clap_plugin_t* GetOrCreatePluginObject(const clap_plugin_descriptor_t* meta);

    template <typename TPlugin = BasePlugin>
    static TPlugin& GetFromPluginObject(const clap_plugin_t* plugin);

    BaseFeature* TryGetFeature(const char* name);
    const BaseFeature* TryGetFeature(const char* name) const;
    BaseProcessor& GetProcessor() const;
    PluginHost& GetHost();
    const PluginHost& GetHost() const;

    /* internal implementation*/
   private:
    std::unique_ptr<clap_plugin_t> mPlugin;
    std::unordered_map<std::string, std::unique_ptr<BaseFeature>> mFeatures;
    PluginHost& mHost;
    std::unique_ptr<BaseProcessor> mProcessor;

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
template <typename TFeature, typename... TArgs>
TFeature& BasePlugin::ConfigFeature(TArgs&&... args) {
    auto ptr = std::make_unique<TFeature>(std::forward<TArgs>(args)...);
    const char* name = ptr->Name();
    TFeature* out = ptr.get();
    mFeatures.emplace(name, std::move(ptr));
    return *out;
}

template <typename TFeature, typename... TArgs>
TFeature* BasePlugin::TryConfigFeature(TArgs&&... args) {
    if (GetHost().SupportsExtension(TFeature::NAME)) {
        return &ConfigFeature<TFeature>(std::forward<TArgs>(args)...);
    }
    return nullptr;
}

template <typename TProcessor, typename... TArgs>
TProcessor& BasePlugin::ConfigProcessor(TArgs&&... args) {
    mProcessor = std::make_unique<TProcessor>(std::forward<TArgs>(args)...);
    return static_cast<TProcessor&>(*mProcessor);
}

template <typename TPlugin>
TPlugin& BasePlugin::GetFromPluginObject(const clap_plugin_t* plugin) {
    BasePlugin* self = static_cast<BasePlugin*>(plugin->plugin_data);
    return static_cast<TPlugin&>(*self);
}

template <typename TFeature>
TFeature& BaseFeature::GetFromPluginObject(const clap_plugin_t* plugin) {
    BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
    BaseFeature* ext = self.TryGetFeature(TFeature::NAME);
    assert(ext);
    return static_cast<TFeature&>(*ext);
}

template <typename TFeature>
TFeature& BaseFeature::GetFromPlugin(BasePlugin& self) {
    BaseFeature* ext = self.TryGetFeature(TFeature::NAME);
    assert(ext);
    return static_cast<TFeature&>(*ext);
}

}  // namespace clapeze