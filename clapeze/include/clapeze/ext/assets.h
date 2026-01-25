// Loads files from either from disk directly, or from a virtual filesystem embedded within the plugin.
#pragma once

#include <string>
#include "clapeze/ext/baseFeature.h"

namespace clapeze {
class PluginHost;
class AssetsFeature : public BaseFeature {
   public:
    explicit AssetsFeature(PluginHost& host);
    ~AssetsFeature() override;
    AssetsFeature(const AssetsFeature&) = delete;
    AssetsFeature(AssetsFeature&&) = delete;
    AssetsFeature& operator=(const AssetsFeature&) = delete;
    AssetsFeature& operator=(AssetsFeature&&) = delete;

    static constexpr auto NAME = "clapeze.assets";
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override;

    bool LoadFromPlugin(const char* path, std::string& out);
    bool LoadFromFilesystem(const char* path, std::string& out);

   private:
    PluginHost& mHost;
};
}  // namespace clapeze