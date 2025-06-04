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
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#if __linux__
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#endif

bool SdlImguiExt::IsApiSupported(ClapWindowApi api, bool isFloating) {
    // All APIs supported, but non-floating windows NYI
    return api != ClapWindowApi::_None && isFloating == true;
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

    outIsFloating = true;
    return true;
}

bool SdlImguiExt::Create(ClapWindowApi api, bool isFloating) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        printf("Error: SDL_InitSubSystem(): %s\n", SDL_GetError());
        return false;
    }

    CreatePluginWindow();

    return true;
}

void SdlImguiExt::Destroy() {
    DestroyPluginWindow();
    DestroyParentWindow();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool SdlImguiExt::SetScale(double scale) {
    // unused. should be communicated to ui toolkit
    return true;
}
bool SdlImguiExt::GetSize(uint32_t& outWidth, uint32_t& outHeight) {
    int32_t w;
    int32_t h;
    bool success = SDL_GetWindowSize(mWindowHandle, &w, &h);
    outWidth = static_cast<uint32_t>(w);
    outHeight = static_cast<uint32_t>(h);
    return success;
}
bool SdlImguiExt::CanResize() {
    return false;
}
bool SdlImguiExt::GetResizeHints(clap_gui_resize_hints_t& outResizeHints) {
    outResizeHints.can_resize_horizontally = false;
    outResizeHints.can_resize_vertically = false;
    outResizeHints.preserve_aspect_ratio = false;
    outResizeHints.aspect_ratio_width = 1;
    outResizeHints.aspect_ratio_height = 1;
    return true;
}
bool SdlImguiExt::AdjustSize(uint32_t& inOutWidth, uint32_t& inOutHeight) {
    // good enough :p
    return true;
}
bool SdlImguiExt::SetSize(uint32_t width, uint32_t height) {
    return SDL_SetWindowSize(mWindowHandle, width, height);
}
bool SdlImguiExt::SetParent(WindowHandle handle) {
    CreateParentWindow(handle);
#if __linux__
    SDL_PropertiesID parentProps = SDL_GetWindowProperties(mParentHandle);
    if (!parentProps) {
        printf("Error: parentProps(): %s\n", SDL_GetError());
        return false;
    }
    SDL_PropertiesID childProps = SDL_GetWindowProperties(mWindowHandle);
    if (!childProps) {
        printf("Error: childProps(): %s\n", SDL_GetError());
        return false;
    }
    Display* xDisplay = (Display*)SDL_GetPointerProperty(parentProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    Window xParentWindow = (Window)SDL_GetNumberProperty(parentProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    Window xChildWindow = (Window)SDL_GetNumberProperty(childProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

    XReparentWindow(xDisplay, xChildWindow, xParentWindow, 0, 0);
    return true;
#endif
    return false;
}
bool SdlImguiExt::SetTransient(WindowHandle handle) {
    CreateParentWindow(handle);
    // weirdly, SDL_SetWindowParent actually calls XSetTransientForHint on X11
    return SDL_SetWindowParent(mWindowHandle, mParentHandle);
}
void SdlImguiExt::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    SDL_SetWindowTitle(mWindowHandle, titleTemp.c_str());
}
bool SdlImguiExt::Show() {
    if (!SDL_ShowWindow(mWindowHandle)) {
        return false;
    }
    // Setup main loop
    if (mTimerId) {
        mHost.CancelTimer(mTimerId);
    }
    mTimerId = mHost.AddTimer(16, [this]() {
        // TODO: deltatime
        this->Update(1.0f / 60.0f);
    });
    return true;
}
bool SdlImguiExt::Hide() {
    if (!SDL_HideWindow(mWindowHandle)) {
        return false;
    }
    if (mTimerId) {
        mHost.CancelTimer(mTimerId);
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
    if (mWindowHandle) {
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
        SDL_GL_SwapWindow(mWindowHandle);
    }
}

bool SdlImguiExt::CreatePluginWindow() {
    DestroyPluginWindow();
    // GL 3.0 + GLSL 130
    // const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
    SDL_Window* window = SDL_CreateWindow("", 400, 400, window_flags);
    if (window == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return false;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    mWindowHandle = window;
    mCtx = gl_context;

    // setup imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(mWindowHandle, mCtx);
    ImGui_ImplOpenGL3_Init();

    return true;
}

void SdlImguiExt::DestroyPluginWindow() {
    if (mTimerId) {
        mHost.CancelTimer(mTimerId);
    }
    if (mWindowHandle) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(mCtx);
        SDL_DestroyWindow(mWindowHandle);
        mWindowHandle = nullptr;
    }
}

bool SdlImguiExt::CreateParentWindow(WindowHandle clapHandle) {
    DestroyParentWindow();
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        printf("Error: SDL_CreateProperties(): %s\n", SDL_GetError());
        return false;
    }

    switch (clapHandle.api) {
        case ClapWindowApi::_None: {
            return false;
        }
        case ClapWindowApi::X11: {
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER,
                                  reinterpret_cast<std::uintptr_t>(clapHandle.ptr));
            break;
        }
        case ClapWindowApi::Wayland: {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, clapHandle.ptr);
            break;
        }
        case ClapWindowApi::Win32: {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, clapHandle.ptr);
            break;
        }
        case ClapWindowApi::Cocoa: {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, clapHandle.ptr);
            break;
        }
    }

    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    ///... is this safe? or do i need to keep it alive?
    SDL_DestroyProperties(props);
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindowWithProperties(): %s", SDL_GetError());
        return false;
    }

    mParentHandle = window;
    return true;
}

void SdlImguiExt::DestroyParentWindow() {
    if (mParentHandle != nullptr) {
        SDL_DestroyWindow(mParentHandle);
        mParentHandle = nullptr;
    }
}