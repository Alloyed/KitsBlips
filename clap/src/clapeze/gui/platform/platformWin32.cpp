#ifdef _WIN32

#include "clapeze/gui/platform/platform.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <windows.h>
#include "clapeze/ext/gui.h"

namespace {
void getPlatformHandles(SDL_Window* sdlWindow, HWND& hWindow) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    hWindow = static_cast<HWND>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, 0));
}
}  // namespace

namespace clapeze {

namespace platformGui {
void onCreateWindow(ClapWindowApi _api, SDL_Window* sdlWindow) {
    // do nothing
    (void)sdlWindow;
}

bool setParent(ClapWindowApi _api, SDL_Window* sdlWindow, const WindowHandle& parent) {
    HWND childWindow;
    getPlatformHandles(sdlWindow, childWindow);
    HWND parentWindow = static_cast<HWND>(parent.ptr);

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent?redirectedfrom=MSDN#remarks
    // per documentation, we need to manually update the style to reflect child status
    const DWORD style = GetWindowLong(childWindow, GWL_STYLE);
    SetWindowLong(childWindow, GWL_STYLE, (style | WS_CHILD) ^ WS_POPUP);
    SetParent(childWindow, parentWindow);

    return true;
}

bool setTransient(ClapWindowApi _api, SDL_Window* sdlWindow, const WindowHandle& parent) {
    // same implementation as setParent()
    setParent(_api, sdlWindow, parent);

    return true;
}

clap_id addGuiTimer(PluginHost& host, int32_t periodMs, void (*fn)()) {
    TIMERPROC winFn = (TIMERPROC)(fn);
    UINT_PTR id = SetTimer(nullptr, 0, periodMs, winFn);
    return id;
}

void cancelGuiTimer(PluginHost& host, clap_id id) {
    KillTimer(nullptr, id);
}
}  // namespace platformGui
}  // namespace clapeze
#endif