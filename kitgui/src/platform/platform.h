#pragma once

#include <SDL3/SDL_video.h>
#include <kitgui/kitgui.h>
#include <functional>

namespace kitgui {
namespace platform {
void onCreateWindow(Api api, SDL_Window* window);
bool setParent(Api api, SDL_Window* window, SDL_Window* parent);
bool setTransient(Api api, SDL_Window* window, SDL_Window* parent);
uint64_t createPlatformTimer(std::function<void()> fn);
void cancelPlatformTimer(uint64_t id);
namespace sdl {
SDL_Window* wrapWindow(const WindowRef& window);
}  // namespace sdl
}  // namespace platform
}  // namespace kitgui