#include "sdlImgui.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include "clap/ext/gui.h"
#include "clapApi/ext/gui.h"
#include "clapApi/pluginHost.h"
#include "gui/platform/platform.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

bool SdlImguiExt::IsApiSupported(ClapWindowApi api, bool isFloating) {
    // TODO: lots of apis to support out there
    if(isFloating)
    {
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

bool SdlImguiExt::GetPreferredApi(ClapWindowApi& outApi, bool& outIsFloating) {
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

bool SdlImguiExt::Create(ClapWindowApi api, bool isFloating) {
    switch (api) {
        // only one API possible on these platforms
        case Win32:
        case Cocoa:
        case _None: {
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
        SDL_Log("Error: SDL_InitSubSystem(): %s", SDL_GetError());
        return false;
    }

    // SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
    // SDL_Window* window = SDL_CreateWindow("", 400, 400, window_flags);
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
        SDL_Log("Error: SDL_CreateProperties(): %s", SDL_GetError());
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
        SDL_Log("SDL_CreateWindowWithProperties(): %s", SDL_GetError());
        return false;
    }
    PlatformGui::onCreateWindow(mApi, mWindow);

    SDL_GLContext gl_context = SDL_GL_CreateContext(mWindow);
    if (gl_context == nullptr) {
        SDL_Log("Error: SDL_GL_CreateContext(): %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(mWindow, gl_context);
    mCtx = gl_context;

    // setup imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(mWindow, mCtx);
    ImGui_ImplOpenGL3_Init();
    return true;
}

void SdlImguiExt::Destroy() {
    if (mTimerId) {
        mHost.CancelTimer(mTimerId);
        mTimerId = 0;
    }
    if (mWindow) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(mCtx);
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool SdlImguiExt::SetScale(double scale) {
    // unused. should be communicated to ui toolkit
    return true;
}
bool SdlImguiExt::GetSize(uint32_t& outWidth, uint32_t& outHeight) {
    int32_t w;
    int32_t h;
    bool success = SDL_GetWindowSize(mWindow, &w, &h);
    outWidth = static_cast<uint32_t>(w);
    outHeight = static_cast<uint32_t>(h);
    return success;
}
bool SdlImguiExt::CanResize() {
    SDL_WindowFlags flags = SDL_GetWindowFlags(mWindow);
    return (flags & SDL_WINDOW_RESIZABLE) > 0;
}
bool SdlImguiExt::GetResizeHints(clap_gui_resize_hints_t& outResizeHints) {
    // override to force constrained resize
    outResizeHints.can_resize_horizontally = CanResize();
    outResizeHints.can_resize_vertically = CanResize();
    outResizeHints.preserve_aspect_ratio = false;
    outResizeHints.aspect_ratio_width = 1;
    outResizeHints.aspect_ratio_height = 1;
    return true;
}
bool SdlImguiExt::AdjustSize(uint32_t& inOutWidth, uint32_t& inOutHeight) {
    // apply whatever resize constraint algorithm you'd like to the inOut variables.
    // use ResizeHints to help out the OS.
    // no change means no constraint
    return true;
}
bool SdlImguiExt::SetSize(uint32_t width, uint32_t height) {
    return SDL_SetWindowSize(mWindow, width, height);
}
bool SdlImguiExt::SetParent(WindowHandle handle) {
    return PlatformGui::setParent(mApi, mWindow, handle);
}
bool SdlImguiExt::SetTransient(WindowHandle handle) {
    return PlatformGui::setTransient(mApi, mWindow, handle);
}
void SdlImguiExt::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    SDL_SetWindowTitle(mWindow, titleTemp.c_str());
}
bool SdlImguiExt::Show() {
    if (!SDL_ShowWindow(mWindow)) {
        return false;
    }
    // Setup main loop
    if (mTimerId) {
        mHost.CancelTimer(mTimerId);
        mTimerId = 0;
    }
    mTimerId = mHost.AddTimer(16, [this]() {
        // TODO: deltatime
        this->Update(1.0f / 60.0f);
    });
    return true;
}
bool SdlImguiExt::Hide() {
    if (mTimerId) {
        mHost.CancelTimer(mTimerId);
        mTimerId = 0;
    }
    if (!SDL_HideWindow(mWindow)) {
        return false;
    }
    return true;
}

void SdlImguiExt::Update(float dt) {
    ImGuiIO& io = ImGui::GetIO();
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            const clap_host_t* rawHost;
            const clap_host_gui_t* rawHostGui;
            mHost.TryGetExtension(CLAP_EXT_GUI, rawHost, rawHostGui);
            rawHostGui->closed(rawHost, true);
        } else {
            ImGui_ImplSDL3_ProcessEvent(&event);  // Forward your event to backend
        }
    }
    if (mWindow) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // app code
        mConfig.onGui();
        // end app code

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(mWindow);
    }
}