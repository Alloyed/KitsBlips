#pragma once

#include "clapApi/ext/gui.h"
#include <SDL3/SDL_video.h>
#include <memory>

// using pimpl to hide platform details.
class Platform {
    private:
    class Impl;
    std::unique_ptr<Impl> mImpl;
    public:
    Platform();
    ~Platform();
    SDL_Window* Create();
    void Destroy();
    bool SetParent(const WindowHandle& parent);
    bool SetTransient(const WindowHandle& parent);
};