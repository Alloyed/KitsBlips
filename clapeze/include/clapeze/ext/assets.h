// Loads files from either from disk directly, or from a virtual filesystem embedded within the plugin.
#pragma once

#include <clap/ext/preset-load.h>
#include <clap/factory/preset-discovery.h>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include "clapeze/basePlugin.h"
#include "clapeze/ext/baseFeature.h"
#include "clapeze/pluginHost.h"
#include "physfs.h"

namespace clapeze {
class AssetsFeature : public BaseFeature {
   public:
    explicit AssetsFeature(PluginHost& host) : mHost(host) {}
    static constexpr auto NAME = CLAP_EXT_PRESET_LOAD;
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override {}

    bool LoadResourceToString(uint32_t location_kind, const char* location, const char* load_key, std::string& buf) {
        buf.clear();
        switch (location_kind) {
            case CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN: {
                PHYSFS_File* handle = PHYSFS_openRead(load_key);
                if (handle == nullptr) {
                    const char* err = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
                    mHost.Log(LogSeverity::Error, err);
                    return false;
                }
                size_t bufLengthHint = PHYSFS_fileLength(handle);
                buf.resize(bufLengthHint);
                size_t bytesRead = PHYSFS_readBytes(handle, buf.data(), bufLengthHint);
                if (bytesRead < 0) {
                    // error
                    buf.clear();
                    const char* err = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
                    mHost.Log(LogSeverity::Error, err);
                    return false;
                } else {
                    // truncate
                    buf.resize(bytesRead);
                }
                return true;

                break;
            }
            case CLAP_PRESET_DISCOVERY_LOCATION_FILE: {
                // this is an absolute path, so we need to use raw file io
                auto close_file = [](FILE* f) { fclose(f); };

                auto holder = std::unique_ptr<FILE, decltype(close_file)>(fopen(location, "rb"), close_file);
                if (!holder) {
                    return false;
                }

                FILE* f = holder.get();

                // in C++17 following lines can be folded into std::filesystem::file_size invocation
                std::error_code ec;
                size_t size = std::filesystem::file_size(location, ec);
                if (ec) {
                    return false;
                }

                buf.resize(size);
                // C++17 defines .data() which returns a non-const pointer
                fread(const_cast<char*>(buf.data()), 1, size, f);
                return true;
            }
            default: {
                return false;
            }
        }
    }

   private:
    PluginHost& mHost;
};
}  // namespace clapeze