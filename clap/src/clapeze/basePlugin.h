#pragma once

#include <cassert>
#include <clap/clap.h>
#include <clap/events.h>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <unordered_map>

#include "clapeze/pluginHost.h"

class BasePlugin;

class BaseExt {
    friend class BasePlugin;
    /* abstract interface */
   public:
    virtual ~BaseExt() = default;
    virtual const char* Name() const = 0;
    virtual const void* Extension() const = 0;
    virtual const bool Validate(const BasePlugin& plugin) const { return true; }

    template <typename ExtType = BaseExt>
    static ExtType& GetFromPlugin(BasePlugin& plugin);

   protected:
    template <typename ExtType = BaseExt>
    static ExtType& GetFromPluginObject(const clap_plugin_t* plugin);
};

class BaseProcessor {
    friend class BasePlugin;

   public:
    BaseProcessor() {}
    virtual ~BaseProcessor() = default;

    /**
     * Called when the plugin is first activated. This is a good place to allocate resources, and do any state setup
     * before the main thread can no longer communicate with the processor directly.
     *
     * [main-thread & !active]
     */
    virtual void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) {};
    /**
     * Called when the plugin is deactivated.
     *
     * [main-thread & active]
     */
    virtual void Deactivate() {};
    /**
     * Override to handle discrete events coming from the host.
     *
     * [audio-thread & active & processing]
     */
    virtual void ProcessEvent(const clap_event_header_t& event) = 0;
    /**
     * Override to process an audio block. the block size can be anything below GetMaxBlockSize(). This is to support
     * sample-accurate timing for events.
     *
     * [audio-thread & active & processing]
     */
    virtual void ProcessAudio(const clap_process_t& process, size_t blockStart, size_t blockStop) = 0;
    /**
     * Override to communicate with the main thread. Called once per process.
     *
     * [audio-thread & active & processing]
     */
    virtual void ProcessFlush(const clap_process_t& process) = 0;
    /**
     * Override to reset internal state. you should kill all playing voices, clear all buffers, reset oscillator phases,
     * etc.
     *
     * [audio-thread & active]
     */
    virtual void ProcessReset() {};

    /* only valid while processing. using contextual state. */
    void SendEvent(clap_event_header_t& event)
    {
        event.time += mTime; // this is probably very not smart
        mOutEvents->try_push(mOutEvents, &event);
    }

   protected:
    /**
     * Returns the sample rate in hz
     */
    double GetSampleRate() const { return mSampleRate; }
    size_t GetMaxBlockSize() const { return mMinBlockSize; }
    size_t GetMinBlockSize() const { return mMaxBlockSize; }
    
   private:
    double mSampleRate;
    size_t mMinBlockSize;
    size_t mMaxBlockSize;

    const clap_output_events_t* mOutEvents;
    uint32_t mTime;
};

struct PluginEntry {
    clap_plugin_descriptor_t meta;
    BasePlugin* (*factory)(PluginHost& host);
};

/* Override to create a new plugin */
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
    template <class BaseExtT, class... Args>
    BaseExtT& ConfigExtension(Args&&... args);

    template <class BaseExtT, class... Args>
    BaseExtT* TryConfigExtension(Args&&... args);

    template <class BaseProcessorT, class... Args>
    BaseProcessorT& ConfigProcessor(Args&&... args);

    /* helpers */
   public:
    const clap_plugin_t* GetOrCreatePluginObject(const clap_plugin_descriptor_t* meta);

    template <typename PluginType = BasePlugin>
    static PluginType& GetFromPluginObject(const clap_plugin_t* plugin);

    BaseExt* TryGetExtension(const char* name);
    const BaseExt* TryGetExtension(const char* name) const;
    BaseProcessor& GetProcessor() const;
    PluginHost& GetHost();
    const PluginHost& GetHost() const;

    /* internal implementation*/
   private:
    std::unique_ptr<clap_plugin_t> mPlugin;
    std::unordered_map<std::string, std::unique_ptr<BaseExt>> mExtensions;
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
template <typename ExtType, typename... Args>
ExtType& BasePlugin::ConfigExtension(Args&&... args) {
    auto ptr = std::make_unique<ExtType>(std::forward<Args>(args)...);
    const char* name = ptr->Name();
    ExtType* out = ptr.get();
    mExtensions.emplace(name, std::move(ptr));
    return *out;
}

template <typename ExtType, typename... Args>
ExtType* BasePlugin::TryConfigExtension(Args&&... args) {
    if(GetHost().SupportsExtension(ExtType::NAME)) {
        return &ConfigExtension<ExtType>(std::forward<Args> (args)...);
    }
    return nullptr;
}

template <typename ProcessorType, typename... Args>
ProcessorType& BasePlugin::ConfigProcessor(Args&&... args) {
    mProcessor = std::make_unique<ProcessorType>(std::forward<Args>(args)...);
    return static_cast<ProcessorType&>(*mProcessor);
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
