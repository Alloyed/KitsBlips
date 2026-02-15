#pragma once

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <cstdio>

#include "clapeze/basePlugin.h"
#include "clapeze/features/state/baseStateFeature.h"
#include "clapeze/impl/streamUtils.h"

namespace clapeze {

/*
 * Saves and loads parameter state. This works(tm), but you should probably prefer TomlStateFeature.
 */
template <class TParamsFeature>
class BinaryStateFeature : public BaseStateFeature {
   public:
    explicit BinaryStateFeature(BasePlugin& self) : mPlugin(self) {}
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

    bool Save(std::ostream& out) override {
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(mPlugin);

        params.FlushFromAudio();  // empty queue to ensure newest changes
        auto& handle = params.GetMainHandle();

        size_t numParams = params.GetNumParams();
        for (clap_id id = 0; id < numParams; ++id) {
            double value = handle.GetRawValue(id);
            out.write(reinterpret_cast<char*>(&value), sizeof(double));
            if (out.fail()) {
                return false;
            }
        }
        return true;
        return true;
    }
    bool Load(std::istream& in) override {
        TParamsFeature& params = BaseFeature::GetFromPlugin<TParamsFeature>(mPlugin);

        params.FlushFromAudio();  // empty queue so changes apply on top
        auto& handle = params.GetMainHandle();
        size_t numParams = params.GetNumParams();
        clap_id id = 0;
        while (id < numParams) {
            double value = 0.0f;
            in.read(reinterpret_cast<char*>(&value), sizeof(double));
            if (in.bad()) {
                // error
                return false;
            } else if (in.eof()) {
                // eof
                break;
            }
            handle.SetRawValue(id, value);
            id++;
        }
        return id == numParams;
        return true;
    }

   private:
    BasePlugin& mPlugin;
    static bool _save(const clap_plugin_t* plugin, const clap_ostream_t* out) {
        BinaryStateFeature<TParamsFeature>& self =
            BaseFeature::GetFromPluginObject<BinaryStateFeature<TParamsFeature>>(plugin);
        impl::clap_ostream stream(out);
        return self.Save(stream);
    }

    static bool _load(const clap_plugin_t* plugin, const clap_istream_t* in) {
        BinaryStateFeature<TParamsFeature>& self =
            BaseFeature::GetFromPluginObject<BinaryStateFeature<TParamsFeature>>(plugin);
        impl::clap_istream stream(in);
        return self.Load(stream);
    }
};
}  // namespace clapeze
