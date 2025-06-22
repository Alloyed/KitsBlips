#pragma once

#if KITSBLIPS_ENABLE_GUI

#include <kitgui/context.h>
#include <kitgui/kitgui.h>
#include <string_view>
#include "clapeze/ext/gui.h"

namespace clapeze {

class KitguiExt : public clapeze::GuiExt {
   public:
    bool IsApiSupported(ClapWindowApi api, bool isFloating) override {
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
    bool GetPreferredApi(ClapWindowApi& apiOut, bool& isFloatingOut) override {
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
    bool Create(ClapWindowApi api, bool isFloating) override {
        sInitCount++;
        if (sInitCount == 1) {
            kitgui::init();
        }

        return mCtx.Create(ToPlatformApi(api), isFloating);
    }

    void Destroy() override {
        mCtx.Destroy();
        sInitCount--;
        if (sInitCount == 1) {
            kitgui::deinit();
        }
    }
    bool SetScale(double scale) override { return mCtx.SetScale(scale); }
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) override { return mCtx.GetSize(widthOut, heightOut); }
    bool CanResize() override { return mCtx.CanResize(); }
    bool GetResizeHints(clap_gui_resize_hints_t& hintsOut) override {
        // TODO
        return true;
    }
    bool AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) override {
        return mCtx.AdjustSize(widthInOut, heightInOut);
    }
    bool SetSize(uint32_t width, uint32_t height) override { return mCtx.SetSize(width, height); }
    bool SetParent(WindowHandle handle) override {
        SDL_Window* desiredParent = kitgui::platform::wrapWindow(ToPlatformApi(handle.api), handle.ptr);
        return mCtx.SetParent();
    }
    bool SetTransient(WindowHandle handle) override {
        SDL_Window* desiredTransient = kitgui::platform::wrapWindow(ToPlatformApi(handle.api), handle.ptr);
        return mCtx.SetTransient(desiredTransient);
    }
    void SuggestTitle(std::string_view title) override { mCtx.SuggestTitle(title); }
    bool Show() override { mCtx.Show(); }
    bool Hide() override { mCtx.Hide(); }

   private:
    kitgui::Context mCtx;
    static int32_t sInitCount;

    static kitgui::platform::Api ToPlatformApi(ClapWindowApi api) {
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
    }
};
}  // namespace clapeze
#endif