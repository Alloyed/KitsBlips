#include "clapeze/gui/sdlOpenGlExt.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <algorithm>

#include "clap/ext/gui.h"
#include "clapeze/ext/gui.h"
#include "clapeze/pluginHost.h"
#include "clapeze/gui/platform/platform.h"

bool SdlOpenGlExt::MakeCurrent() {
    return SDL_GL_MakeCurrent(mWindow, mCtx);
}

bool SdlOpenGlExt::IsApiSupported(ClapWindowApi api, bool isFloating) {
    // TODO: lots of apis to support out there
    if (isFloating) {
        // we can get away with using all native SDL
        return true;
    }

    // embedding requires API-specific code, still a work in progress
    switch (api) {
        case _None:
        case Wayland:
        case Cocoa: {
            return false;
        }
        case Win32:
        case X11: {
            return true;
        }
    }
    return false;
}

bool SdlOpenGlExt::GetPreferredApi(ClapWindowApi& outApi, bool& outIsFloating) {
    std::string_view platformName = SDL_GetPlatform();
    if (platformName == "Windows") {
        outApi = ClapWindowApi::Win32;
    } else if (platformName == "macOS") {
        outApi = ClapWindowApi::Cocoa;
    } else if (platformName == "Linux") {
        outApi = ClapWindowApi::X11;
    }

    outIsFloating = false;
    return true;
}

bool SdlOpenGlExt::Create(ClapWindowApi api, bool isFloating) {
    switch (api) {
        // only one API possible on these platforms
        case Cocoa:
        case _None: {
            break;
        }
        case Win32: {
            SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
            break;
        }
        case X11: {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
            break;
        }
        case Wayland: {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
            break;
        }
    }

    mApi = api;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        CLAPEZE_LOG_SDL_ERROR(this);
        return false;
    }

    //  GL 3.0 + GLSL 130
    //  const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_PropertiesID createProps = SDL_CreateProperties();
    if (createProps == 0) {
        CLAPEZE_LOG_SDL_ERROR(this);
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
        CLAPEZE_LOG_SDL_ERROR(this);
        return false;
    }
    platformGui::onCreateWindow(mApi, mWindow);

    SDL_GLContext gl_context = SDL_GL_CreateContext(mWindow);
    if (gl_context == nullptr) {
        CLAPEZE_LOG_SDL_ERROR(this);
        return false;
    }
    mCtx = gl_context;
    SDL_GL_MakeCurrent(mWindow, mCtx);

    return true;
}

void SdlOpenGlExt::Destroy() {
    if (!MakeCurrent()) {
        CLAPEZE_LOG_SDL_ERROR(this);
    }
    RemoveActiveInstance(this);
    SDL_GL_DestroyContext(mCtx);
    SDL_DestroyWindow(mWindow);
    mWindow = nullptr;
    mCtx = nullptr;

    // pick any valid current engine for later: this works around a bug i don't fully understand in SDL3 itself :x
    // without this block, closing a window when multiple are active causes a crash in the main update loop when we call
    // SDL_GL_MakeCurrent(). maybe a required resource is already destroyed by that point?
    for (auto instance : sActiveInstances) {
        if (instance->mWindow) {
            if (!instance->MakeCurrent()) {
                CLAPEZE_LOG_SDL_ERROR(instance);
            }
            break;
        }
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool SdlOpenGlExt::SetScale(double scale) {
    // unused. should be communicated to ui toolkit
    return true;
}
bool SdlOpenGlExt::GetSize(uint32_t& outWidth, uint32_t& outHeight) {
    int32_t w;
    int32_t h;
    bool success = SDL_GetWindowSize(mWindow, &w, &h);
    outWidth = static_cast<uint32_t>(w);
    outHeight = static_cast<uint32_t>(h);
    return success;
}
bool SdlOpenGlExt::CanResize() {
    SDL_WindowFlags flags = SDL_GetWindowFlags(mWindow);
    return (flags & SDL_WINDOW_RESIZABLE) > 0;
}
bool SdlOpenGlExt::GetResizeHints(clap_gui_resize_hints_t& outResizeHints) {
    // override to force constrained resize
    outResizeHints.can_resize_horizontally = CanResize();
    outResizeHints.can_resize_vertically = CanResize();
    outResizeHints.preserve_aspect_ratio = false;
    outResizeHints.aspect_ratio_width = 1;
    outResizeHints.aspect_ratio_height = 1;
    return true;
}
bool SdlOpenGlExt::AdjustSize(uint32_t& inOutWidth, uint32_t& inOutHeight) {
    // apply whatever resize constraint algorithm you'd like to the inOut variables.
    // use ResizeHints to help out the OS.
    // no change means no constraint
    return true;
}
bool SdlOpenGlExt::SetSize(uint32_t width, uint32_t height) {
    return SDL_SetWindowSize(mWindow, width, height);
}
bool SdlOpenGlExt::SetParent(WindowHandle handle) {
    return platformGui::setParent(mApi, mWindow, handle);
}
bool SdlOpenGlExt::SetTransient(WindowHandle handle) {
    return platformGui::setTransient(mApi, mWindow, handle);
}
void SdlOpenGlExt::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    SDL_SetWindowTitle(mWindow, titleTemp.c_str());
}
bool SdlOpenGlExt::Show() {
    if (!SDL_ShowWindow(mWindow)) {
        CLAPEZE_LOG_SDL_ERROR(this);
        return false;
    }
    // Setup main loop
    AddActiveInstance(this);
    return true;
}
bool SdlOpenGlExt::Hide() {
    RemoveActiveInstance(this);
    if (!SDL_HideWindow(mWindow)) {
        CLAPEZE_LOG_SDL_ERROR(this);
        return false;
    }
    return true;
}

PluginHost::TimerId SdlOpenGlExt::sUpdateTimerId = 0;
std::vector<SdlOpenGlExt*> SdlOpenGlExt::sActiveInstances = {};

void SdlOpenGlExt::AddActiveInstance(SdlOpenGlExt* instance) {
    bool wasEmpty = sActiveInstances.empty();
    if (std::find(sActiveInstances.begin(), sActiveInstances.end(), instance) == sActiveInstances.end()) {
        sActiveInstances.push_back(instance);
    }
    if (wasEmpty && !sActiveInstances.empty()) {
        if (sUpdateTimerId) {
            platformGui::cancelGuiTimer(instance->mHost, sUpdateTimerId);
            sUpdateTimerId = 0;
        }

        sUpdateTimerId = platformGui::addGuiTimer(instance->mHost, 16, &UpdateInstances);
    }
}

void SdlOpenGlExt::RemoveActiveInstance(SdlOpenGlExt* instance) {
    bool wasEmpty = sActiveInstances.empty();
    const auto iter = std::find(sActiveInstances.begin(), sActiveInstances.end(), instance);
    if (iter != sActiveInstances.end()) {
        sActiveInstances.erase(iter);
    }
    if (!wasEmpty && sActiveInstances.empty()) {
        if (sUpdateTimerId) {
            platformGui::cancelGuiTimer(instance->mHost, sUpdateTimerId);
            sUpdateTimerId = 0;
        }
    }
}

SdlOpenGlExt* SdlOpenGlExt::FindInstanceForWindow(SDL_WindowID window) {
    for (SdlOpenGlExt* instance : sActiveInstances) {
        if (SDL_GetWindowID(instance->mWindow) == window) {
            return instance;
        }
    }
    return nullptr;
}

void SdlOpenGlExt::UpdateInstances() {
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_WINDOW_DESTROYED: {
                goto skip_event;
            }
            case SDL_EVENT_QUIT: {
                SdlOpenGlExt* instance = sActiveInstances.front();
                if (instance) {
                    const clap_host_t* rawHost;
                    const clap_host_gui_t* rawHostGui;
                    instance->mHost.TryGetExtension(CLAP_EXT_GUI, rawHost, rawHostGui);
                    rawHostGui->closed(rawHost, true);
                }
            }
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                SdlOpenGlExt* instance = FindInstanceForWindow(event.window.windowID);
                if (instance) {
                    const clap_host_t* rawHost;
                    const clap_host_gui_t* rawHostGui;
                    instance->mHost.TryGetExtension(CLAP_EXT_GUI, rawHost, rawHostGui);
                    rawHostGui->closed(rawHost, true);
                }
            }
        }
        for (auto instance : sActiveInstances) {
            if (!instance->MakeCurrent()) {
                CLAPEZE_LOG_SDL_ERROR(instance);
                goto skip_event;
            }
            instance->OnEvent(event);
        }
    skip_event:
        continue;
    }

    // new frame
    for (auto instance : sActiveInstances) {
        if (!instance->MakeCurrent()) {
            CLAPEZE_LOG_SDL_ERROR(instance);
            continue;
        }
        instance->Update();

        uint32_t width, height;
        instance->GetSize(width, height);
        glViewport(0, 0, width, height);
        glClearColor(0, 1.0f, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        instance->Draw();
        SDL_GL_SwapWindow(instance->mWindow);
    }
}