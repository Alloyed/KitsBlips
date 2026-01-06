#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <cstdint>
#include <cstdio>

#include "clapeze/basePlugin.h"
#include "clapeze/ext/parameters.h"

namespace clapeze {

/* Saves and loads parameter state. Depends on ParametersFeature. */
class StateFeature : public BaseFeature {
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
        BaseParamsFeature& params = BaseParamsFeature::GetFromPluginObject<BaseParamsFeature>(plugin);

        params.FlushFromAudio();  // empty queue to ensure newest changes

        size_t numParams = params.GetNumParams();
        for (clap_id id = 0; id < numParams; ++id) {
            double value = params.GetRawValue(id);
            if (out->write(out, &value, sizeof(double)) == -1) {
                return false;
            }
        }
        return true;
    }

    static bool _load(const clap_plugin_t* plugin, const clap_istream_t* in) {
        BaseParamsFeature& params = BaseParamsFeature::GetFromPluginObject<BaseParamsFeature>(plugin);

        params.FlushFromAudio();  // empty queue so changes apply on top

        size_t numParams = params.GetNumParams();
        clap_id id = 0;
        while (id < numParams) {
            double value = 0.0f;
            int64_t result = in->read(in, &value, sizeof(double));
            if (result == -1) {
                // error
                return false;
            } else if (result == 0) {
                // eof
                break;
            }
            params.SetRawValue(id, value);
            id++;
        }
        return id == numParams;
    }
};
}  // namespace clapeze
