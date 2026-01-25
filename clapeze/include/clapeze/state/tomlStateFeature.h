#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <toml++/toml.hpp>

#include "clapeze/basePlugin.h"
#include "clapeze/params/baseParameter.h"
#include "clapeze/pluginHost.h"

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
    static bool _save(const clap_plugin_t* plugin, const clap_ostream_t* out) {
        TParamsFeature& feature = TParamsFeature::template GetFromPluginObject<TParamsFeature>(plugin);
        auto& host = BasePlugin::GetFromPluginObject(plugin).GetHost();

        feature.FlushFromAudio();  // empty queue to ensure newest changes
        auto& handle = feature.GetMainHandle();

        toml::table paramkv{};
        size_t numParams = feature.GetNumParams();
        for (clap_id id = 0; id < numParams; ++id) {
            const BaseParam* param = feature.GetBaseParam(id);
            clap_param_info_t info;
            param->FillInformation(id, &info);
            // TODO: obviously wrong, each param should have a computer-safe key associated
            std::string key = fmt::format("{:03d}/{}", id, info.name);

            double raw = handle.GetRawValue(id);
            paramkv.insert(key, raw);
        }

        // TODO meta
        toml::table file{{"_meta", toml::table{{"version", 0}, {"plugin", "halp"}}}, {"params", paramkv}};
        // TODO: could i wrap a clap_ostream in the C++ stream iterator instead?
        std::ostringstream ss;
        ss << file;
        std::string s = ss.str();
        host.LogFmt(clapeze::LogSeverity::Debug, "saving\n\n{}\n\n", s);
        if (out->write(out, s.data(), s.size()) == -1) {
            return false;
        }
        return true;
    }

    static bool _load(const clap_plugin_t* plugin, const clap_istream_t* in) {
        TParamsFeature& params = TParamsFeature::template GetFromPluginObject<TParamsFeature>(plugin);
        auto& host = BasePlugin::GetFromPluginObject(plugin).GetHost();

        params.FlushFromAudio();  // empty queue so changes apply on top
        auto& handle = params.GetMainHandle();
        std::string file;
        std::string chunk(256, '\0');
        int64_t size{};
        while (size = in->read(in, chunk.data(), chunk.size()), size > 0) {
            file.append(chunk.begin(), chunk.begin() + size);
        }
        if (size == -1) {
            // read error
            return false;
        }

        host.LogFmt(clapeze::LogSeverity::Debug, "loading\n\n{}\n\n", file);
        auto result = toml::parse(file);
        if (!result) {
            // parse error
            std::stringstream ss;
            ss << result.error().source();
            host.LogFmt(clapeze::LogSeverity::Warning, "loading, could not parse toml: {}\nsrc: {}",
                        result.error().description(), ss.str());
            return false;
        }

        // TODO validate meta
        auto savedParams = result.table()["params"].as_table();
        if (!savedParams) {
            // no params
            host.Log(clapeze::LogSeverity::Warning, "loading, no params table (malformed preset?)");
            return false;
        }

        auto parseNumberFromText = [](std::string_view input, double& out) -> bool {
#ifdef __APPLE__
            // Apple doesn't support charconv properly yet (2026/1/7, apple clang 14)
            std::string tmp;
            out = std::strtod(tmp.c_str(), nullptr);
            return true;
#else
            const char* first = input.data();
            const char* last = input.data() + input.size();
            const auto [ptr, ec] = std::from_chars(first, last, out);
            return ec == std::errc{} && ptr == last;
#endif
        };

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
