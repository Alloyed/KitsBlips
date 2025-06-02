#include "sdlImgui.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include "clapApi/ext/gui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"


int32_t SdlImguiExt::sInstances = 0;

bool SdlImguiExt::IsApiSupported(WindowingApi api, bool isFloating) {
    // All APIs supported, but floating windows NYI
    return api != WindowingApi::None && isFloating != true;
}

bool SdlImguiExt::GetPreferredApi(WindowingApi& outApi, bool& outIsFloating) {
    std::string_view platformName = SDL_GetPlatform();
    if (platformName == "Windows") {
        outApi = WindowingApi::Win32;
    } else if (platformName == "macOS") {
        outApi = WindowingApi::Cocoa;
    } else if (platformName == "Linux") {
        outApi = WindowingApi::X11;
    }

    outIsFloating = false;
    return true;
}

bool SdlImguiExt::OnAppInit() {
	printf("onAppInit\n");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return false;
    }

    // GL 3.0 + GLSL 130
    //const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    return true;
}

bool SdlImguiExt::OnAppQuit() {
	printf("onAppQuit\n");
    SDL_Quit();
    return true;
}

bool SdlImguiExt::Create(WindowingApi api, bool isFloating) {
	printf("Create\n");
	sInstances++;
	if(sInstances == 1)
	{
		OnAppInit();
	}
    CreatePluginWindow();
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(mWindowHandle, mCtx);
    ImGui_ImplOpenGL3_Init();

    // Setup main loop
    mHost.AddTimer(16, [this]() {
        // TODO: deltatime
        this->Update(1.0f / 60.0f);
    });
	return true;
}

void SdlImguiExt::Destroy() {
	printf("Destroy\n");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(mCtx);

	if(mWindowHandle != nullptr)
	{
		SDL_DestroyWindow(mWindowHandle);
	}
	if(mParentHandle != nullptr)
	{
		SDL_DestroyWindow(mParentHandle);
	}
	sInstances--;
	if(sInstances == 0)
	{
		OnAppQuit();
	}
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
    return SDL_SetWindowParent(mWindowHandle, mParentHandle);
}
bool SdlImguiExt::SetTransient(WindowHandle handle) {
    // Do nothing
    return true;
}
void SdlImguiExt::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    SDL_SetWindowTitle(mWindowHandle, titleTemp.c_str());
}
bool SdlImguiExt::Show() {
    return SDL_ShowWindow(mWindowHandle);
}
bool SdlImguiExt::Hide() {
    return SDL_HideWindow(mWindowHandle);
}

void SdlImguiExt::Update(float dt) {
    ImGuiIO& io = ImGui::GetIO();
    static SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);  // Forward your event to backend
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

void SdlImguiExt::CreatePluginWindow() {
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("", 400, 400, window_flags);
    if (window == nullptr) {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    mWindowHandle = window;
    mCtx = gl_context;
}

void SdlImguiExt::CreateParentWindow(WindowHandle clapHandle) {
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        return;
    }

    switch (clapHandle.api) {
        case WindowingApi::None: {
            return;
        };
        case WindowingApi::X11: {
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER,
                                  reinterpret_cast<std::uintptr_t>(clapHandle.ptr));
            break;
        }
        case WindowingApi::Wayland: {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, clapHandle.ptr);
            break;
        }
        case WindowingApi::Win32: {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, clapHandle.ptr);
            break;
        }
        case WindowingApi::Cocoa: {
            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, clapHandle.ptr);
            break;
        }
    }

    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    if (window == nullptr) {
        SDL_Log("Unable to create parent window: %s", SDL_GetError());
        return;
    }
    mParentHandle = window;

    ///... is this safe? or do i need to keep it alive?
    SDL_DestroyProperties(props);
}