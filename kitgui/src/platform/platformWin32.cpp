#ifdef _WIN32

#include "platform/platform.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <windows.h>
#include <functional>
#include <unordered_map>
#include "log.h"

namespace {
void getPlatformHandles(SDL_Window* sdlWindow, HWND& hWindow) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    hWindow = static_cast<HWND>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_WIN32_HWND_POINTER, 0));
}

std::unordered_map<uint64_t, std::function<void()>> sStoredCallbacks{};
bool kitgui_MessageHook(void* userdata, MSG* msg) {
    if (msg != nullptr && msg->message == WM_TIMER && msg->hwnd == nullptr) {
        uint64_t id = msg->wParam;
        auto iter = sStoredCallbacks.find(id);
        if (iter != sStoredCallbacks.end()) {
            iter->second();
        }
    }

    // pass message on
    return true;
}

}  // namespace

namespace kitgui {

namespace platform {
namespace sdl {

SDL_Window* wrapWindow(const Window& win) {
    SDL_PropertiesID createProps = SDL_CreateProperties();
    if (createProps == 0) {
        LOG_SDL_ERROR();
        return nullptr;
    }

    SDL_SetPointerProperty(createProps, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, win.ptr);
    SDL_Window* wrappedWindow = SDL_CreateWindowWithProperties(createProps);
    SDL_DestroyProperties(createProps);
    return wrappedWindow;
}
}  // namespace sdl

void onCreateWindow(Api api, SDL_Window* sdlWindow) {
    // do nothing
    (void)api;
    (void)sdlWindow;
    SDL_SetWindowsMessageHook(kitgui_MessageHook, nullptr);
}

uint64_t createPlatformTimer(std::function<void()> fn) {
    uint32_t timeoutMs = 10;
    auto id = SetTimer(nullptr, 0, timeoutMs, nullptr);
    sStoredCallbacks.insert({id, fn});
    return id;
}

void cancelPlatformTimer(uint64_t id) {
    KillTimer(nullptr, id);
    sStoredCallbacks.erase(id);
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
}  // namespace platform
}  // namespace kitgui
#endif