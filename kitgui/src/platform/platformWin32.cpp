#ifdef _WIN32

#include "platform/platform.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <windows.h>

namespace {
void getPlatformHandles(SDL_Window* sdlWindow, HWND& hWindow) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    hWindow = static_cast<HWND>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, 0));
}
}  // namespace

namespace kitgui {

namespace platform {
void onCreateWindow(Api api, SDL_Window* sdlWindow) {
    // do nothing
    (void)api;
    (void)sdlWindow;
}

bool setParent(Api api, SDL_Window* sdlWindow, SDL_Window* parent) {
    (void)api;
    HWND childWindow;
    HWND parentWindow;
    getPlatformHandles(sdlWindow, childWindow);
    getPlatformHandles(parent, parentWindow);

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent?redirectedfrom=MSDN#remarks
    // per documentation, we need to manually update the style to reflect child status
    const DWORD style = GetWindowLong(childWindow, GWL_STYLE);
    SetWindowLong(childWindow, GWL_STYLE, (style | WS_CHILD) ^ WS_POPUP);
    SetParent(childWindow, parentWindow);

    return true;
}

bool setTransient(Api _api, SDL_Window* sdlWindow, SDL_Window* parent) {
    // same implementation as setParent()
    setParent(_api, sdlWindow, parent);

    return true;
}
}  // namespace platformGui
}  // namespace kitgui
#endif