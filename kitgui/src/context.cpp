#include "kitgui/context.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <algorithm>
#include <string>
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "log.h"
#include "platform/platform.h"

namespace kitgui {

bool Context::Create(platform::Api api, bool isFloating) {
    mApi = api;
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
    // SDL_SetHint(SDL_HINT_EVENT_LOGGING, "1");

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
    gladLoadGLContext(&mGl, (GLADloadfunc)SDL_GL_GetProcAddress);

    // setup imgui
    IMGUI_CHECKVERSION();
    mImgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(mImgui);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(mWindow, mSdlGl);
    ImGui_ImplOpenGL3_Init();

    return true;
}

bool Context::Destroy() {
    if (IsCreated()) {
        MakeCurrent();
        RemoveActiveInstance(this);
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

bool Context::IsCreated() const {
    return mImgui != nullptr;
}

bool Context::SetScale(double scale) {
    // TODO: scale font
    ImGui::GetStyle().ScaleAllSizes(static_cast<float>(scale));
    return true;
}
bool Context::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
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
bool Context::AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) const {
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
    AddActiveInstance(this);
    return true;
}
bool Context::Hide() {
    if (!SDL_HideWindow(mWindow)) {
        LOG_SDL_ERROR();
        return false;
    }
    mActive = false;  // will be removed
    return true;
}
bool Context::Close() {
    mActive = false;  // will be removed
    mDestroy = true;  // will be destroyed
    return true;
}

const GladGLContext& Context::MakeCurrent() const {
    if (mSdlGl) {
        SDL_GL_MakeCurrent(mWindow, mSdlGl);
    }
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    return mGl;
}

std::vector<Context*> Context::sActiveInstances = {};

void Context::AddActiveInstance(Context* instance) {
    if (std::find(sActiveInstances.begin(), sActiveInstances.end(), instance) == sActiveInstances.end()) {
        sActiveInstances.push_back(instance);
        instance->mActive = true;
    }
    if (instance->mApp) {
        instance->mApp->OnActivate();
    }
}

void Context::RemoveActiveInstance(Context* instance) {
    instance->mActive = false;
    const auto iter = std::find(sActiveInstances.begin(), sActiveInstances.end(), instance);
    if (iter != sActiveInstances.end()) {
        if (instance->mApp) {
            instance->mApp->OnDeactivate();
        }
        sActiveInstances.erase(iter);
    }
}

void Context::RunLoop() {
    while (!sActiveInstances.empty()) {
        Context::RunSingleFrame();
        SDL_Delay(16);
    }
}

void Context::RunSingleFrame() {
    for (int32_t idx = sActiveInstances.size() - 1; idx >= 0; idx--) {
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
        Context* instance = window ? FindContextForWindow(SDL_GetWindowID(window)) : nullptr;
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
        auto& gl = instance->MakeCurrent();

        // update
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (instance->mApp) {
            instance->mApp->OnUpdate();
        }

        // draw
        uint32_t width, height;
        instance->GetSize(width, height);
        gl.Viewport(0, 0, width, height);
        gl.ClearColor(instance->mClearColor[0], instance->mClearColor[1], instance->mClearColor[2],
                      instance->mClearColor[3]);
        gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (instance->mApp) {
            instance->mApp->OnDraw(gl);
        }

        ImGui::Render();
        // TODO: the implementation of this has a big comment on it saying multi-window is untested. it _kinda_ works,
        // but it'd probably work better if we forked it and used our glad handle internally
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(instance->mWindow);
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