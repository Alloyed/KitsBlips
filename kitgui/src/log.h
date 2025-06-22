#pragma once
#include <SDL3/SDL.h>
#include <iostream>

#define LOG_SDL_ERROR()                                                      \
    do {                                                                     \
        std::cerr << __FILE__ << ": " << __LINE__ << ": " << SDL_GetError(); \
    } while (0)