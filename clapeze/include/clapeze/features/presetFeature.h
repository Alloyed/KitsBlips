// combines preset loading and preset discovery into a single feature, using the same serialization/deserialization that
// the state feature provides.

#pragma once

#include <clap/ext/preset-load.h>
#include <clap/factory/preset-discovery.h>
#include <optional>
#include "clapeze/basePlugin.h"
#include "clapeze/features/assetsFeature.h"
#include "clapeze/features/baseFeature.h"

namespace clapeze {

struct PresetInfo {
    std::string name{};
    std::string creator{};
    std::string description{};
    std::vector<std::string> features{};
    clap_preset_discovery_flags flags{};
};
struct PresetLocation {
    clap_preset_discovery_location_kind kind{};
    std::string location{};
};
class PresetFeature : public BaseFeature {
   public:
    explicit PresetFeature(BasePlugin& self);
    static constexpr auto NAME = CLAP_EXT_PRESET_LOAD;
    const char* Name() const override;

    void Configure(BasePlugin& self) override;

    bool Validate(const BasePlugin& plugin) const override;

    bool LoadPreset(clap_preset_discovery_location_kind location_kind, const char* location, const char* load_key);
    bool LoadLastPreset();

    bool SavePreset(const char* path);

    PresetInfo& GetPresetInfo();
    const PresetInfo& GetPresetInfo() const;

   private:
    static bool _from_location(const clap_plugin_t* plugin,
                               uint32_t location_kind,
                               const char* location,
                               const char* load_key);
    BasePlugin& mPlugin;
    PluginHost& mHost;
    PresetInfo mPresetInfo{};
    std::optional<PresetLocation> mLastPresetLocation{};
};

class PresetProvider {
   public:
    static const clap_preset_discovery_provider_t* Create(const clap_preset_discovery_indexer_t* indexer);

    static const clap_preset_discovery_provider_descriptor_t* Descriptor();

   private:
    explicit PresetProvider(const clap_preset_discovery_indexer_t* indexer);
    const clap_preset_discovery_indexer_t* mIndexer;
    AssetsFeature mAssets{};

    bool Init();
    bool GetMetadata(uint32_t location_kind,
                     const char* location,
                     const clap_preset_discovery_metadata_receiver_t* metadata_receiver);

    static PresetProvider& _instance(const clap_preset_discovery_provider* provider);
    static bool _init(const clap_preset_discovery_provider* provider);
    static void _destroy(const clap_preset_discovery_provider* provider);
    static bool _get_metadata(const clap_preset_discovery_provider* provider,
                              uint32_t location_kind,
                              const char* location,
                              const clap_preset_discovery_metadata_receiver_t* metadata_receiver);
    static const void* _get_extension(const struct clap_preset_discovery_provider* provider, const char* extension_id);
    const clap_preset_discovery_provider_t* value();
};
}  // namespace clapeze