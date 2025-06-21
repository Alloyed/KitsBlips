#pragma once

#include <SDL3/SDL_video.h>
#include "clapeze/ext/gui.h"
#include "clapeze/pluginHost.h"

namespace clapeze {
namespace platformGui {
void onCreateWindow(ClapWindowApi api, SDL_Window* window);
bool setParent(ClapWindowApi api, SDL_Window* window, const WindowHandle& parent);
bool setTransient(ClapWindowApi api, SDL_Window* window, const WindowHandle& parent);
clap_id addGuiTimer(PluginHost& host, int32_t delayMs, void (*fn)());
void cancelGuiTimer(PluginHost& host, clap_id id);
}  // namespace platformGui
}  // namespace clapeze