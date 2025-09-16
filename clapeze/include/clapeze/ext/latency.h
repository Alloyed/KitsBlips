/*
 * Reports the latency a plugin introduces to the host.
 */

#pragma once

#include <clap/ext/preset-load.h>
#include <clap/factory/preset-discovery.h>
#include "clap/ext/latency.h"
#include "clapeze/basePlugin.h"
#include "clapeze/ext/baseFeature.h"

namespace clapeze {
class LatencyFeature : public BaseFeature {
   public:
    explicit LatencyFeature(uint32_t initialLatencySamples)
        : mLatencySamples(initialLatencySamples), mNextLatencySamples(initialLatencySamples) {}
    static constexpr auto NAME = CLAP_EXT_LATENCY;
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override {
        self.GetHost().TryGetExtension(NAME, mRawHost, mRawHostLatency);
        static const clap_plugin_latency_t value = {
            &_get,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

    void OnActivated() {
        if (mNextLatencySamples != mLatencySamples) {
            mLatencySamples = mNextLatencySamples;
            mRawHostLatency->changed(mRawHost);
        }
    }

    /*
     * NOTE: if you are changing latency outside of the Activate() method, call host.RequestRestart() so it can be
     * applied on next activation.
     */
    void ChangeLatency(uint32_t newLatencySamples) { mNextLatencySamples = newLatencySamples; }

   private:
    static uint32_t _get(const clap_plugin_t* plugin) {
        LatencyFeature& self = LatencyFeature::GetFromPluginObject<LatencyFeature>(plugin);
        return self.mLatencySamples;
    }
    uint32_t mLatencySamples;
    uint32_t mNextLatencySamples;
    const clap_host_t* mRawHost = nullptr;
    const clap_host_latency_t* mRawHostLatency = nullptr;
};
}  // namespace clapeze