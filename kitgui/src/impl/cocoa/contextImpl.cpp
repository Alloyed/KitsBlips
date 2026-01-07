#include "impl/cocoa/contextImpl.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include "immediateMode/misc.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

using namespace Magnum;

namespace kitgui::cocoa {
void ContextImpl::init(kitgui::WindowApi api, bool isFloating) {
    sIsFloating = isFloating;
}

void ContextImpl::deinit() {}

ContextImpl::ContextImpl(kitgui::Context& ctx) : mContext(ctx) {}

bool ContextImpl::Create() {
    kitgui::WindowApi api = kitgui::WindowApi::Cocoa;
    bool isFloating = sIsFloating;
    mApi = api;
    SizeConfig cfg = mContext.GetSizeConfig();

    // createWindow
    // onCreateWindow(mApi, mWindow);

    // SDL_GLContext gl_context = SDL_GL_CreateContext(mWindow);
    // if (gl_context == nullptr) {
    //     LOG_SDL_ERROR();
    //     return false;
    // }
    // mSdlGl = gl_context;
    // SDL_GL_MakeCurrent(mWindow, mSdlGl);

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
    // ImGui_ImplSDL3_InitForOpenGL(mWindow, mSdlGl);
    ImGui_ImplOpenGL3_Init();

    return true;
}

bool ContextImpl::Destroy() {
    if (IsCreated()) {
        MakeCurrent();
        RemoveActiveInstance(this);
        mGl.reset();
        // SDL_GL_DestroyContext(mSdlGl);
        // SDL_DestroyWindow(mWindow);
        // mWindow = nullptr;
        // mSdlGl = nullptr;

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
    return false;
    // int32_t w{}, h{};
    // bool success = SDL_GetWindowSize(mWindow, &w, &h);
    // widthOut = static_cast<uint32_t>(w);
    // heightOut = static_cast<uint32_t>(h);
    // return success;
}
bool ContextImpl::SetSizeDirectly(uint32_t width, uint32_t height) {
    return false;
    // return SDL_SetWindowSize(mWindow, static_cast<int32_t>(width), static_cast<int32_t>(height));
}
bool ContextImpl::SetParent(const kitgui::WindowRef& parentWindowRef) {
    return false;
    // return setParent(mApi, mWindow, parentWindowRef);
}
bool ContextImpl::SetTransient(const kitgui::WindowRef& transientWindowRef) {
    return false;
    // return setTransient(mApi, mWindow, transientWindowRef);
}
void ContextImpl::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    // SDL_SetWindowTitle(mWindow, titleTemp.c_str());
}
bool ContextImpl::Show() {
    // if (!SDL_ShowWindow(mWindow)) {
    //     LOG_SDL_ERROR();
    //     return false;
    // }
    AddActiveInstance(this);
    return true;
}
bool ContextImpl::Hide() {
    // if (!SDL_HideWindow(mWindow)) {
    //     LOG_SDL_ERROR();
    //     return false;
    // }
    mActive = false;  // will be removed
    return true;
}
bool ContextImpl::Close() {
    mActive = false;  // will be removed
    mDestroy = true;  // will be destroyed
    return true;
}

void ContextImpl::MakeCurrent() {
    // if (mSdlGl) {
    //     SDL_GL_MakeCurrent(mWindow, mSdlGl);
    // }
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    Magnum::Platform::GLContext::makeCurrent(mGl.get());
}

std::vector<ContextImpl*> ContextImpl::sActiveInstances = {};
bool ContextImpl::sIsFloating = false;

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
        // SDL_Delay(16);
    }
}

void ContextImpl::RunSingleFrame() {
    for (int32_t idx = static_cast<int32_t>(sActiveInstances.size()) - 1; idx >= 0; idx--) {
        auto& instance = sActiveInstances[idx];
        if (instance->mDestroy) {
            instance->mContext.Destroy();
        }
        if (instance->mActive == false) {
            RemoveActiveInstance(instance);
        }
    }

    // poll events

    for (auto instance : sActiveInstances) {
        instance->MakeCurrent();

        // update
        ImGui_ImplOpenGL3_NewFrame();
        // ImGui_ImplSDL3_NewFrame();
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

        // SDL_GL_SwapWindow(instance->mWindow);
    }
}

bool ContextImpl::IsApiSupported(kitgui::WindowApi api, bool isFloating) {
    return api == kitgui::WindowApi::Cocoa && isFloating == false;
}
bool ContextImpl::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    apiOut = kitgui::WindowApi::Cocoa;
    isFloatingOut = false;
    return true;
}

// ContextImpl* ContextImpl::FindContextImplForWindow(SDL_Window* win) {
//     for (ContextImpl* instance : sActiveInstances) {
//         if (instance->mWindow == win) {
//             return instance;
//         }
//     }
//     return nullptr;
// }
}  // namespace kitgui::cocoa