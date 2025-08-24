#include "impl/sdl/contextImpl.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include "imguiHelpers/misc.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"
#include "log.h"
#include "platform/platform.h"

using namespace Magnum;

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

bool ContextImpl::Create(platform::Api api, bool isFloating) {
    mApi = api;
    switch (mApi) {
        // only one API possible on these platforms
        case platform::Api::Any:
        case platform::Api::Cocoa: {
            break;
        }
        case platform::Api::Win32: {
            // We're assuming the host platform has a message loop
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
    platform::onCreateWindow(mApi, mWindow);

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
    return SDL_SetWindowSize(mWindow, width, height);
}
bool ContextImpl::SetParent(const platform::WindowRef& window) {
    SDL_Window* handle = platform::sdl::wrapWindow(window);
    return platform::setParent(mApi, mWindow, handle);
}
bool ContextImpl::SetTransient(const platform::WindowRef& window) {
    SDL_Window* handle = platform::sdl::wrapWindow(window);
    return platform::setTransient(mApi, mWindow, handle);
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

bool ContextImpl::GetPreferredApi(platform::Api& apiOut, bool& isFloatingOut) {
#if _WIN32
    apiOut = platform::Api::Win32;
    isFloatingOut = false;
    return true;
#elif __APPLE__
    apiOut = platform::Api::Cocoa;
    isFloatingOut = false;
    return true;
#elif __linux__
    apiOut = platform::Api::X11;
    isFloatingOut = false;
    return true;
#endif
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