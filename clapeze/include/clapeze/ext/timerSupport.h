#pragma once

#include <clap/clap.h>

#include "clap/ext/timer-support.h"
#include "clapeze/basePlugin.h"

namespace clapeze {

class TimerSupportFeature : public BaseFeature {
   public:
    explicit TimerSupportFeature(PluginHost& host) : BaseFeature() {}
    static constexpr auto NAME = CLAP_EXT_TIMER_SUPPORT;
    const char* Name() const override { return NAME; }

    const void* Extension() const override {
        static const clap_plugin_timer_support_t value = {
            &_on_timer,
        };
        return static_cast<const void*>(&value);
    }

   private:
    static void _on_timer(const clap_plugin_t* plugin, clap_id timerId) {
        BasePlugin& self = BasePlugin::GetFromPluginObject(plugin);
        self.GetHost().OnTimer(timerId);
    }
};
}  // namespace clapeze