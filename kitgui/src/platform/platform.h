#pragma once

#include <SDL3/SDL_video.h>
#include <kitgui/kitgui.h>

namespace kitgui {
namespace platform {
void onCreateWindow(Api api, SDL_Window* window);
bool setParent(Api api, SDL_Window* window, SDL_Window* parent);
bool setTransient(Api api, SDL_Window* window, SDL_Window* parent);
}  // namespace platform
}  // namespace kitgui