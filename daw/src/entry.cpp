#include <clapeze/entryPoint.h>

#ifndef KITSBLIPS_ENABLE_SENTRY
// simple entry point
extern "C" const clap_plugin_entry_t clap_entry = CLAPEZE_CREATE_ENTRY_POINT();
#else
// entry point with sentry enabled

#include <sentry.h>
#include <filesystem>
#if _WIN32
#include <processenv.h>
#endif

namespace {
std::filesystem::path getSentryDbPath(std::string_view appName) {
    namespace fs = std::filesystem;
    const fs::path app = fs::path(appName) / "sentry-native";
#if defined(_WIN32)
    char buffer[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("LOCALAPPDATA", buffer, MAX_PATH);
    if (len > 0) {
        return fs::path(buffer) / app;
    }
#elif defined(__APPLE__)
    // the correct thing to do here is use applicationSupportDirectory from an mm file. blegh
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / "Library" / "Application Support" / app;
    }
#else
    // Linux
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg) {
        return fs::path(xdg) / app;
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / ".cache" / app;
    }
#endif
    // backup
    return fs::temp_directory_path() / app;
}

bool initSentry(const char* path) {
    auto* opts = sentry_options_new();
    sentry_options_set_dsn(opts, KITSBLIPS_SENTRY_DSN);
    sentry_options_set_release(opts, PRODUCT_ID "@" PRODUCT_VERSION);
    sentry_options_set_debug(opts, 1);
    sentry_options_set_enable_logs(opts, 1);

    auto pluginPath = std::filesystem::path(path);
    auto dbPath = getSentryDbPath(PRODUCT_NAME).generic_string();
#if _WIN32
    auto crashpadPath = std::filesystem::absolute(pluginPath.parent_path() / "./crashpad_handler.exe").generic_string();
#else
    auto crashpadPath = std::filesystem::absolute(pluginPath.parent_path() / "./crashpad_handler").generic_string();
#endif
    sentry_options_set_handler_path(opts, crashpadPath.c_str());
    sentry_options_set_database_path(opts, dbPath.c_str());

    return sentry_init(opts) != 0;
}
}  // namespace

extern "C" const clap_plugin_entry_t clap_entry = (clap_plugin_entry_t{
    CLAP_VERSION_INIT,
    [](const char* path) -> bool {
        if (!initSentry(path)) {
            return false;
        }
        return clapeze::EntryPoint::_init(path);
    },
    []() {
        clapeze::EntryPoint::_deinit();
        sentry_close();
    },
    clapeze::EntryPoint::_get_factory,
});
#endif
