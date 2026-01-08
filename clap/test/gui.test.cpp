#include <clap/entry.h>
#include <clap/ext/gui.h>
#include <clap/ext/timer-support.h>
#include <clap/factory/plugin-factory.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" const clap_plugin_entry_t clap_entry;
static constexpr const char* cPluginId = "kitsblips.kitskeys";

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;

    // not using gtest because unit-testing UI is tricky
    // instead this is a sample standalone app that can be manually QA'd
    // emulating a clap host from the outside by making all the calls a clap host would
    struct mock_host_data {
        std::unordered_map<std::string, void*> extensions;
        std::vector<clap_id> activeTimers{};
        clap_id nextTimer = 0;
    } sMockHostData;
    clap_host_gui_t sMockGuiHost{
        // resize_hints_changed
        [](const clap_host_t* host) -> void {},
        // request_resize
        [](const clap_host_t* host, uint32_t width, uint32_t height) -> bool { return false; },
        // request_show
        [](const clap_host_t* host) -> bool { return false; },
        // request_hide
        [](const clap_host_t* host) -> bool { return false; },
        // closed
        [](const clap_host_t* host, bool was_destroyed) -> void {},
    };
    sMockHostData.extensions.insert({std::string(CLAP_EXT_GUI), &sMockGuiHost});
    clap_host_timer_support_t sMockTimerSupportHost{
        // register_timer
        [](const clap_host_t* host, uint32_t period_ms, clap_id* timer_id) -> bool {
            auto data = static_cast<mock_host_data*>(host->host_data);
            *timer_id = data->nextTimer;
            printf("registering timer %d\n", *timer_id);
            data->activeTimers.push_back(*timer_id);
            return true;
        },
        // unregister_timer
        [](const clap_host_t* host, clap_id timer_id) -> bool {
            auto data = static_cast<mock_host_data*>(host->host_data);
            auto& timers = data->activeTimers;
            size_t oldCount = timers.size();
            printf("erase timer %d\n", timer_id);
            timers.erase(std::remove(timers.begin(), timers.end(), timer_id), timers.end());
            return timers.size() < oldCount;
        },
    };
    sMockHostData.extensions.insert({std::string(CLAP_EXT_TIMER_SUPPORT), &sMockTimerSupportHost});
    clap_host_t sMockHost{
        CLAP_VERSION,
        &sMockHostData,
        "mock host",
        "mock host vendor",
        "https://crouton.net",
        "0.0.0",
        // get_extension
        [](const clap_host_t* host, const char* extension_id) -> const void* {
            printf("get host extension: %s\n", extension_id);
            auto data = static_cast<mock_host_data*>(host->host_data);
            auto iter = data->extensions.find(extension_id);
            if (iter == data->extensions.end()) {
                return nullptr;
            }
            return iter->second;
        },
        // request_restart
        [](const clap_host_t* host) -> void {},
        // request_process
        [](const clap_host_t* host) -> void {},
        // request_callback
        [](const clap_host_t* host) -> void {},
    };
    clap_host_t* host = &sMockHost;
    bool ok = false;

    // find plugin
    printf("clap_entry.init()\n");
    auto& entry = clap_entry;
    clap_entry.init(getenv("PWD"));
    auto factory = static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    assert(factory);
    auto pluginCount = factory->get_plugin_count(factory);
    assert(pluginCount >= 1);
    printf("factory->create_plugin(%s)\n", cPluginId);
    auto plugin = factory->create_plugin(factory, host, cPluginId);
    assert(plugin);
    // TODO: before now calling into host should assert(false), after now it's safe
    ok = plugin->init(plugin);
    assert(ok);

    printf("Fully Initialized!\n");

    // GUI START
    auto clap_plugin_gui = static_cast<const clap_plugin_gui_t*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
    assert(clap_plugin_gui);
    auto clap_plugin_timer_support =
        static_cast<const clap_plugin_timer_support_t*>(plugin->get_extension(plugin, CLAP_EXT_TIMER_SUPPORT));
    assert(clap_plugin_timer_support);
    const char* api = "x11";
    bool isFloating = true;

    ok = clap_plugin_gui->is_api_supported(plugin, api, isFloating);
    assert(ok);
    ok = clap_plugin_gui->create(plugin, api, isFloating);
    assert(ok);
#if 0  // needs an x11 window to parent to
    clap_window_t parentWindow{};
    parentWindow.api = api;
    parenWindow.ptr = nullptr;
    if (isFloating) {
        ok = clap_plugin_gui->set_transient(plugin, &parentWindow);
        assert(ok);
        clap_plugin_gui->suggest_title(plugin, "mock title");
    } else {
        ok = clap_plugin_gui->set_scale(plugin, 1.0);
        assert(ok);
        bool resizable = clap_plugin_gui->can_resize(plugin);
        assert(resizable == false);
        uint32_t width{};
        uint32_t height{};
        ok = clap_plugin_gui->get_size(plugin, &width, &height);
        assert(ok);
        ok = clap_plugin_gui->set_parent(plugin, &parentWindow);
        assert(ok);
    }
#endif

    // show, hide, update, etc.
    for (int32_t openIter = 0; openIter < 3; openIter++) {
        ok = clap_plugin_gui->show(plugin);
        assert(ok);

        // fake run the timer for a few frames to see what happens
        for (uint32_t ticks = 0; ticks < 30; ++ticks) {
            auto timersCopy = sMockHostData.activeTimers;
            for (const auto id : timersCopy) {
                clap_plugin_timer_support->on_timer(plugin, id);
            }
            std::this_thread::sleep_for(16ms);
        }

        ok = clap_plugin_gui->hide(plugin);
        assert(ok);
    }
    printf("cleanup\n");
    clap_plugin_gui->destroy(plugin);
    // GUI END

    // cleanup
    plugin->destroy(plugin);
    clap_entry.deinit();
}