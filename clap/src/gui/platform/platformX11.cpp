#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <memory>
#include "clapApi/ext/gui.h"
#ifdef __linux__

#include "gui/platform/platform.h"
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <SDL3/SDL_log.h>

struct Platform::Impl {
    Display* mDisplay;
    Window mWindow;
    SDL_Window* mSdlWindow;

    SDL_Window* Create() {
        SDL_PropertiesID createProps = SDL_CreateProperties();
        if (createProps == 0) {
            return nullptr;
        }

        SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
        SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 400);
        SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 400);
        mSdlWindow = SDL_CreateWindowWithProperties(createProps);
        ///... is this safe? or do i need to keep it alive?
        SDL_DestroyProperties(createProps);
        if (mSdlWindow == nullptr) {
            SDL_Log("SDL_CreateWindowWithProperties(): %s", SDL_GetError());
            return nullptr;
        }
        SDL_PropertiesID windowProps = SDL_GetWindowProperties(mSdlWindow);
        mWindow = SDL_GetNumberProperty(windowProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        mDisplay = static_cast<Display*>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));

        // mark window as embedded
        Atom embedInfoAtom = XInternAtom(mDisplay, "_XEMBED_INFO", 0);
        uint32_t embedInfoData[2] = {0 /* version */, 1 /* mapped */};
        XChangeProperty(mDisplay, mWindow, embedInfoAtom, embedInfoAtom, 32, PropModeReplace, (uint8_t*)embedInfoData, 2);

        return mSdlWindow;
    }

    bool SetParent(const WindowHandle& parent) {
        Window parentWindow = static_cast<Window>(parent.x11);
        XReparentWindow(mDisplay, mWindow, parentWindow, 0, 0);
        // per JUCE: https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp
        // The host is likely to attempt to move/resize the window directly after this call,
        // and we need to ensure that the X server knows that our window has been attached
        // before that happens.
        XFlush(mDisplay);
        return true;
    }

    bool SetTransient(const WindowHandle& parent)
    {
        Window parentWindow = static_cast<Window>(parent.x11);
        XSetTransientForHint(mDisplay, mWindow, parentWindow);
        return true;
    }

    void Destroy() {
        // resources obtained from SDL, will be cleaned up by them
        mWindow = 0;
        mDisplay = nullptr;
        SDL_DestroyWindow(mSdlWindow);
    }
    
};

Platform::Platform() : mImpl(std::make_unique<Platform::Impl>()) { printf("ctor\n");}
Platform::~Platform() = default;
SDL_Window* Platform::Create() { printf("AAAA\n");return mImpl->Create(); }
void Platform::Destroy() { mImpl->Destroy(); }
bool Platform::SetParent(const WindowHandle& parent) { return mImpl->SetParent(parent); }
bool Platform::SetTransient(const WindowHandle& parent) { return mImpl->SetTransient(parent); }
#endif