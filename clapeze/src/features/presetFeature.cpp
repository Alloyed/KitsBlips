// combines preset loading and preset discovery into a single feature, using the same serialization/deserialization that
// the state feature provides.

#include "clapeze/features/presetFeature.h"
#include <fstream>
#include "clap/factory/preset-discovery.h"
#include "clapeze/features/state/tomlStateFeature.h"

namespace clapeze {

PresetFeature::PresetFeature(BasePlugin& self) : mHost(self.GetHost()) {}
const char* PresetFeature::Name() const {
    return NAME;
}

void PresetFeature::Configure(BasePlugin& self) {
    static const clap_plugin_preset_load_t value = {
        &_from_location,
    };
    self.RegisterExtension(NAME, static_cast<const void*>(&value));
}
bool PresetFeature::Validate(const BasePlugin& plugin) const {
    if (plugin.TryGetFeature(AssetsFeature::NAME) == nullptr) {
        plugin.GetHost().Log(LogSeverity::Fatal, "Presets enabled, but AssetsFeature is not. Please add it");
        return false;
    }
    if (plugin.TryGetFeature(CLAP_EXT_STATE) == nullptr) {
        plugin.GetHost().Log(LogSeverity::Fatal, "Presets enabled, but CLAP_EXT_STATE is not. Please add it");
        return false;
    }
    return true;
}

bool PresetFeature::LoadPreset(clap_preset_discovery_location_kind location_kind,
                               const char* location,
                               const char* load_key) {
    const clap_host_t* rawHost = nullptr;
    const clap_host_preset_load_t* rawPresetLoad = nullptr;
    if (!mHost.TryGetExtension(NAME, rawHost, rawPresetLoad)) {
        return false;
    }

    // TODO: actually load preset
    /*
    BaseStateFeature& state = BaseFeature::TryGetFromPlugin<BaseStateFeature>(mPlugin);
    AssetsFeature& assets = BaseFeature::TryGetFromPlugin<AssetsFeature>(mPlugin);
    {
        auto loadStream = assets.OpenLoadStream();
        if (!loadStream) {
            // int32_t osError = 0;
            // const char* msg = "";
            // rawPresetLoad->on_error(rawHost, location_kind, location, load_key, osError, msg);
            return false;
        }
        if (!state.Load(loadStream.get())) {
            // int32_t osError = 0;
            // const char* msg = "";
            // rawPresetLoad->on_error(rawHost, location_kind, location, load_key, osError, msg);
            return false;
        }
    }
    // rawPresetLoad->loaded(rawHost, location_kind, location, load_key);
    */
    mLastPresetLocation = {.kind = location_kind,
                           .location = location_kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN ? load_key : location};
    return true;
}
bool PresetFeature::LoadLastPreset() {
    if (!mLastPresetLocation) {
        // no last preset to load
        return true;
    }

    return LoadPreset(mLastPresetLocation->kind, mLastPresetLocation->location.c_str(),
                      mLastPresetLocation->location.c_str());
}

bool PresetFeature::SavePreset(const char* path) {
    (void)path;
    // TODO: something like so
    /*
    BaseStateFeature& state = BaseFeature::TryGetFromPlugin<BaseStateFeature>(mPlugin);
    AssetsFeature& assets = BaseFeature::TryGetFromPlugin<AssetsFeature>(mPlugin);
    {
        auto saveStream = assets.OpenSaveStream();
        if (!saveStream) {
            return false;
        }
        if (!state.Save(saveStream.get())) {
            return false;
        }
    }
    */
    return true;
}

PresetInfo& PresetFeature::GetPresetInfo() {
    return mPresetInfo;
}
const PresetInfo& PresetFeature::GetPresetInfo() const {
    return mPresetInfo;
}

/*static*/ bool PresetFeature::_from_location(const clap_plugin_t* plugin,
                                              uint32_t location_kind,
                                              const char* location,
                                              const char* load_key) {
    PresetFeature& self = PresetFeature::GetFromPluginObject<PresetFeature>(plugin);
    return self.LoadPreset(static_cast<clap_preset_discovery_location_kind>(location_kind), location, load_key);
}

/*static*/ const clap_preset_discovery_provider_t* PresetProvider::Create(
    const clap_preset_discovery_indexer_t* indexer) {
    PresetProvider* self = new PresetProvider(indexer);
    return self->value();
}

/*static*/ const clap_preset_discovery_provider_descriptor_t* PresetProvider::Descriptor() {
    // TODO fill out
    static const clap_preset_discovery_provider_descriptor_t desc{};
    return &desc;
}
PresetProvider::PresetProvider(const clap_preset_discovery_indexer_t* indexer) : mIndexer(indexer) {}

bool PresetProvider::Init() {
    static clap_preset_discovery_filetype_t filetype{
        .name = "preset",
        .description = "plain text preset",
        .file_extension = "toml",
    };
    if (!mIndexer->declare_filetype(mIndexer, &filetype)) {
        return false;
    }

    static clap_preset_discovery_location_t builtins{
        .flags = CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT,
        .name = "builtin",
        .kind = CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN,
        .location = nullptr,
    };
    if (!mIndexer->declare_location(mIndexer, &builtins)) {
        return false;
    }

    static clap_preset_discovery_location_t savedPresets{
        .flags = CLAP_PRESET_DISCOVERY_IS_USER_CONTENT,
        .name = "user",
        .kind = CLAP_PRESET_DISCOVERY_LOCATION_FILE,
        .location = "",  // directory
    };
    if (!mIndexer->declare_location(mIndexer, &savedPresets)) {
        return false;
    }
    return true;
}
bool PresetProvider::GetMetadata(uint32_t location_kind,
                                 const char* location,
                                 const clap_preset_discovery_metadata_receiver_t* metadata_receiver) {
    /* Fill preset. plugin id should be `_meta.plugin` in the file, preset should be `preset` in the file */
    auto fillPreset = [&metadata_receiver](const std::string& pluginId, const PresetInfo& info) {
        const char* loadKey = nullptr;
        if (!metadata_receiver->begin_preset(metadata_receiver, info.name.c_str(), loadKey)) {
            return false;
        }

        clap_universal_plugin_id_t id{"clap", pluginId.c_str()};
        metadata_receiver->add_plugin_id(metadata_receiver, &id);
        metadata_receiver->add_creator(metadata_receiver, info.creator.c_str());
        metadata_receiver->set_description(metadata_receiver, info.description.c_str());
        for (const auto& feature : info.features) {
            metadata_receiver->add_feature(metadata_receiver, feature.c_str());
        }

        metadata_receiver->set_flags(metadata_receiver, info.flags);

        return true;
    };

    if (location_kind == CLAP_PRESET_DISCOVERY_LOCATION_FILE) {
        // read preset from disk, location == path
        std::ifstream stream = mAssets.OpenFromFilesystem(location);
        Metadata data = *Metadata::loadMetadata(stream);
        return fillPreset(data.pluginId, data.preset);
    } else if (location_kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN) {
        // read all presets loaded into plugin at once, no location
    }

    return true;
}
/*static*/ PresetProvider& PresetProvider::_instance(const clap_preset_discovery_provider* provider) {
    return *static_cast<PresetProvider*>(provider->provider_data);
}
/*static*/ bool PresetProvider::_init(const clap_preset_discovery_provider* provider) {
    return _instance(provider).Init();
}
/*static*/ void PresetProvider::_destroy(const clap_preset_discovery_provider* provider) {
    delete &_instance(provider);
}
/*static*/ bool PresetProvider::_get_metadata(const clap_preset_discovery_provider* provider,
                                              uint32_t location_kind,
                                              const char* location,
                                              const clap_preset_discovery_metadata_receiver_t* metadata_receiver) {
    return _instance(provider).GetMetadata(location_kind, location, metadata_receiver);
}
/*static*/ const void* PresetProvider::_get_extension(const struct clap_preset_discovery_provider* provider,
                                                      const char* extension_id) {
    (void)provider;
    (void)extension_id;
    // this is for preset discovery extensions, of which there are none right now
    return nullptr;
}
const clap_preset_discovery_provider_t* PresetProvider::value() {
    static clap_preset_discovery_provider_t value{Descriptor(), this, _init, _destroy, _get_metadata, _get_extension};
    return &value;
}
}  // namespace clapeze