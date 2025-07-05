#pragma once

struct SDL_Window;

namespace kitgui {
void init();
void deinit();
namespace platform {
enum class Api { Any, Win32, Cocoa, X11, Wayland };
SDL_Window* wrapWindow(Api api, void* apiWindow);
}  // namespace platform
}  // namespace kitgui