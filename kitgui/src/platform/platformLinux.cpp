#ifdef __linux__

#include "log.h"
#include "platform/platform.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>

// https://github.com/libsdl-org/SDL/blob/main/src/video/x11/SDL_x11window.c
// NYI: https://github.com/libsdl-org/SDL/blob/main/src/video/wayland/SDL_waylandwindow.c

namespace {
void getX11Handles(SDL_Window* sdlWindow, Window& xWindow, Display*& xDisplay) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    xWindow = SDL_GetNumberProperty(windowProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    xDisplay = static_cast<Display*>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
}
}  // namespace

namespace kitgui {

namespace platform {
namespace sdl {

SDL_Window* wrapWindow(const platform::WindowRef& win) {
    SDL_PropertiesID createProps = SDL_CreateProperties();
    if (createProps == 0) {
        LOG_SDL_ERROR();
        return nullptr;
    }

    if (win.api == Api::X11) {
        SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER,
                              reinterpret_cast<unsigned long>(win.ptr));
    } else if (win.api == Api::Wayland) {
        SDL_SetPointerProperty(createProps, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, win.ptr);
    }
    SDL_Window* wrappedWindow = SDL_CreateWindowWithProperties(createProps);
    SDL_DestroyProperties(createProps);
    return wrappedWindow;
}
}  // namespace sdl

void onCreateWindow(Api api, SDL_Window* sdlWindow) {
    if (api == Api::Wayland) {
        return;
    } else if (api == Api::X11) {
        Window xWindow;
        Display* xDisplay;
        getX11Handles(sdlWindow, xWindow, xDisplay);

        // mark window as embedded
        // https://specifications.freedesktop.org/xembed-spec/0.5/lifecycle.html
        Atom embedInfoAtom = XInternAtom(xDisplay, "_XEMBED_INFO", 0);
        uint32_t embedInfoData[2] = {0 /* version */, 1 /* mapped = true */};
        int32_t bitFormat = 32;
        int32_t numElements = 2;
        XChangeProperty(xDisplay, xWindow, embedInfoAtom, embedInfoAtom, bitFormat, PropModeReplace,
                        (uint8_t*)embedInfoData, numElements);
    }
}

uint64_t createPlatformTimer([[maybe_unused]] std::function<void()> fn) {
    return 0;
}

void cancelPlatformTimer([[maybe_unused]] uint64_t id) {}

bool setParent(Api api, SDL_Window* sdlWindow, SDL_Window* parent) {
    if (api == Api::Wayland) {
        return true;
    } else if (api == Api::X11) {
        Window xWindow;
        Window parentWindow;
        Display* xDisplay;
        getX11Handles(sdlWindow, xWindow, xDisplay);
        getX11Handles(parent, parentWindow, xDisplay);

        XReparentWindow(xDisplay, xWindow, parentWindow, 0, 0);
        // per JUCE:
        // https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp
        // The host is likely to attempt to move/resize the window directly after this call,
        // and we need to ensure that the X server knows that our window has been attached
        // before that happens.
        XFlush(xDisplay);
    }

    return true;
}

bool setTransient(Api api, SDL_Window* sdlWindow, SDL_Window* parent) {
    if (api == Api::Wayland) {
        return true;
    } else if (api == Api::X11) {
        Window xWindow{};
        Window parentWindow{};
        Display* xDisplay{};
        getX11Handles(sdlWindow, xWindow, xDisplay);
        getX11Handles(parent, parentWindow, xDisplay);

        XSetTransientForHint(xDisplay, xWindow, parentWindow);
        return true;
    }

    return true;
}

};  // namespace platform
}  // namespace kitgui
#endif