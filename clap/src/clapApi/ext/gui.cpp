/*
namespace GuiExt {
    Gui::WindowingApi toApiEnum(const char* api) 
    {
        // clap promises that equal api types are the same pointer
        if(api == CLAP_WINDOW_API_X11) { return Gui::WindowingApi::X11; }
        if(api == CLAP_WINDOW_API_WAYLAND) { return Gui::WindowingApi::Wayland; }
        if(api == CLAP_WINDOW_API_WIN32) { return Gui::WindowingApi::Win32; }
        if(api == CLAP_WINDOW_API_COCOA) { return Gui::WindowingApi::Cocoa; }
        // should never happen
        return Gui::WindowingApi::None;
    }

    bool is_api_supported (const clap_plugin_t *plugin, const char *api, bool isFloating) {
        return Gui::IsApiSupported(toApiEnum(api), isFloating);
    }

    bool get_preferred_api(const clap_plugin_t *plugin, const char **apiString, bool *isFloating)  {
        Gui::WindowingApi api;
        bool success = Gui::GetPreferredApi(api, *isFloating);
        if(!success){return false;}
        switch(api) {
            case Gui::WindowingApi::X11: { *apiString = CLAP_WINDOW_API_X11; break; }
            case Gui::WindowingApi::Wayland: { *apiString = CLAP_WINDOW_API_WAYLAND; break; }
            case Gui::WindowingApi::Win32: { *apiString = CLAP_WINDOW_API_WIN32; break; }
            case Gui::WindowingApi::Cocoa: { *apiString = CLAP_WINDOW_API_COCOA; break; }
            case Gui::WindowingApi::None: { return false; }
        }
        return true;
    }

    bool create(const clap_plugin_t *plugin, const char *apiString, bool isFloating) {
        Gui::WindowingApi api = toApiEnum(apiString);
        if (!Gui::IsApiSupported(api, isFloating)) {
            return false;
        }
        GetSharedState(plugin)->mGui = std::make_unique<Gui>(
                GetSharedState(plugin)->mHost.get(), api, isFloating);
        return true;
    }

    void destroy (const clap_plugin_t *plugin) {
        GetSharedState(plugin)->mGui.reset();
    }

    bool set_scale (const clap_plugin_t *plugin, double scale)  {
        return GetSharedState(plugin)->mGui->SetScale(scale);
    }

    bool get_size (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)  {
        return GetSharedState(plugin)->mGui->GetSize(*width, *height);
    }

    bool can_resize (const clap_plugin_t *plugin)  {
        return GetSharedState(plugin)->mGui->CanResize();
    }

    bool get_resize_hints (const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints)  {
        Gui::ResizeHints guiHints;
        bool success = GetSharedState(plugin)->mGui->GetResizeHints(guiHints);
        if(!success) {
            return false;
        }
        hints->can_resize_horizontally = guiHints.canResizeHorizontally;
        hints->can_resize_vertically = guiHints.canResizeVertically;
        hints->preserve_aspect_ratio = guiHints.preserveAspectRatio;
        hints->aspect_ratio_width = guiHints.aspectRatioWidth;
        hints->aspect_ratio_height = guiHints.aspectRatioHeight;
        return true;
    }

    bool adjust_size (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)  {
        return GetSharedState(plugin)->mGui->AdjustSize(*width, *height);
    }

    bool set_size (const clap_plugin_t *plugin, uint32_t width, uint32_t height)  {
        return GetSharedState(plugin)->mGui->SetSize(width, height);
    }

    bool set_parent (const clap_plugin_t *plugin, const clap_window_t *window)  {
        Gui::WindowingApi api = toApiEnum(window->api);
        return GetSharedState(plugin)->mGui->SetParent(api, window->ptr);
    }

    bool set_transient (const clap_plugin_t *plugin, const clap_window_t *window)  {
        Gui::WindowingApi api = toApiEnum(window->api);
        return GetSharedState(plugin)->mGui->SetTransient(api, window->ptr);
    }

    void suggest_title (const clap_plugin_t *plugin, const char *title) {
        GetSharedState(plugin)->mGui->SuggestTitle(title);
    }

    bool show (const clap_plugin_t *plugin)  {
        return GetSharedState(plugin)->mGui->Show();
    }

    bool hide (const clap_plugin_t *plugin)  {
        return GetSharedState(plugin)->mGui->Hide();
    }

    const clap_plugin_gui_t value = {
        is_api_supported,
        get_preferred_api,
        create,
        destroy,
        set_scale,
        get_size,
        can_resize,
        get_resize_hints,
        adjust_size,
        set_size,
        set_parent,
        set_transient,
        suggest_title,
        show,
        hide,
    };
}
*/
