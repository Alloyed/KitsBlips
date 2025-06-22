#include "kitgui/context.h"
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_video.h>
#include <algorithm>
#include <string>
#include "log.h"
#include "platform/platform.h"

namespace kitgui {

Context::Context(platform::Api api) {}
Context::~Context() {}

bool Context::Init() {
    switch (mApi) {
        // only one API possible on these platforms
        case platform::Api::Any:
        case platform::Api::Cocoa: {
            break;
        }
        case platform::Api::Win32: {
            SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
            break;
        }
        case platform::Api::X11: {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
            break;
        }
        case platform::Api::Wayland: {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
            break;
        }
    }

    SDL_PropertiesID createProps = SDL_CreateProperties();
    if (createProps == 0) {
        LOG_SDL_ERROR();
        return false;
    }

    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 400);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 400);
    mWindow = SDL_CreateWindowWithProperties(createProps);
    ///... is this safe? or do i need to keep it alive?
    SDL_DestroyProperties(createProps);

    if (mWindow == nullptr) {
        LOG_SDL_ERROR();
        return false;
    }
    platform::onCreateWindow(mApi, mWindow);

    SDL_GLContext gl_context = SDL_GL_CreateContext(mWindow);
    if (gl_context == nullptr) {
        LOG_SDL_ERROR();
        return false;
    }
    mSdlGl = gl_context;
    SDL_GL_MakeCurrent(mWindow, mSdlGl);
    int version = gladLoadGLContext(&mGl, (GLADloadfunc)SDL_GL_GetProcAddress);
    printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    return true;
}

bool Context::SetScale(double scale) {
    // TODO: scale font
    ImGui::GetStyle().ScaleAllSizes(static_cast<float>(scale));
    return true;
}
bool Context::GetSize(uint32_t& widthOut, uint32_t& heightOut) {
    int32_t w;
    int32_t h;
    bool success = SDL_GetWindowSize(mWindow, &w, &h);
    widthOut = static_cast<uint32_t>(w);
    heightOut = static_cast<uint32_t>(h);
    return success;
}
bool Context::CanResize() {
    SDL_WindowFlags flags = SDL_GetWindowFlags(mWindow);
    return (flags & SDL_WINDOW_RESIZABLE) > 0;
}
// bool GetResizeHints(clap_gui_resize_hints_t& hintsOut);
bool Context::AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) {
    // apply whatever resize constraint algorithm you'd like to the inOut variables.
    // use ResizeHints to help out the OS.
    // no change means no constraint
    return true;
}
bool Context::SetSize(uint32_t width, uint32_t height) {
    return SDL_SetWindowSize(mWindow, width, height);
}
bool Context::SetParent(SDL_Window* handle) {
    return platform::setParent(mApi, mWindow, handle);
}
bool Context::SetTransient(SDL_Window* handle) {
    return platform::setTransient(mApi, mWindow, handle);
}
void Context::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    SDL_SetWindowTitle(mWindow, titleTemp.c_str());
}
bool Context::Show() {
    if (!SDL_ShowWindow(mWindow)) {
        LOG_SDL_ERROR();
        return false;
    }
    // Setup main loop
    // AddActiveInstance(this);
    return true;
}
bool Context::Hide() {
    // RemoveActiveInstance(this);
    if (!SDL_HideWindow(mWindow)) {
        LOG_SDL_ERROR();
        return false;
    }
}

std::vector<Context*> Context::sActiveInstances = {};

void Context::AddActiveInstance(Context* instance) {
    bool wasEmpty = sActiveInstances.empty();
    if (std::find(sActiveInstances.begin(), sActiveInstances.end(), instance) == sActiveInstances.end()) {
        sActiveInstances.push_back(instance);
    }
    if (wasEmpty && !sActiveInstances.empty()) {
        // if (sUpdateTimerId) {
        //     platformGui::cancelGuiTimer(instance->mHost, sUpdateTimerId);
        //     sUpdateTimerId = 0;
        // }

        // constexpr int32_t timerMs = 32;  // 30fps
        // sUpdateTimerId = platformGui::addGuiTimer(instance->mHost, timerMs, &UpdateInstances);
    }
}

void Context::RemoveActiveInstance(Context* instance) {
    bool wasEmpty = sActiveInstances.empty();
    const auto iter = std::find(sActiveInstances.begin(), sActiveInstances.end(), instance);
    if (iter != sActiveInstances.end()) {
        sActiveInstances.erase(iter);
    }
    if (!wasEmpty && sActiveInstances.empty()) {
        // if (sUpdateTimerId) {
        //     platformGui::cancelGuiTimer(instance->mHost, sUpdateTimerId);
        //     sUpdateTimerId = 0;
        // }
    }
}
Context* Context::FindContextForWindow(SDL_WindowID window) {
    for (Context* instance : sActiveInstances) {
        if (SDL_GetWindowID(instance->mWindow) == window) {
            return instance;
        }
    }
    return nullptr;
}
}  // namespace kitgui