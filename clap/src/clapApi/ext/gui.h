#pragma once

#include <clap/clap.h>
#include <cstdint>

#include "clapApi/basePlugin.h"

enum class WindowingApi { None = 0, X11, Wayland, Win32, Cocoa };

struct WindowHandle {
    WindowHandle(const clap_window_t* window) {}
    WindowingApi api;
    void* ptr;
};

inline WindowingApi toApiEnum(const char* api) {
    // clap promises that equal api types are the same pointer
    if (api == CLAP_WINDOW_API_X11) {
        return WindowingApi::X11;
    }
    if (api == CLAP_WINDOW_API_WAYLAND) {
        return WindowingApi::Wayland;
    }
    if (api == CLAP_WINDOW_API_WIN32) {
        return WindowingApi::Win32;
    }
    if (api == CLAP_WINDOW_API_COCOA) {
        return WindowingApi::Cocoa;
    }
    // should never happen
    return WindowingApi::None;
}

/* Unlike all other extensions, BaseGuiExt is an abstract class. Implement it with your preferred gui library! */
class GuiExt : public BaseExt {
   public:
       ~GuiExt() = default;
    virtual bool IsApiSupported(WindowingApi api, bool isFloating) = 0;
    virtual bool GetPreferredApi(WindowingApi& apiOut, bool& isFloatingOut) = 0;
    virtual bool Create(WindowingApi api, bool isFloating) = 0;
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

    const void* Extension() const override {
        static const clap_plugin_gui_t value = {
            &_is_api_supported, &_get_preferred_api, &_create,           &_destroy,     &_set_scale,
            &_get_size,         &_can_resize,        &_get_resize_hints, &_adjust_size, &_set_size,
            &_set_parent,       &_set_transient,     &_suggest_title,    &_show,        &_hide,
        };
        return static_cast<const void*>(&value);
    }

   private:
    static bool _is_api_supported(const clap_plugin_t* plugin, const char* api, bool isFloating) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.IsApiSupported(toApiEnum(api), isFloating);
    }

    static bool _get_preferred_api(const clap_plugin_t* plugin, const char** apiString, bool* isFloating) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        WindowingApi api;
        bool success = self.GetPreferredApi(api, *isFloating);
        if (!success) {
            return false;
        }
        switch (api) {
            case WindowingApi::X11: {
                *apiString = CLAP_WINDOW_API_X11;
                break;
            }
            case WindowingApi::Wayland: {
                *apiString = CLAP_WINDOW_API_WAYLAND;
                break;
            }
            case WindowingApi::Win32: {
                *apiString = CLAP_WINDOW_API_WIN32;
                break;
            }
            case WindowingApi::Cocoa: {
                *apiString = CLAP_WINDOW_API_COCOA;
                break;
            }
            case WindowingApi::None: {
                return false;
            }
        }
        return true;
    }

    static bool _create(const clap_plugin_t* plugin, const char* apiString, bool isFloating) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        WindowingApi api = toApiEnum(apiString);
        if (!self.IsApiSupported(api, isFloating)) {
            return false;
        }
        return self.Create(api, isFloating);
    }

    static void _destroy(const clap_plugin_t* plugin) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        self.Destroy();
    }

    static bool _set_scale(const clap_plugin_t* plugin, double scale) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.SetScale(scale);
    }

    static bool _get_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.GetSize(*width, *height);
    }

    static bool _can_resize(const clap_plugin_t* plugin) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.CanResize();
    }

    static bool _get_resize_hints(const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.GetResizeHints(*hints);
    }

    static bool _adjust_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.AdjustSize(*width, *height);
    }

    static bool _set_size(const clap_plugin_t* plugin, uint32_t width, uint32_t height) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.SetSize(width, height);
    }

    static bool _set_parent(const clap_plugin_t* plugin, const clap_window_t* window) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.SetParent(WindowHandle(window));
    }

    static bool _set_transient(const clap_plugin_t* plugin, const clap_window_t* window) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.SetTransient(WindowHandle(window));
    }

    static void _suggest_title(const clap_plugin_t* plugin, const char* title) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        self.SuggestTitle(std::string_view(title));
    }

    static bool _show(const clap_plugin_t* plugin) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.Show();
    }

    static bool _hide(const clap_plugin_t* plugin) {
        GuiExt& self = GuiExt::GetFromPluginObject<GuiExt>(plugin);
        return self.Hide();
    }
};