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
#include "clapeze/stringUtils.h"

namespace clapeze {

/*
 * Saves and loads parameter state. Depends on ParametersFeature.
 * TODO:
 *  - plain text format (xml? toml?)
 *  - migrations/versioning
 *  - non-parameter state
 *  - samples/wavetables?
 *  - presets?
 */
template <class TParamsFeature>
class TomlStateFeature : public BaseFeature {
   public:
    explicit TomlStateFeature() {}
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

   private:
    // TODO: you should subclass to get migration functionality i think
    uint32_t mSaveVersion = 0;
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
            // TODO: remove id
            std::string key = fmt::format("{:03d}/{}", id, param->GetKey());

            double raw = handle.GetRawValue(id);
            paramkv.insert(key, raw);
        }

        toml::table file{{"_meta", toml::table{{"version", self.mSaveVersion}, {"plugin", desc.id}}},
                         {"params", paramkv}};

        clap_ostream_streambuf buf(out);
        std::ostream stream(&buf);
        stream << file;

        return stream.good();
    }

    static bool _load(const clap_plugin_t* _plugin, const clap_istream_t* in) {
        BasePlugin& plugin = BasePlugin::GetFromPluginObject(_plugin);
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(plugin);
        // TomlStateFeature<TParamsFeature>& self =
        // BaseFeature::GetFromPlugin<TomlStateFeature<TParamsFeature>>(plugin);
        const clap_plugin_descriptor_t& desc = plugin.GetDescriptor();
        PluginHost& host = plugin.GetHost();

        params.FlushFromAudio();  // empty queue so changes apply on top
        auto& handle = params.GetMainHandle();

        // TODO we can't use clap_istream_streambuf because it doesn't implement random seek, and toml::parse() expects
        // that.
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

        // if (auto v = meta->get("version")->value_or(0u); v != self.mSaveVersion) {
        //     if (!self.mMigrator(table, v)) {
        //         host.LogFmt(clapeze::LogSeverity::Warning, "loading, migration failed");
        //         return true;
        //     }
        // }

        auto savedParams = table["params"].as_table();
        if (!savedParams) {
            host.Log(clapeze::LogSeverity::Warning, "loading, no params table (malformed preset?)");
            return true;
        }

        // TODO: obviously wrong, each param should have a computer-safe key associated
        for (const auto& [k, v] : *savedParams) {
            double id{};
            if (parseNumberFromText(k.str().substr(0, 3), id)) {
                double value = v.value_or(0.0);
                handle.SetRawValue(static_cast<clap_id>(id), value);
            } else {
                host.LogFmt(clapeze::LogSeverity::Warning, "loading, could not parse key: {}", k.str());
            }
        }

        return true;
    }
};
}  // namespace clapeze
