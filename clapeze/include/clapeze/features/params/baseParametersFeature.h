#pragma once

#include <clap/events.h>
#include <clap/ext/params.h>
#include <clap/id.h>
#include <etl/queue_spsc_atomic.h>
#include <concepts>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include "clapeze/basePlugin.h"
#include "clapeze/features/params/baseParameter.h"
#include "clapeze/stringUtils.h"

namespace clapeze::params {
enum class ChangeType : uint8_t { SetValue, SetModulation, StartGesture, StopGesture };
struct Change {
    ChangeType type;
    clap_id id;
    double value;
};
using Queue = etl::queue_spsc_atomic<Change, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

class BaseMainHandle {
   public:
    virtual ~BaseMainHandle() = default;
    virtual double GetRawValue(clap_id id) const = 0;
    virtual void SetRawValue(clap_id id, double value) = 0;
    virtual void StartGesture(clap_id id) = 0;
    virtual void StopGesture(clap_id id) = 0;
    virtual void FlushFromAudio() = 0;
};

class BaseProcessorHandle {
   public:
    virtual ~BaseProcessorHandle() = default;
    virtual bool ProcessEvent(const clap_event_header_t& event) = 0;
    virtual void FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) = 0;
};

class BaseParametersFeature : public BaseFeature {
   public:
    static constexpr auto NAME = CLAP_EXT_PARAMS;
    const char* Name() const override;

    BaseParametersFeature(PluginHost& host, clap_id numParams);

    void Configure(BasePlugin& self) override;

    bool Validate(const BasePlugin& self) const override;

    const BaseParam* GetBaseParam(clap_id id) const;

    void RequestClear(clap_id id, clap_param_clear_flags flags = CLAP_PARAM_CLEAR_ALL);

    void RequestRescan(clap_param_rescan_flags flags = CLAP_PARAM_RESCAN_ALL);

    void FlushFromAudio();

    void RequestFlushIfNotProcessing();

    template <std::derived_from<BaseProcessorHandle> THandle = BaseProcessorHandle>
    THandle& GetProcessorHandle() {
        return static_cast<THandle&>(*mAudio);
    }
    template <std::derived_from<BaseMainHandle> THandle = BaseMainHandle>
    THandle& GetMainHandle() {
        return static_cast<THandle&>(*mMain);
    }

    size_t GetNumParams() const;
    std::optional<clap_id> GetIdFromKey(std::string_view) const;
    void ResetAllParamsToDefault();

   protected:
    PluginHost& mHost;
    const size_t mNumParams;
    std::vector<std::unique_ptr<const BaseParam>> mParams;
    std::unordered_map<std::string, clap_id> mParamKeyToId{};
    Queue mAudioToMain;
    Queue mMainToAudio;
    std::unique_ptr<BaseMainHandle> mMain;
    std::unique_ptr<BaseProcessorHandle> mAudio;

   private:
    static uint32_t _count(const clap_plugin_t* plugin);
    static bool _get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* information);

    static bool _get_value(const clap_plugin_t* plugin, clap_id id, double* value);

    static bool _value_to_text(const clap_plugin_t* plugin,
                               clap_id param_id,
                               double value,
                               char* buf,
                               uint32_t bufferSize);

    static bool _text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* display, double* out);

    static void _flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out);
};

// impl
inline const char* BaseParametersFeature::Name() const {
    return NAME;
}

inline void BaseParametersFeature::Configure(BasePlugin& self) {
    static const clap_plugin_params_t value = {
        &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
    };
    self.RegisterExtension(NAME, static_cast<const void*>(&value));
}

inline bool BaseParametersFeature::Validate(const BasePlugin& self) const {
    for (size_t index = 0; index < mNumParams; index++) {
        if (mParams[index].get() == nullptr) {
            self.GetHost().Log(LogSeverity::Fatal, fmt::format("Missing configuration for parameter {}", index));
            return false;
        }
    }

    if (self.TryGetFeature(CLAP_EXT_STATE) == nullptr) {
        self.GetHost().Log(LogSeverity::Warning,
                           "Missing implementation of CLAP_EXT_STATE: all parameters will be lost upon save/reload");
    }
    return true;
}

inline BaseParametersFeature::BaseParametersFeature(PluginHost& host, clap_id numParams)
    : mHost(host), mNumParams(static_cast<size_t>(numParams)), mParams(mNumParams), mAudioToMain(), mMainToAudio() {}

inline const BaseParam* BaseParametersFeature::GetBaseParam(clap_id id) const {
    if (id >= mNumParams) {
        return nullptr;
    }
    return mParams[id].get();
}

inline void BaseParametersFeature::RequestClear(clap_id id, clap_param_clear_flags flags) {
    const clap_host_t* rawHost = nullptr;
    const clap_host_params_t* rawHostParams = nullptr;
    if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
        rawHostParams->clear(rawHost, id, flags);
    }
}

inline void BaseParametersFeature::RequestRescan(clap_param_rescan_flags flags) {
    const clap_host_t* rawHost = nullptr;
    const clap_host_params_t* rawHostParams = nullptr;
    if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
        rawHostParams->rescan(rawHost, flags);
    }
}

inline void BaseParametersFeature::FlushFromAudio() {
    mMain->FlushFromAudio();
}

inline void BaseParametersFeature::RequestFlushIfNotProcessing() {
    const clap_host_t* rawHost{};
    const clap_host_params_t* rawHostParams{};
    if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
        rawHostParams->request_flush(rawHost);
    }
}

inline size_t BaseParametersFeature::GetNumParams() const {
    return mNumParams;
}
inline std::optional<clap_id> BaseParametersFeature::GetIdFromKey(std::string_view key) const {
    auto iter = mParamKeyToId.find(std::string(key));
    if (iter != mParamKeyToId.end()) {
        return iter->second;
    }
    return std::nullopt;
}
inline void BaseParametersFeature::ResetAllParamsToDefault() {
    for (clap_id id = 0; id < mNumParams; ++id) {
        mMain->SetRawValue(id, GetBaseParam(id)->GetRawDefault());
        // RequestClear(id); // resets automation? idk if necessary
    }
    RequestRescan(CLAP_PARAM_RESCAN_VALUES);
}

/*static*/ inline uint32_t BaseParametersFeature::_count(const clap_plugin_t* plugin) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);
    return static_cast<uint32_t>(self.mNumParams);
}
/*static*/ inline bool BaseParametersFeature::_get_info(const clap_plugin_t* plugin,
                                                        uint32_t index,
                                                        clap_param_info_t* information) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);

    const BaseParam* param = self.GetBaseParam(index);
    if (param != nullptr) {
        memset(information, 0, sizeof(clap_param_info_t));
        information->id = index;
        information->flags = param->GetFlags();
        information->min_value = param->GetRawMin();
        information->max_value = param->GetRawMax();
        information->default_value = param->GetRawDefault();
        stringCopy(information->name, param->GetName());
        stringCopy(information->module, param->GetModule());
        return information;
    }
    return false;
}

/*static*/ inline bool BaseParametersFeature::_get_value(const clap_plugin_t* plugin, clap_id id, double* value) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);
    // called from main thread
    if (id < self.mParams.size()) {
        self.FlushFromAudio();
        *value = self.mMain->GetRawValue(id);
        return true;
    }
    return false;
}

/*static*/ inline bool BaseParametersFeature::_value_to_text(const clap_plugin_t* plugin,
                                                             clap_id param_id,
                                                             double value,
                                                             char* buf,
                                                             uint32_t bufferSize) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);

    const BaseParam* param = self.GetBaseParam(param_id);
    if (param != nullptr) {
        auto span = etl::span<char>(buf, bufferSize);
        return param->ToText(value, span);
    }
    return false;
}

/*static*/ inline bool BaseParametersFeature::_text_to_value(const clap_plugin_t* plugin,
                                                             clap_id param_id,
                                                             const char* display,
                                                             double* out) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);

    const BaseParam* param = self.GetBaseParam(param_id);
    if (param != nullptr) {
        return param->FromText(display, *out);
    }
    return false;
}

/*static*/ inline void BaseParametersFeature::_flush(const clap_plugin_t* plugin,
                                                     const clap_input_events_t* in,
                                                     const clap_output_events_t* out) {
    // called from audio thread when active(), main thread when inactive
    BasePlugin& basePlugin = BasePlugin::GetFromPluginObject(plugin);
    BaseProcessor& processor = basePlugin.GetProcessor();
    BaseParametersFeature& self = BaseParametersFeature::GetFromPlugin<BaseParametersFeature>(basePlugin);

    // Process events sent from the host to us
    if (in) {
        const uint32_t eventCount = in->size(in);
        for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
            processor.ProcessEvent(*in->get(in, eventIndex));
        }
    }

    self.mAudio->FlushEventsFromMain(processor, out);
}

}  // namespace clapeze::params
