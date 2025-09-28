#include "impl/sdl/contextImpl.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include "immediateMode/misc.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"
#include "log.h"

// linux-specific platform details
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
namespace {
using X11Window = Window;
using X11Display = Display;
void getX11Handles(SDL_Window* sdlWindow, X11Window& xWindow, X11Display*& xDisplay) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    xWindow = SDL_GetNumberProperty(windowProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    xDisplay =
        static_cast<X11Display*>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
}
X11Window getX11Window(const kitgui::WindowRef& ref) {
    if (ref.api == kitgui::WindowApi::X11) {
        return reinterpret_cast<unsigned long>(ref.ptr);
    }
    return 0;
}
void onCreateWindow(kitgui::WindowApi api, SDL_Window* sdlWindow) {
    if (api == kitgui::WindowApi::Wayland) {
        return;
    } else if (api == kitgui::WindowApi::X11) {
        Window xWindow{};
        Display* xDisplay{};
        getX11Handles(sdlWindow, xWindow, xDisplay);

        // mark window as embedded
        // https://specifications.freedesktop.org/xembed-spec/0.5/lifecycle.html
        Atom embedInfoAtom = XInternAtom(xDisplay, "_XEMBED_INFO", 0);
        uint32_t embedInfoData[2] = {0 /* version */, 1 /* mapped = true */};
        int32_t bitFormat = 32;
        int32_t numElements = 2;
        XChangeProperty(xDisplay, xWindow, embedInfoAtom, embedInfoAtom, bitFormat, PropModeReplace,
                        (uint8_t*)embedInfoData, numElements);
    }
}
bool setParent(kitgui::WindowApi api, SDL_Window* sdlWindow, const kitgui::WindowRef& parentRef) {
    if (api == kitgui::WindowApi::Wayland) {
        return true;
    } else if (api == kitgui::WindowApi::X11) {
        X11Window xWindow{};
        X11Display* xDisplay{};
        getX11Handles(sdlWindow, xWindow, xDisplay);
        X11Window parentWindow = getX11Window(parentRef);

        XReparentWindow(xDisplay, xWindow, parentWindow, 0, 0);
        XFlush(xDisplay);
    }

    return true;
}

bool setTransient(kitgui::WindowApi api, SDL_Window* sdlWindow, const kitgui::WindowRef& parentRef) {
    if (api == kitgui::WindowApi::Wayland) {
        return true;
    } else if (api == kitgui::WindowApi::X11) {
        X11Window xWindow{};
        X11Display* xDisplay{};
        getX11Handles(sdlWindow, xWindow, xDisplay);
        X11Window parentWindow = getX11Window(parentRef);

        XSetTransientForHint(xDisplay, xWindow, parentWindow);
        return true;
    }

    return true;
}
bool isApiSupported(kitgui::WindowApi api, bool isFloating) {
    if (api == kitgui::WindowApi::Any) {
        api = kitgui::WindowApi::X11;
    }

    if (api == kitgui::WindowApi::X11) {
        return true;
    } else if (api == kitgui::WindowApi::Wayland) {
        return isFloating;
    }
    return false;
}
bool getPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    apiOut = kitgui::WindowApi::X11;
    isFloatingOut = false;
    return true;
}
}  // namespace
#endif

using namespace Magnum;

#define LOG_SDL_ERROR() kitgui::log::error(SDL_GetError())

namespace kitgui::sdl {
void ContextImpl::init() {
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

void ContextImpl::deinit() {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

ContextImpl::ContextImpl(kitgui::Context& ctx) : mContext(ctx) {}

bool ContextImpl::Create(kitgui::WindowApi api, bool isFloating) {
    mApi = api;
    switch (mApi) {
        // only one API possible on these platforms
        case kitgui::WindowApi::Any:
        case kitgui::WindowApi::Win32:
        case kitgui::WindowApi::Cocoa: {
            break;
        }
        case kitgui::WindowApi::X11: {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
            break;
        }
        case kitgui::WindowApi::Wayland: {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
            break;
        }
    }
    // uncomment to see verbose event logs
    // SDL_SetHint(SDL_HINT_EVENT_LOGGING, "1");

    SDL_PropertiesID createProps = SDL_CreateProperties();
    if (createProps == 0) {
        LOG_SDL_ERROR();
        return false;
    }

    SizeConfig cfg = mContext.GetSizeConfig();
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, cfg.resizable);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, cfg.startingWidth);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, cfg.startingHeight);
    mWindow = SDL_CreateWindowWithProperties(createProps);
    SDL_DestroyProperties(createProps);

    if (mWindow == nullptr) {
        LOG_SDL_ERROR();
        return false;
    }

    onCreateWindow(mApi, mWindow);

    SDL_GLContext gl_context = SDL_GL_CreateContext(mWindow);
    if (gl_context == nullptr) {
        LOG_SDL_ERROR();
        return false;
    }
    mSdlGl = gl_context;
    SDL_GL_MakeCurrent(mWindow, mSdlGl);
    Magnum::Platform::GLContext::makeCurrent(nullptr);
    mGl = std::make_unique<Magnum::Platform::GLContext>();
    Magnum::Platform::GLContext::makeCurrent(mGl.get());

    // setup imgui
    IMGUI_CHECKVERSION();
    mImgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(mImgui);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // disable file writing
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(mWindow, mSdlGl);
    ImGui_ImplOpenGL3_Init();

    return true;
}

bool ContextImpl::Destroy() {
    if (IsCreated()) {
        MakeCurrent();
        RemoveActiveInstance(this);
        mGl.reset();
        SDL_GL_DestroyContext(mSdlGl);
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
        mSdlGl = nullptr;

        // pick any valid current engine for later: this works around a bug i don't fully understand in SDL3 itself :x
        // without this block, closing a window when multiple are active causes a crash in the main update loop when we
        // call SDL_GL_MakeCurrent(). maybe a required resource is already destroyed by that point?
        for (auto instance : sActiveInstances) {
            if (instance->IsCreated()) {
                instance->MakeCurrent();
                break;
            }
        }
    }
    return true;
}

bool ContextImpl::IsCreated() const {
    return mImgui != nullptr;
}

void ContextImpl::SetClearColor(Magnum::Color4 color) {
    mClearColor = color;
}

bool ContextImpl::SetScale(double scale) {
    // TODO: scale font
    ImGui::GetStyle().ScaleAllSizes(static_cast<float>(scale));
    return true;
}
bool ContextImpl::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
    int32_t w{}, h{};
    bool success = SDL_GetWindowSize(mWindow, &w, &h);
    widthOut = static_cast<uint32_t>(w);
    heightOut = static_cast<uint32_t>(h);
    return success;
}
bool ContextImpl::SetSizeDirectly(uint32_t width, uint32_t height) {
    return SDL_SetWindowSize(mWindow, static_cast<int32_t>(width), static_cast<int32_t>(height));
}
bool ContextImpl::SetParent(const kitgui::WindowRef& parentWindowRef) {
    return setParent(mApi, mWindow, parentWindowRef);
}
bool ContextImpl::SetTransient(const kitgui::WindowRef& transientWindowRef) {
    return setTransient(mApi, mWindow, transientWindowRef);
}
void ContextImpl::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    SDL_SetWindowTitle(mWindow, titleTemp.c_str());
}
bool ContextImpl::Show() {
    if (!SDL_ShowWindow(mWindow)) {
        LOG_SDL_ERROR();
        return false;
    }
    AddActiveInstance(this);
    return true;
}
bool ContextImpl::Hide() {
    if (!SDL_HideWindow(mWindow)) {
        LOG_SDL_ERROR();
        return false;
    }
    mActive = false;  // will be removed
    return true;
}
bool ContextImpl::Close() {
    mActive = false;  // will be removed
    mDestroy = true;  // will be destroyed
    return true;
}

void ContextImpl::MakeCurrent() {
    if (mSdlGl) {
        SDL_GL_MakeCurrent(mWindow, mSdlGl);
    }
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    Magnum::Platform::GLContext::makeCurrent(mGl.get());
}

std::vector<ContextImpl*> ContextImpl::sActiveInstances = {};

void ContextImpl::AddActiveInstance(ContextImpl* instance) {
    if (std::find(sActiveInstances.begin(), sActiveInstances.end(), instance) == sActiveInstances.end()) {
        sActiveInstances.push_back(instance);
        instance->mActive = true;
    }
    instance->mContext.OnActivate();
}

void ContextImpl::RemoveActiveInstance(ContextImpl* instance) {
    instance->mActive = false;
    const auto iter = std::find(sActiveInstances.begin(), sActiveInstances.end(), instance);
    if (iter != sActiveInstances.end()) {
        instance->mContext.OnDeactivate();
        sActiveInstances.erase(iter);
    }
}

void ContextImpl::RunLoop() {
    while (!sActiveInstances.empty()) {
        ContextImpl::RunSingleFrame();
        SDL_Delay(16);
    }
}

void ContextImpl::RunSingleFrame() {
    for (int32_t idx = static_cast<int32_t>(sActiveInstances.size()) - 1; idx >= 0; idx--) {
        auto& instance = sActiveInstances[idx];
        if (instance->mDestroy) {
            instance->Destroy();
        }
        if (instance->mActive == false) {
            RemoveActiveInstance(instance);
        }
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        SDL_Window* window = SDL_GetWindowFromEvent(&event);
        ContextImpl* instance = window ? FindContextImplForWindow(window) : nullptr;
        switch (event.type) {
            case SDL_EVENT_WINDOW_DESTROYED: {
                goto skip_event;
            }
            case SDL_EVENT_QUIT: {
                for (auto instanceIter : sActiveInstances) {
                    instanceIter->Close();
                }
                break;
            }
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                if (instance) {
                    instance->Close();
                }
                break;
            }
        }

        if (instance) {
            instance->MakeCurrent();
            ImGui_ImplSDL3_ProcessEvent(&event);
        }

    skip_event:
        continue;
    }

    for (auto instance : sActiveInstances) {
        instance->MakeCurrent();

        // update
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGuiHelpers::beginFullscreen([&]() { instance->mContext.OnUpdate(); });

        // draw
        uint32_t width = 0, height = 0;
        instance->GetSize(width, height);
        GL::defaultFramebuffer.setViewport({{}, {static_cast<int>(width), static_cast<int>(height)}});
        GL::defaultFramebuffer.clearColor(instance->mClearColor);
        GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

        instance->mContext.OnDraw();

        ImGui::Render();
        // TODO: the implementation of this has a big comment on it saying multi-window is untested. it _kinda_ works,
        // but it'd probably work better if we forked it and used our glad handle internally
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(instance->mWindow);
    }
}

bool ContextImpl::IsApiSupported(kitgui::WindowApi api, bool isFloating) {
    return isApiSupported(api, isFloating);
}
bool ContextImpl::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    return getPreferredApi(apiOut, isFloatingOut);
}

ContextImpl* ContextImpl::FindContextImplForWindow(SDL_Window* win) {
    for (ContextImpl* instance : sActiveInstances) {
        if (instance->mWindow == win) {
            return instance;
        }
    }
    return nullptr;
}
}  // namespace kitgui::sdl