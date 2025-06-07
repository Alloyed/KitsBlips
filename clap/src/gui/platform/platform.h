#pragma once

#include "clapApi/ext/gui.h"
#include <SDL3/SDL_video.h>


namespace PlatformGui {
    void onCreateWindow(SDL_Window* window);
    bool setParent(SDL_Window* window, const WindowHandle& parent);
    bool setTransient(SDL_Window* window, const WindowHandle& parent);
}