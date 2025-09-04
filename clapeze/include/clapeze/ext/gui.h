#pragma once

#include <clap/clap.h>
#include <cstdint>

#include "clap/ext/timer-support.h"
#include "clapeze/basePlugin.h"
namespace clapeze {

enum ClapWindowApi { None = 0, X11, Wayland, Win32, Cocoa };

inline ClapWindowApi toApiEnum(std::string_view api) {
    // clap promises that equal api types are the same pointer
    if (api == CLAP_WINDOW_API_X11) {
        return ClapWindowApi::X11;
    }
    if (api == CLAP_WINDOW_API_WAYLAND) {
        return ClapWindowApi::Wayland;
    }
    if (api == CLAP_WINDOW_API_WIN32) {
        return ClapWindowApi::Win32;
    }
    if (api == CLAP_WINDOW_API_COCOA) {
        return ClapWindowApi::Cocoa;
    }
    // should never happen
    return ClapWindowApi::None;
}

struct WindowHandle {
    explicit WindowHandle(const clap_window_t* window) : api(toApiEnum(window->api)) {
        if (api == ClapWindowApi::X11) {
            x11 = window->x11;
        } else {
            ptr = window->ptr;
        }
    }
    ClapWindowApi api;
    union {
        unsigned long x11;
        void* ptr;
    };
};

/* Unlike all other extensions, BaseGuiFeature is an abstract class. Implement it with your preferred gui library! */
class GuiFeature : public BaseFeature {
   public:
    ~GuiFeature() = default;
    virtual bool IsApiSupported(ClapWindowApi api, bool isFloating) = 0;
    virtual bool GetPreferredApi(ClapWindowApi& apiOut, bool& isFloatingOut) = 0;
    virtual bool Create(ClapWindowApi api, bool isFloating) = 0;
    virtual void Destroy() {};
    virtual bool SetScale(double scale) = 0;
    virtual bool GetSize(uint32_t& widthOut, uint32_t& heightOut) = 0;
    virtual bool CanResize() = 0;
    virtual bool GetResizeHints(clap_gui_resize_hints_t& hintsOut) = 0;
    virtual bool AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) = 0;
    virtual bool SetSize(uint32_t width, uint32_t height) = 0;
    virtual bool SetParent(WindowHandle handle) = 0;
    virtual bool SetTransient(WindowHandle handle) = 0;
    virtual void SuggestTitle(std::string_view title) = 0;
    virtual bool Show() = 0;
    virtual bool Hide() = 0;

    // implementation
   public:
    static constexpr auto NAME = CLAP_EXT_GUI;
    const char* Name() const override { return NAME; }

    void Configure(BasePlugin& self) override {
        static const clap_plugin_gui_t value = {
            &_is_api_supported, &_get_preferred_api, &_create,           &_destroy,     &_set_scale,
            &_get_size,         &_can_resize,        &_get_resize_hints, &_adjust_size, &_set_size,
            &_set_parent,       &_set_transient,     &_suggest_title,    &_show,        &_hide,
        };
        self.RegisterExtension(NAME, static_cast<const void*>(&value));
    }

    bool Validate([[maybe_unused]] const BasePlugin& plugin) const override {
#ifdef __linux
        // if (plugin.TryGetFeature(CLAP_EXT_TIMER_SUPPORT) == nullptr) {
        //     plugin.GetHost().Log(LogSeverity::Fatal, "Gui on linux requires CLAP_EXT_TIMER_SUPPORT");
        //     return false;
        // }
#endif
        return true;
    }

   private:
    static bool _is_api_supported(const clap_plugin_t* plugin, const char* api, bool isFloating) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.IsApiSupported(toApiEnum(api), isFloating);
    }

    static bool _get_preferred_api(const clap_plugin_t* plugin, const char** apiString, bool* isFloating) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        ClapWindowApi api{};
        bool success = self.GetPreferredApi(api, *isFloating);
        if (!success) {
            return false;
        }
        switch (api) {
            case ClapWindowApi::X11: {
                *apiString = CLAP_WINDOW_API_X11;
                break;
            }
            case ClapWindowApi::Wayland: {
                *apiString = CLAP_WINDOW_API_WAYLAND;
                break;
            }
            case ClapWindowApi::Win32: {
                *apiString = CLAP_WINDOW_API_WIN32;
                break;
            }
            case ClapWindowApi::Cocoa: {
                *apiString = CLAP_WINDOW_API_COCOA;
                break;
            }
            case ClapWindowApi::None: {
                return false;
            }
        }
        return true;
    }

    static bool _create(const clap_plugin_t* plugin, const char* apiString, bool isFloating) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        ClapWindowApi api = toApiEnum(apiString);
        if (!self.IsApiSupported(api, isFloating)) {
            return false;
        }
        return self.Create(api, isFloating);
    }

    static void _destroy(const clap_plugin_t* plugin) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        self.Destroy();
    }

    static bool _set_scale(const clap_plugin_t* plugin, double scale) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.SetScale(scale);
    }

    static bool _get_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.GetSize(*width, *height);
    }

    static bool _can_resize(const clap_plugin_t* plugin) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.CanResize();
    }

    static bool _get_resize_hints(const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.GetResizeHints(*hints);
    }

    static bool _adjust_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.AdjustSize(*width, *height);
    }

    static bool _set_size(const clap_plugin_t* plugin, uint32_t width, uint32_t height) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.SetSize(width, height);
    }

    static bool _set_parent(const clap_plugin_t* plugin, const clap_window_t* window) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.SetParent(WindowHandle(window));
    }

    static bool _set_transient(const clap_plugin_t* plugin, const clap_window_t* window) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.SetTransient(WindowHandle(window));
    }

    static void _suggest_title(const clap_plugin_t* plugin, const char* title) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        self.SuggestTitle(std::string_view(title));
    }

    static bool _show(const clap_plugin_t* plugin) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.Show();
    }

    static bool _hide(const clap_plugin_t* plugin) {
        GuiFeature& self = GuiFeature::GetFromPluginObject<GuiFeature>(plugin);
        return self.Hide();
    }
};
}  // namespace clapeze