#pragma once

#include <clap/clap.h>
#include <cstdint>
#include <cstdio>

#include "clapApi/basePlugin.h"

/* Saves and loads parameter state. Depends on ParametersExt. */
class StateExt: public BaseExt {
   public:
    static constexpr auto NAME = CLAP_EXT_STATE;
    const char* Name() const override {
        return NAME;
    }

    const void* Extension() const override {
        static const clap_plugin_state_t value = {
            &_save,
            &_load,
        };
        return static_cast<const void*>(&value);
    }

   private:
    static bool _save(const clap_plugin_t* plugin, const clap_ostream_t* out) {
        ParametersExt& params = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);

        params.Flush();

        size_t numParams = params.GetNumParams();
        for(ParamId id = 0; id < numParams; ++id)
        {
            float value = params.Get(id);
            if(out->write(out, &value, sizeof(float)) == -1)
            {
                return false;
            }
        }
        return true;
    }

    static bool _load(const clap_plugin_t* plugin, const clap_istream_t* in) {
        ParametersExt& params = ParametersExt::GetFromPluginObject<ParametersExt>(plugin);

        size_t numParams = params.GetNumParams();
        for(ParamId id = 0; id < numParams; ++id)
        {
            float value = 0.0f;
            if(in->read(in, &value, sizeof(float)) == -1)
            {
                return false;
            }
            params.Set(id, value);
        }
        return true;
    }
};