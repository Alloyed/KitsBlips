// combines preset loading and preset discovery into a single feature, using the same serialization/deserialization that
// the state feature provides.

#pragma once

#include <clap/ext/preset-load.h>
#include <clap/factory/preset-discovery.h>
#include "clapeze/basePlugin.h"
#include "clapeze/ext/baseFeature.h"

namespace clapeze {
class PresetFeature : public BaseFeature {
   public:
    explicit PresetFeature(PluginHost& host) : mHost(host) {}
    static constexpr auto NAME = CLAP_EXT_PRESET_LOAD;
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override {
        static const clap_plugin_preset_load_t value = {
            &_from_location,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

    bool LoadPreset(uint32_t location_kind, const char* location, const char* load_key) {
        const clap_host_t* rawHost = nullptr;
        const clap_host_preset_load_t* rawPresetLoad = nullptr;
        if (!mHost.TryGetExtension(NAME, rawHost, rawPresetLoad)) {
            return false;
        }

        // TODO: actually load preset

        // int32_t osError = 0;
        // const char* msg = "";
        // rawPresetLoad->on_error(rawHost, location_kind, location, load_key, osError, msg);
        rawPresetLoad->loaded(rawHost, location_kind, location, load_key);
        return true;
    }

   private:
    static bool _from_location(const clap_plugin_t* plugin,
                               uint32_t location_kind,
                               const char* location,
                               const char* load_key) {
        PresetFeature& self = PresetFeature::GetFromPluginObject<PresetFeature>(plugin);
        return self.LoadPreset(location_kind, location, load_key);
    }
    PluginHost& mHost;
};

class PresetProvider {
   public:
    static const clap_preset_discovery_provider_t* Create(const clap_preset_discovery_indexer_t* indexer) {
        PresetProvider* self = new PresetProvider(indexer);
        return self->value();
    }

    static const clap_preset_discovery_provider_descriptor_t* Descriptor() {
        // TODO fill out
        static const clap_preset_discovery_provider_descriptor_t desc{};
        return &desc;
    }

   private:
    explicit PresetProvider(const clap_preset_discovery_indexer_t* indexer) : mIndexer(indexer) {}
    const clap_preset_discovery_indexer_t* mIndexer;
    bool Init() {
        // TODO: fill out
        static clap_preset_discovery_filetype_t filetype{};
        if (!mIndexer->declare_filetype(mIndexer, &filetype)) {
            return false;
        }

        static clap_preset_discovery_location_t location{};
        if (!mIndexer->declare_location(mIndexer, &location)) {
            return false;
        }
        return true;
    }
    bool GetMetadata(uint32_t location_kind,
                     const char* location,
                     const clap_preset_discovery_metadata_receiver_t* metadata_receiver) {
        // TODO: fill out
        (void)location;
        (void)metadata_receiver;
        if (location_kind == CLAP_PRESET_DISCOVERY_LOCATION_FILE) {
            // read preset from disk, location == path
        } else if (location_kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN) {
            // read all presets loaded into plugin at once, no location
        }

        // fill in to match preset file format
        // const char* name = "";
        // ATTEMPT(metadata_receiver->begin_preset(metadata_receiver, name, nullptr));
        // clap_universal_plugin_id_t id{};
        // metadata_receiver->add_plugin_id(metadata_receiver, &id);
        // clap_preset_discovery_flags flags{};
        // metadata_receiver->set_flags(metadata_receiver, flags);
        // const char* creator = "";
        // metadata_receiver->add_creator(metadata_receiver, creator);
        // const char* description = "";
        // metadata_receiver->set_description(metadata_receiver, description);
        // const char* feature = "";
        // metadata_receiver->add_feature(metadata_receiver, feature);

        return false;
    }
    static PresetProvider& _instance(const clap_preset_discovery_provider* provider) {
        return *static_cast<PresetProvider*>(provider->provider_data);
    }
    static bool _init(const clap_preset_discovery_provider* provider) { return _instance(provider).Init(); }
    static void _destroy(const clap_preset_discovery_provider* provider) { delete &_instance(provider); }
    static bool _get_metadata(const clap_preset_discovery_provider* provider,
                              uint32_t location_kind,
                              const char* location,
                              const clap_preset_discovery_metadata_receiver_t* metadata_receiver) {
        return _instance(provider).GetMetadata(location_kind, location, metadata_receiver);
    }
    static const void* _get_extension(const struct clap_preset_discovery_provider* provider, const char* extension_id) {
        (void)provider;
        (void)extension_id;
        // this is for preset discovery extensions, of which there are none right now
        return nullptr;
    }
    const clap_preset_discovery_provider_t* value() {
        static clap_preset_discovery_provider_t value{Descriptor(), this,          _init,
                                                      _destroy,     _get_metadata, _get_extension};
        return &value;
    }
};
}  // namespace clapeze