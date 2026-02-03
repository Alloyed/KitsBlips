#include "impl/pugl/contextImpl.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <pugl/gl.h>
#include <pugl/pugl.h>
#include "immediateMode/misc.h"
#include "impl/pugl/imgui_impl_pugl.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

#define LOG_PUGL_ERROR(status) kitgui::log::error(puglStrerror(status))

using namespace Magnum;

namespace kitgui::pugl {
PuglWorld* ContextImpl::sWorld{};
kitgui::WindowApi ContextImpl::sApi{};
int32_t ContextImpl::sNumInstances{};
std::vector<Context*> ContextImpl::sToClose{};

namespace {

PuglStatus onEvent(PuglView* view, const PuglEvent* event) {
    ContextImpl* self = static_cast<ContextImpl*>(puglGetHandle(view));
    return self->OnPuglEvent(event);
}
}  // namespace

void ContextImpl::init(kitgui::WindowApi api, std::string_view appName) {
    sApi = api;
    sWorld = puglNewWorld(PUGL_MODULE, PUGL_WORLD_THREADS);
    std::string tmp(appName);
    puglSetWorldString(sWorld, PUGL_CLASS_NAME, tmp.c_str());
}

void ContextImpl::deinit() {
    puglFreeWorld(sWorld);
}

ContextImpl::ContextImpl(kitgui::Context& ctx) : mContext(ctx) {}

bool ContextImpl::Create(bool isFloating) {
    mApi = sApi;
    mView = puglNewView(sWorld);
    puglSetHandle(mView, this);
    puglSetEventFunc(mView, onEvent);
    puglSetViewHint(mView, PUGL_CONTEXT_DEBUG, true);

    SizeConfig cfg = mContext.GetSizeConfig();
    puglSetSizeHint(mView, PUGL_DEFAULT_SIZE, cfg.startingWidth, cfg.startingHeight);
    puglSetViewHint(mView, PUGL_RESIZABLE, cfg.resizable);
    mContext.mSizeConfigChanged = false;

    puglSetViewHint(mView, PUGL_CONTEXT_API, PUGL_OPENGL_API);
    puglSetViewHint(mView, PUGL_CONTEXT_VERSION_MAJOR, 3);
    puglSetViewHint(mView, PUGL_CONTEXT_VERSION_MINOR, 3);
    puglSetViewHint(mView, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_CORE_PROFILE);
    puglSetViewHint(mView, PUGL_DOUBLE_BUFFER, true);
    puglSetViewHint(mView, PUGL_SWAP_INTERVAL, true);
    puglSetViewHint(mView, PUGL_DEPTH_BITS, 24);
    puglSetViewHint(mView, PUGL_STENCIL_BITS, 8);
    puglSetBackend(mView, puglGlBackend());

    sNumInstances++;
    return true;
}

bool ContextImpl::Realize() {
    // Realise is the pugl name for opengl setup
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
    ImGui_ImplPugl_InitForOpenGL(sWorld, mView);
    ImGui_ImplOpenGL3_Init();
    // compile shaders and such
    ImGui_ImplPugl_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    ImGui::EndFrame();

    mContext.OnActivate();
    return true;
}

void ContextImpl::Unrealize() {
    // opengl teardown
    MakeCurrent();
    mContext.OnDeactivate();
}

bool ContextImpl::Destroy() {
    mGl.reset();
    puglFreeView(mView);
    mView = nullptr;
    sNumInstances--;
    return true;
}

bool ContextImpl::IsCreated() const {
    return mView != nullptr;
}

void ContextImpl::SetClearColor(Magnum::Color4 color) {
    mClearColor = color;
}

bool ContextImpl::SetUIScale(double scale) {
    // Also, include puglGetScaleFactor?
    // TODO: scale font
    ImGui::GetStyle().ScaleAllSizes(static_cast<float>(scale));
    return true;
}
double ContextImpl::GetUIScale() const {
    // TODO
    return 1.0f;
}
bool ContextImpl::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
    auto area = puglGetSizeHint(mView, PUGL_CURRENT_SIZE);
    widthOut = static_cast<uint32_t>(area.width);
    heightOut = static_cast<uint32_t>(area.height);
    return true;
}
bool ContextImpl::SetSizeDirectly(uint32_t width, uint32_t height, bool resizable) {
    return puglSetViewHint(mView, PUGL_RESIZABLE, resizable) == PuglStatus::PUGL_SUCCESS &&
           puglSetSizeHint(mView, PUGL_CURRENT_SIZE, width, height) == PuglStatus::PUGL_SUCCESS;
}
bool ContextImpl::SetParent(const kitgui::WindowRef& parentWindowRef) {
    return puglSetParent(mView, reinterpret_cast<PuglNativeView>(parentWindowRef.ptr)) == PUGL_SUCCESS;
}
bool ContextImpl::SetTransient(const kitgui::WindowRef& transientWindowRef) {
    return puglSetTransientParent(mView, reinterpret_cast<PuglNativeView>(transientWindowRef.ptr)) == PUGL_SUCCESS;
}
void ContextImpl::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    puglSetViewString(mView, PUGL_WINDOW_TITLE, titleTemp.c_str());
}
bool ContextImpl::Show() {
    return puglShow(mView, PUGL_SHOW_RAISE) == PUGL_SUCCESS;
}
bool ContextImpl::Hide() {
    return puglHide(mView) == PUGL_SUCCESS;
}
bool ContextImpl::Close() {
    // to avoid potential iterator invalidation we close in a single pass after an update
    sToClose.push_back(&mContext);
    return true;
}

void ContextImpl::MakeCurrent() {
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    if (mGl.get()) {
        Magnum::Platform::GLContext::makeCurrent(mGl.get());
    }
}

void ContextImpl::ClearCurrent() {
    ImGui::SetCurrentContext(nullptr);
    Magnum::Platform::GLContext::makeCurrent(nullptr);
}

void ContextImpl::RunLoop() {
    while (sNumInstances > 0) {
        puglUpdate(sWorld, -1.0);
        for (auto ctx : sToClose) {
            ctx->Destroy();
        }
        sToClose.clear();
    }
}

void ContextImpl::RunSingleFrame() {
    puglUpdate(sWorld, 0.0);
    for (auto ctx : sToClose) {
        ctx->Destroy();
    }
    sToClose.clear();
}

void ContextImpl::OnResizeOrMove() {
    // nothing to do, yet
    // imgui viewport size is updated every frame
}

PuglStatus ContextImpl::OnPuglEvent(const PuglEvent* event) {
    MakeCurrent();
    if (mImgui && ImGui_ImplPugl_ProcessEvent(event)) {
        return PUGL_SUCCESS;
    }
    switch (event->type) {
        case PUGL_CLOSE: {
            Close();
            break;
        }
        case PUGL_UPDATE: {
            // update
            ImGui_ImplPugl_NewFrame();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();
            ImGuiHelpers::beginFullscreen([&]() { mContext.OnUpdate(); });
            mContext.OnGuiUpdate();
            puglObscureView(mView);  // request draw
            ImGui::EndFrame();
            break;
        }
        case PUGL_EXPOSE: {
            // draw
            uint32_t width = 0, height = 0;
            GetSize(width, height);
            GL::defaultFramebuffer.setViewport({{}, {static_cast<int>(width), static_cast<int>(height)}});
            GL::defaultFramebuffer.clearColor(mClearColor);
            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

            mContext.OnDraw();

            ImGui::Render();
            // TODO: the implementation of this has a big comment on it saying multi-window is untested. it _kinda_
            // works, but it'd probably work better if we forked it and used our glad handle internally
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            // Pugl will swap buffers for us
            break;
        }
        case PUGL_REALIZE: {
            Realize();
            break;
        }
        case PUGL_UNREALIZE: {
            Unrealize();
            break;
        }
        case PUGL_CONFIGURE: {
            OnResizeOrMove();
            break;
        }
        case PUGL_NOTHING:
        case PUGL_FOCUS_IN:
        case PUGL_FOCUS_OUT:
        case PUGL_KEY_PRESS:
        case PUGL_KEY_RELEASE:
        case PUGL_TEXT:
        case PUGL_POINTER_IN:
        case PUGL_POINTER_OUT:
        case PUGL_BUTTON_PRESS:
        case PUGL_BUTTON_RELEASE:
        case PUGL_MOTION:
        case PUGL_SCROLL:
        case PUGL_CLIENT:
        case PUGL_TIMER:
        case PUGL_LOOP_ENTER:
        case PUGL_LOOP_LEAVE:
        case PUGL_DATA_OFFER:
        case PUGL_DATA:
            break;
    }
    ClearCurrent();
    return PUGL_SUCCESS;
}

bool ContextImpl::IsApiSupported(kitgui::WindowApi api, bool isFloating) {
    // everything but wayland supported on backend
    return api != kitgui::WindowApi::Wayland;
}
bool ContextImpl::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
#if _WIN32
    apiOut = kitgui::WindowApi::Win32;
#elif __APPLE__
    apiOut = kitgui::WindowApi::Cocoa;
#else
    apiOut = kitgui::WindowApi::X11;
#endif
    isFloatingOut = false;
    return true;
}

}  // namespace kitgui::pugl
