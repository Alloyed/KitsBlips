#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <cstdio>
#include <cwchar>
#include <toml++/toml.hpp>

#include "clap/plugin.h"
#include "clap/stream.h"
#include "clapeze/basePlugin.h"
#include "clapeze/features/baseFeature.h"
#include "clapeze/features/params/baseParameter.h"
#include "clapeze/features/presetFeature.h"
#include "clapeze/features/state/baseStateFeature.h"
#include "clapeze/impl/streamUtils.h"
#include "clapeze/pluginHost.h"

namespace clapeze {
struct Metadata {
    std::string pluginId;
    PresetInfo preset;

    static void LoadPresetInfo(toml::table* presetkv, PresetInfo& preset);
    /* Used for filesystem scanning */
    static std::optional<Metadata> loadMetadata(std::istream& in);
};

template <class TParamsFeature>
class TomlStateFeature : public BaseStateFeature {
   public:
    explicit TomlStateFeature(BasePlugin& self, uint32_t saveVersion = 0) : mPlugin(self), mSaveVersion(saveVersion) {}
    static constexpr auto NAME = CLAP_EXT_STATE;
    const char* Name() const override { return NAME; }
    void Configure(BasePlugin& self) override {
        static const clap_plugin_state_t value = {
            &_save,
            &_load,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

    bool Validate(const BasePlugin& plugin) const override { return true; }

    // customization points
    virtual bool OnMigrate(toml::table& t, uint32_t currentVersion, uint32_t targetVersion) const { return true; }
    virtual bool OnSave(toml::table& t) const { return true; }
    virtual bool OnLoad(const toml::table& t) { return true; }

    bool Save(std::ostream& out) override {
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(mPlugin);
        PresetFeature* presets = static_cast<PresetFeature*>(mPlugin.TryGetFeature(PresetFeature::NAME));
        const clap_plugin_descriptor_t& desc = mPlugin.GetDescriptor();

        toml::table file{};
        file.insert("_meta", toml::table{{"version", mSaveVersion}, {"plugin", desc.id}});

        if (presets) {
            const PresetInfo& info = presets->GetPresetInfo();
            toml::table presetkv{
                {"name", info.name},
                {"creator", info.creator},
                {"description", info.description},
                //{"features", info.features}
            };
            file.insert("preset", presetkv);
        }

        toml::table paramkv{};
        params.FlushFromAudio();  // empty queue to ensure newest changes
        auto& handle = params.GetMainHandle();
        size_t numParams = params.GetNumParams();
        for (clap_id id = 0; id < numParams; ++id) {
            const BaseParam* param = params.GetBaseParam(id);
            paramkv.insert(param->GetKey(), handle.GetRawValue(id));
        }
        file.insert("params", paramkv);

        if (!OnSave(file)) {
            return false;
        }

        out << file;
        return out.good();
    }

    bool Load(std::istream& in) override {
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(mPlugin);
        PresetFeature* presets = static_cast<PresetFeature*>(mPlugin.TryGetFeature(PresetFeature::NAME));
        PluginHost& host = mPlugin.GetHost();
        const clap_plugin_descriptor_t& desc = mPlugin.GetDescriptor();

        host.Log(clapeze::LogSeverity::Debug, "loading file");
        std::string fileText = impl::istream_tostring(in);
        auto result = toml::parse(fileText);
        if (!result) {
            // parse error
            host.LogFmt(clapeze::LogSeverity::Warning, "loading, could not parse toml: {}",
                        result.error().description());
            return true;
        }

        auto& file = result.table();

        // Meta
        {
            auto metakv = file["_meta"].as_table();
            if (!metakv) {
                host.Log(clapeze::LogSeverity::Warning, "loading, no _meta table (malformed preset?)");
                return true;
            }

            if (auto s = metakv->get_as<std::string>("plugin"); !s || **s != desc.id) {
                host.LogFmt(clapeze::LogSeverity::Warning, "loading, wrong plugin id (expected '{}', got '{}')",
                            desc.id, s ? **s : "nullptr");
                return true;
            }

            if (auto v = metakv->get("version")->value_or(0u); v != mSaveVersion) {
                if (!OnMigrate(file, v, mSaveVersion)) {
                    host.LogFmt(clapeze::LogSeverity::Warning, "loading, migration failed");
                    return true;
                }
            }
        }

        // Preset Info (optional)
        {
            auto presetkv = file["preset"].as_table();
            if (presetkv && presets) {
                PresetInfo& preset = presets->GetPresetInfo();
                Metadata::LoadPresetInfo(presetkv, preset);
            }
        }

        // Params
        {
            auto paramskv = file["params"].as_table();
            if (!paramskv) {
                host.Log(clapeze::LogSeverity::Warning, "loading, no params table (malformed preset?)");
                return true;
            }
            params.FlushFromAudio();  // empty queue so changes apply on top
            auto& handle = params.GetMainHandle();
            for (const auto& [keyNode, valueNode] : *paramskv) {
                if (auto id = params.GetIdFromKey(keyNode.str())) {
                    double value = valueNode.value_or(0.0);
                    handle.SetRawValue(*id, value);
                } else {
                    host.LogFmt(clapeze::LogSeverity::Warning, "loading, skipping unknown key: {}", keyNode.str());
                }
            }
        }

        if (!OnLoad(file)) {
            return false;
        }

        return true;
    }

   private:
    BasePlugin& mPlugin;
    uint32_t mSaveVersion;
    static bool _save(const clap_plugin_t* plugin, const clap_ostream_t* out) {
        TomlStateFeature<TParamsFeature>& self =
            BaseFeature::GetFromPluginObject<TomlStateFeature<TParamsFeature>>(plugin);
        impl::clap_ostream stream(out);
        return self.Save(stream);
    }

    static bool _load(const clap_plugin_t* plugin, const clap_istream_t* in) {
        TomlStateFeature<TParamsFeature>& self =
            BaseFeature::GetFromPluginObject<TomlStateFeature<TParamsFeature>>(plugin);
        impl::clap_istream stream(in);
        return self.Load(stream);
    }
};

inline void Metadata::LoadPresetInfo(toml::table* presetkv, PresetInfo& preset) {
    auto nameNode = presetkv->get("name");
    preset.name = nameNode ? nameNode->value_or("") : "";
    auto creatorNode = presetkv->get("creator");
    preset.creator = creatorNode ? creatorNode->value_or("") : "";
    auto descriptionNode = presetkv->get("description");
    preset.description = descriptionNode ? descriptionNode->value_or("") : "";
    auto featuresNode = presetkv->get_as<toml::array>("features");
    if (featuresNode) {
        preset.features.clear();
        featuresNode->visit([&preset](toml::value<std::string>& s) { preset.features.push_back(*s); });
    }
}
/* Used for filesystem scanning */
inline std::optional<Metadata> Metadata::loadMetadata(std::istream& in) {
    Metadata data;

    std::string fileText = impl::istream_tostring(in);
    auto result = toml::parse(fileText);
    if (!result) {
        return std::nullopt;
    }
    auto& file = result.table();

    auto metakv = file["_meta"].as_table();
    if (!metakv) {
        return std::nullopt;
    }
    auto pluginId = metakv->get_as<std::string>("plugin");
    if (!pluginId) {
        return std::nullopt;
    }
    data.pluginId = **pluginId;

    auto presetkv = file["presetInfo"].as_table();
    if (presetkv) {
        LoadPresetInfo(presetkv, data.preset);
    }
    return data;
}
}  // namespace clapeze
