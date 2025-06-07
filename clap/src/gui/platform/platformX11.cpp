#ifdef __linux__

#include "gui/platform/platform.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include "clapApi/ext/gui.h"

// https://github.com/libsdl-org/SDL/blob/main/src/video/x11/SDL_x11window.c
// NYI: https://github.com/libsdl-org/SDL/blob/main/src/video/wayland/SDL_waylandwindow.c

namespace {
void getPlatformHandles(SDL_Window* sdlWindow, Window& xWindow, Display*& xDisplay) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    xWindow = SDL_GetNumberProperty(windowProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    xDisplay = static_cast<Display*>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
}
}  // namespace

namespace PlatformGui {
void onCreateWindow(ClapWindowApi api, SDL_Window* sdlWindow) {
    if(api == ClapWindowApi::Wayland)
    {
        return;
    }
    Window xWindow;
    Display* xDisplay;
    getPlatformHandles(sdlWindow, xWindow, xDisplay);

    // mark window as embedded
    // https://specifications.freedesktop.org/xembed-spec/0.5/lifecycle.html
    Atom embedInfoAtom = XInternAtom(xDisplay, "_XEMBED_INFO", 0);
    uint32_t embedInfoData[2] = {0 /* version */, 1 /* mapped = true */};
    int32_t bitFormat = 32;
    int32_t numElements = 2;
    XChangeProperty(xDisplay, xWindow, embedInfoAtom, embedInfoAtom, bitFormat, PropModeReplace,
                    (uint8_t*)embedInfoData, numElements);
}

bool setParent(ClapWindowApi api, SDL_Window* sdlWindow, const WindowHandle& parent) {
    if(api == ClapWindowApi::Wayland)
    {
        return true;
    }
    Window xWindow;
    Display* xDisplay;
    getPlatformHandles(sdlWindow, xWindow, xDisplay);
    Window parentWindow = static_cast<Window>(parent.x11);

    XReparentWindow(xDisplay, xWindow, parentWindow, 0, 0);
    // per JUCE:
    // https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp
    // The host is likely to attempt to move/resize the window directly after this call,
    // and we need to ensure that the X server knows that our window has been attached
    // before that happens.
    XFlush(xDisplay);

    return true;
}

bool setTransient(ClapWindowApi api, SDL_Window* sdlWindow, const WindowHandle& parent) {
    if(api == ClapWindowApi::Wayland)
    {
        return true;
    }
    Window xWindow;
    Display* xDisplay;
    getPlatformHandles(sdlWindow, xWindow, xDisplay);
    Window parentWindow = static_cast<Window>(parent.x11);

    XSetTransientForHint(xDisplay, xWindow, parentWindow);

    return true;
}
};  // namespace PlatformGui
#endif