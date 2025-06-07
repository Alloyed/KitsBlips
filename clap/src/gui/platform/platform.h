#pragma once

#include "clapApi/ext/gui.h"
#include <SDL3/SDL_video.h>

namespace PlatformGui {
    void onCreateWindow(ClapWindowApi api, SDL_Window* window);
    bool setParent(ClapWindowApi api, SDL_Window* window, const WindowHandle& parent);
    bool setTransient(ClapWindowApi api, SDL_Window* window, const WindowHandle& parent);
}