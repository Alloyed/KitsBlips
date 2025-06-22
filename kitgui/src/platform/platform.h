#pragma once

#include <SDL3/SDL_video.h>
#include <kitgui/kitgui.h>

namespace kitgui {
namespace platform {
void onCreateWindow(Api api, SDL_Window* window);
bool setParent(Api api, SDL_Window* window, SDL_Window* parent);
bool setTransient(Api api, SDL_Window* window, SDL_Window* parent);
SDL_Window* wrapWindow(Api api, void* apiWindow);
}  // namespace platform
}  // namespace kitgui