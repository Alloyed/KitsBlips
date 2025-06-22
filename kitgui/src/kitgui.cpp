#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include "log.h"

namespace kitgui {

void init() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        LOG_SDL_ERROR();
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
}

void deinit() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

}  // namespace kitgui