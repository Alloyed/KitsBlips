#pragma once

#include "clapApi/ext/gui.h"
#include "clapApi/pluginHost.h"
#include <SDL3/SDL_video.h>

namespace platformGui {
    void onCreateWindow(ClapWindowApi api, SDL_Window* window);
    bool setParent(ClapWindowApi api, SDL_Window* window, const WindowHandle& parent);
    bool setTransient(ClapWindowApi api, SDL_Window* window, const WindowHandle& parent);
    clap_id addGuiTimer(PluginHost& host, int32_t delayMs, void (*fn)());
    void cancelGuiTimer(PluginHost& host, clap_id id);
}