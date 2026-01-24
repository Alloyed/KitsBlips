#pragma once

#include <clap/ext/params.h>
#include <clap/id.h>
#include <etl/queue_spsc_atomic.h>
#include <cstdint>
#include "clapeze/basePlugin.h"
#include "clapeze/params/baseParameter.h"

namespace clapeze::params {
// TODO: remove
using Id = clap_id;
enum class ChangeType : uint8_t { SetValue, SetModulation, StartGesture, StopGesture };
struct Change {
    ChangeType type;
    Id id;
    double value;
};
using Queue = etl::queue_spsc_atomic<Change, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

template <typename T>
concept BaseMainHandle = requires(T handle, Id id, double value) {
    { handle.GetRawValue(id) } -> std::convertible_to<double>;
    { handle.SetRawValue(id, value) } -> std::same_as<void>;
    { handle.StartGesture(id) } -> std::same_as<void>;
    { handle.StopGesture(id) } -> std::same_as<void>;
    { handle.FlushFromAudio() } -> std::same_as<void>;
};

template <typename T>
concept BaseAudioHandle =
    requires(T handle, Id id, double value, BaseProcessor& processor, const clap_output_events_t* out) {
        { handle.ProcessEvent(std::declval<const clap_event_header_t&>()) } -> std::convertible_to<bool>;
        { handle.FlushEventsFromMain(processor, out) } -> std::same_as<void>;
    };

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
class BaseParametersFeature : public BaseFeature {
   public:
    // these `using` declarations are part of the public API. use them to get the handle types without having to think
    // about the declaration of them :)
    using MainHandle = TMainHandle;
    using ProcessorHandle = TAudioHandle;

    static constexpr auto NAME = CLAP_EXT_PARAMS;
    const char* Name() const override;

    BaseParametersFeature(PluginHost& host, Id numParams);

    void Configure(BasePlugin& self) override;

    bool Validate(const BasePlugin& self) const override;

    const BaseParam* GetBaseParam(Id id) const;

    void RequestClear(Id id, clap_param_clear_flags flags = CLAP_PARAM_CLEAR_ALL);

    void RequestRescan(clap_param_rescan_flags flags = CLAP_PARAM_RESCAN_ALL);

    void FlushFromAudio();

    void RequestFlushIfNotProcessing();

    TAudioHandle& GetProcessorHandle();
    TMainHandle& GetMainHandle();

    size_t GetNumParams() const;

   protected:
    PluginHost& mHost;
    const size_t mNumParams;
    std::vector<std::unique_ptr<const BaseParam>> mParams;
    Queue mAudioToMain;
    Queue mMainToAudio;
    TMainHandle mMain;
    TAudioHandle mAudio;

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
template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
const char* BaseParametersFeature<TMainHandle, TAudioHandle>::Name() const {
    return NAME;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
void BaseParametersFeature<TMainHandle, TAudioHandle>::Configure(BasePlugin& self) {
    static const clap_plugin_params_t value = {
        &_count, &_get_info, &_get_value, &_value_to_text, &_text_to_value, &_flush,
    };
    self.RegisterExtension(NAME, static_cast<const void*>(&value));
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
bool BaseParametersFeature<TMainHandle, TAudioHandle>::Validate(const BasePlugin& self) const {
    for (size_t index = 0; index < mNumParams; index++) {
        if (mParams[index].get() == nullptr) {
            self.GetHost().Log(LogSeverity::Fatal, fmt::format("Missing configuration for parameter {}", index));
            return false;
        }
    }
    return true;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
BaseParametersFeature<TMainHandle, TAudioHandle>::BaseParametersFeature(PluginHost& host, Id numParams)
    : mHost(host),
      mNumParams(static_cast<size_t>(numParams)),
      mParams(mNumParams),
      mAudioToMain(),
      mMainToAudio(),
      mMain(mNumParams, mMainToAudio, mAudioToMain),
      mAudio(mParams, mNumParams, mMainToAudio, mAudioToMain) {}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
const BaseParam* BaseParametersFeature<TMainHandle, TAudioHandle>::GetBaseParam(Id id) const {
    clap_id index = static_cast<clap_id>(id);
    if (index >= mNumParams) {
        return nullptr;
    }
    return mParams[index].get();
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
void BaseParametersFeature<TMainHandle, TAudioHandle>::RequestClear(Id id, clap_param_clear_flags flags) {
    const clap_host_t* rawHost = nullptr;
    const clap_host_params_t* rawHostParams = nullptr;
    if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
        clap_id index = static_cast<clap_id>(id);
        rawHostParams->clear(rawHost, index, flags);
    }
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
void BaseParametersFeature<TMainHandle, TAudioHandle>::RequestRescan(clap_param_rescan_flags flags) {
    const clap_host_t* rawHost = nullptr;
    const clap_host_params_t* rawHostParams = nullptr;
    if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
        rawHostParams->rescan(rawHost, flags);
    }
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
void BaseParametersFeature<TMainHandle, TAudioHandle>::FlushFromAudio() {
    mMain.FlushFromAudio();
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
void BaseParametersFeature<TMainHandle, TAudioHandle>::RequestFlushIfNotProcessing() {
    const clap_host_t* rawHost{};
    const clap_host_params_t* rawHostParams{};
    if (mHost.TryGetExtension(CLAP_EXT_PARAMS, rawHost, rawHostParams)) {
        rawHostParams->request_flush(rawHost);
    }
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
TAudioHandle& BaseParametersFeature<TMainHandle, TAudioHandle>::GetProcessorHandle() {
    return mAudio;
}
template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
TMainHandle& BaseParametersFeature<TMainHandle, TAudioHandle>::GetMainHandle() {
    return mMain;
}
template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
size_t BaseParametersFeature<TMainHandle, TAudioHandle>::GetNumParams() const {
    return mNumParams;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
/*static*/ uint32_t BaseParametersFeature<TMainHandle, TAudioHandle>::_count(const clap_plugin_t* plugin) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);
    return static_cast<uint32_t>(self.mNumParams);
}
template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
/*static*/ bool BaseParametersFeature<TMainHandle, TAudioHandle>::_get_info(const clap_plugin_t* plugin,
                                                                            uint32_t index,
                                                                            clap_param_info_t* information) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);

    const BaseParam* param = self.GetBaseParam(static_cast<Id>(index));
    if (param != nullptr) {
        return param->FillInformation(index, information);
    }
    return false;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
/*static*/ bool BaseParametersFeature<TMainHandle, TAudioHandle>::_get_value(const clap_plugin_t* plugin,
                                                                             clap_id id,
                                                                             double* value) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);
    // called from main thread
    if (id < self.mParams.size()) {
        self.FlushFromAudio();
        *value = self.mMain.GetRawValue(static_cast<Id>(id));
        return true;
    }
    return false;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
/*static*/ bool BaseParametersFeature<TMainHandle, TAudioHandle>::_value_to_text(const clap_plugin_t* plugin,
                                                                                 clap_id param_id,
                                                                                 double value,
                                                                                 char* buf,
                                                                                 uint32_t bufferSize) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);

    const BaseParam* param = self.GetBaseParam(static_cast<Id>(param_id));
    if (param != nullptr) {
        auto span = etl::span<char>(buf, bufferSize);
        return param->ToText(value, span);
    }
    return false;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
/*static*/ bool BaseParametersFeature<TMainHandle, TAudioHandle>::_text_to_value(const clap_plugin_t* plugin,
                                                                                 clap_id param_id,
                                                                                 const char* display,
                                                                                 double* out) {
    BaseParametersFeature& self = BaseParametersFeature::GetFromPluginObject<BaseParametersFeature>(plugin);

    const BaseParam* param = self.GetBaseParam(static_cast<Id>(param_id));
    if (param != nullptr) {
        return param->FromText(display, *out);
    }
    return false;
}

template <BaseMainHandle TMainHandle, BaseAudioHandle TAudioHandle>
/*static*/ void BaseParametersFeature<TMainHandle, TAudioHandle>::_flush(const clap_plugin_t* plugin,
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

    self.mAudio.FlushEventsFromMain(processor, out);
}

}  // namespace clapeze::params