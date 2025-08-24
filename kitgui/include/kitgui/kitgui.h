#pragma once

struct SDL_Window;

namespace kitgui {
void init();
void deinit();
namespace platform {
enum class Api { Any, Win32, Cocoa, X11, Wayland };
struct WindowRef {
    Api api;
    void* ptr;
};
inline WindowRef wrapWindow(Api api, void* apiWindow) {
    return {api, apiWindow};
}
}  // namespace platform
}  // namespace kitgui