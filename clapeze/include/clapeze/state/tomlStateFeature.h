#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <cstdio>
#include <cwchar>
#include <toml++/toml.hpp>

#include "clap/plugin.h"
#include "clapeze/basePlugin.h"
#include "clapeze/ext/baseFeature.h"
#include "clapeze/params/baseParameter.h"
#include "clapeze/pluginHost.h"
#include "clapeze/streamUtils.h"

namespace clapeze {

template <class TParamsFeature>
class TomlStateFeature : public BaseFeature {
   public:
    explicit TomlStateFeature(uint32_t saveVersion = 0) : mSaveVersion(saveVersion) {}
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

   private:
    uint32_t mSaveVersion;
    static bool _save(const clap_plugin_t* _plugin, const clap_ostream_t* out) {
        BasePlugin& plugin = BasePlugin::GetFromPluginObject(_plugin);
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(plugin);
        TomlStateFeature<TParamsFeature>& self = BaseFeature::GetFromPlugin<TomlStateFeature<TParamsFeature>>(plugin);
        const clap_plugin_descriptor_t& desc = plugin.GetDescriptor();

        params.FlushFromAudio();  // empty queue to ensure newest changes
        auto& handle = params.GetMainHandle();

        toml::table paramkv{};
        size_t numParams = params.GetNumParams();
        for (clap_id id = 0; id < numParams; ++id) {
            const BaseParam* param = params.GetBaseParam(id);
            paramkv.insert(param->GetKey(), handle.GetRawValue(id));
        }

        toml::table file{{"_meta", toml::table{{"version", self.mSaveVersion}, {"plugin", desc.id}}},
                         {"params", paramkv}};

        if (!self.OnSave(file)) {
            return false;
        }

        clap_ostream_streambuf buf(out);
        std::ostream stream(&buf);
        stream << file;

        return stream.good();
    }

    static bool _load(const clap_plugin_t* _plugin, const clap_istream_t* in) {
        BasePlugin& plugin = BasePlugin::GetFromPluginObject(_plugin);
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(plugin);
        TomlStateFeature<TParamsFeature>& self = BaseFeature::GetFromPlugin<TomlStateFeature<TParamsFeature>>(plugin);
        const clap_plugin_descriptor_t& desc = plugin.GetDescriptor();
        PluginHost& host = plugin.GetHost();

        params.FlushFromAudio();  // empty queue so changes apply on top
        auto& handle = params.GetMainHandle();

        host.Log(clapeze::LogSeverity::Debug, "loading file");
        std::string file = clap_istream_tostring(in);
        auto result = toml::parse(file);
        if (!result) {
            // parse error
            host.LogFmt(clapeze::LogSeverity::Warning, "loading, could not parse toml: {}",
                        result.error().description());
            return true;
        }

        auto& table = result.table();
        auto meta = table["_meta"].as_table();
        if (!meta) {
            host.Log(clapeze::LogSeverity::Warning, "loading, no _meta table (malformed preset?)");
            return true;
        }

        if (auto s = meta->get_as<std::string>("plugin"); !s || **s != desc.id) {
            host.LogFmt(clapeze::LogSeverity::Warning, "loading, wrong plugin id (expected '{}', got '{}')", desc.id,
                        s ? **s : "nullptr");
            return true;
        }

        if (auto v = meta->get("version")->value_or(0u); v != self.mSaveVersion) {
            if (!self.OnMigrate(table, v, self.mSaveVersion)) {
                host.LogFmt(clapeze::LogSeverity::Warning, "loading, migration failed");
                return true;
            }
        }

        auto savedParams = table["params"].as_table();
        if (!savedParams) {
            host.Log(clapeze::LogSeverity::Warning, "loading, no params table (malformed preset?)");
            return true;
        }

        for (const auto& [keyNode, valueNode] : *savedParams) {
            if (auto id = params.GetIdFromKey(keyNode.str())) {
                double value = valueNode.value_or(0.0);
                handle.SetRawValue(*id, value);
            } else {
                host.LogFmt(clapeze::LogSeverity::Warning, "loading, skipping unknown key: {}", keyNode.str());
            }
        }

        if (!self.OnLoad(table)) {
            return false;
        }

        return true;
    }
};
}  // namespace clapeze
