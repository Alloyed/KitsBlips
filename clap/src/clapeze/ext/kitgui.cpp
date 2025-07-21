#if KITSBLIPS_ENABLE_GUI

#include "clapeze/ext/kitgui.h"

namespace clapeze {

int32_t KitguiFeature::sInitCount = 0;

KitguiFeature::KitguiFeature(kitgui::Context::AppFactory createAppFn) : mCtx(createAppFn) {}

bool KitguiFeature::IsApiSupported(ClapWindowApi api, bool isFloating) {
    // TODO: lots of apis to support out there
    if (isFloating) {
        // we can get away with using all native SDL
        return true;
    }

    // embedding requires API-specific code, still a work in progress
    switch (api) {
        case _None:
        case Wayland:
        case Win32:
        case Cocoa: {
            return false;
        }
        case X11: {
            return true;
        }
    }
    return false;
}

bool KitguiFeature::GetPreferredApi(ClapWindowApi& apiOut, bool& isFloatingOut) {
    kitgui::platform::Api guiApi;
    kitgui::Context::GetPreferredApi(guiApi, isFloatingOut);
    switch (guiApi) {
        case kitgui::platform::Api::Any: {
            apiOut = ClapWindowApi::_None;
            break;
        }
        case kitgui::platform::Api::Win32: {
            apiOut = ClapWindowApi::Win32;
            break;
        }
        case kitgui::platform::Api::Cocoa: {
            apiOut = ClapWindowApi::Cocoa;
            break;
        }
        case kitgui::platform::Api::X11: {
            apiOut = ClapWindowApi::X11;
            break;
        }
        case kitgui::platform::Api::Wayland: {
            apiOut = ClapWindowApi::Wayland;
            break;
        }
    }
    return true;
}

bool KitguiFeature::Create(ClapWindowApi api, bool isFloating) {
    sInitCount++;
    if (sInitCount == 1) {
        kitgui::init();
    }

    return mCtx.Create(ToPlatformApi(api), isFloating);
}

void KitguiFeature::Destroy() {
    mCtx.Destroy();
    sInitCount--;
    if (sInitCount == 1) {
        kitgui::deinit();
    }
}

bool KitguiFeature::SetScale(double scale) {
    return mCtx.SetScale(scale);
}

bool KitguiFeature::GetSize(uint32_t& widthOut, uint32_t& heightOut) {
    return mCtx.GetSize(widthOut, heightOut);
}

bool KitguiFeature::CanResize() {
    return mCtx.GetSizeConfig().resizable;
}

bool KitguiFeature::GetResizeHints(clap_gui_resize_hints_t& hintsOut) {
    const auto& cfg = mCtx.GetSizeConfig();
    hintsOut.preserve_aspect_ratio = cfg.preserveAspectRatio;
    hintsOut.aspect_ratio_width = cfg.startingWidth;
    hintsOut.aspect_ratio_height = cfg.startingHeight;
    hintsOut.can_resize_horizontally = cfg.resizable;
    hintsOut.can_resize_vertically = cfg.resizable;
    return true;
}

bool KitguiFeature::AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) {
    const auto& cfg = mCtx.GetSizeConfig();
    if (cfg.resizable && cfg.preserveAspectRatio) {
        float ratio = static_cast<float>(cfg.startingWidth) / static_cast<float>(cfg.startingHeight);
        heightInOut = widthInOut * ratio;
    }
    return true;
}

bool KitguiFeature::SetSize(uint32_t width, uint32_t height) {
    return mCtx.SetSizeDirectly(width, height);
}

bool KitguiFeature::SetParent(WindowHandle handle) {
    SDL_Window* desiredParent = kitgui::platform::wrapWindow(ToPlatformApi(handle.api), handle.ptr);
    return mCtx.SetParent(desiredParent);
}

bool KitguiFeature::SetTransient(WindowHandle handle) {
    SDL_Window* desiredTransient = kitgui::platform::wrapWindow(ToPlatformApi(handle.api), handle.ptr);
    return mCtx.SetTransient(desiredTransient);
}

void KitguiFeature::SuggestTitle(std::string_view title) {
    mCtx.SuggestTitle(title);
}

bool KitguiFeature::Show() {
    return mCtx.Show();
}

bool KitguiFeature::Hide() {
    return mCtx.Hide();
}

kitgui::platform::Api KitguiFeature::ToPlatformApi(ClapWindowApi api) {
    switch (api) {
        case clapeze::ClapWindowApi::_None: {
            return kitgui::platform::Api::Any;
        }
        case clapeze::ClapWindowApi::Win32: {
            return kitgui::platform::Api::Win32;
        }
        case clapeze::ClapWindowApi::Cocoa: {
            return kitgui::platform::Api::Cocoa;
        }
        case clapeze::ClapWindowApi::X11: {
            return kitgui::platform::Api::X11;
        }
        case clapeze::ClapWindowApi::Wayland: {
            return kitgui::platform::Api::Wayland;
        }
    }
    assert(false);
    return kitgui::platform::Api::Any;
}

}  // namespace clapeze

#endif