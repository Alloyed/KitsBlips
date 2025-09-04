#pragma once

#include <clap/plugin.h>

namespace clapeze {
class BasePlugin;

/** Features are opt-in units of functionality for a plugin. */
class BaseFeature {
    friend class BasePlugin;
    /* abstract interface */
   public:
    virtual ~BaseFeature() = default;
    virtual const char* Name() const = 0;
    // virtual const void* Extension() const = 0;
    virtual void Configure(BasePlugin& self) = 0;
    virtual bool Validate([[maybe_unused]] const BasePlugin& plugin) const { return true; }

    template <typename TFeature = BaseFeature>
    static TFeature& GetFromPlugin(BasePlugin& plugin);

   protected:
    template <typename TFeature = BaseFeature>
    static TFeature& GetFromPluginObject(const clap_plugin_t* plugin);
};
}  // namespace clapeze