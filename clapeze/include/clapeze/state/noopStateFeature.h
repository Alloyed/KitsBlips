#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <cstdint>
#include <cstdio>

#include "clapeze/basePlugin.h"

namespace clapeze {

/*
 * Useful more for debugging. some plugin hosts will bail if they don't see a state feature
 */
template <class TParamsFeature>
class NoopStateFeature : public BaseFeature {
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

    bool Validate(const BasePlugin& plugin) const override {
        if (plugin.TryGetFeature(CLAP_EXT_PARAMS) == nullptr) {
            plugin.GetHost().Log(LogSeverity::Fatal,
                                 "Missing implementation of CLAP_EXT_PARAMS, does this plugin need parameters?");
            return false;
        }
        return true;
    }

   private:
    static bool _save(const clap_plugin_t* plugin, const clap_ostream_t* out) {
        TParamsFeature& params = TParamsFeature::template GetFromPluginObject<TParamsFeature>(plugin);
        params.FlushFromAudio();  // empty queue to ensure newest changes
        return true;
    }

    static bool _load(const clap_plugin_t* plugin, const clap_istream_t* in) {
        TParamsFeature& params = TParamsFeature::template GetFromPluginObject<TParamsFeature>(plugin);
        auto& host = BasePlugin::GetFromPluginObject(plugin).GetHost();

        params.FlushFromAudio();  // empty queue so changes apply on top

        // loading file just to ensure we can, and log the results
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

        host.Log(clapeze::LogSeverity::Info, fmt::format("received file {}", file));

        return true;
    }
};
}  // namespace clapeze
