#include "impl/pugl/contextImpl.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <pugl/gl.h>
#include <pugl/pugl.h>
#include "immediateMode/misc.h"
#include "impl/pugl/imgui_impl_pugl.h"
#include "kitgui/kitgui.h"

#define LOG_PUGL_ERROR(status) kitgui::log::error(puglStrerror(status))

using namespace Magnum;

namespace kitgui::pugl {
PuglWorld* ContextImpl::sWorld{};
kitgui::WindowApi ContextImpl::sApi{};
int32_t ContextImpl::sNumInstances{};

namespace {

PuglStatus onEvent(PuglView* view, const PuglEvent* event) {
    ContextImpl* self = static_cast<ContextImpl*>(puglGetHandle(view));
    return self->OnPuglEvent(event);
}
}  // namespace

void ContextImpl::init(kitgui::WindowApi api) {
    sApi = api;
    sWorld = puglNewWorld(PUGL_MODULE, PUGL_WORLD_THREADS);
    puglSetWorldString(sWorld, PUGL_CLASS_NAME, "Swag");  // TODO
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

    puglSetViewHint(mView, PUGL_CONTEXT_VERSION_MAJOR, 3);
    puglSetViewHint(mView, PUGL_CONTEXT_VERSION_MINOR, 3);
    puglSetViewHint(mView, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_COMPATIBILITY_PROFILE);
    puglSetBackend(mView, puglGlBackend());
    return true;
}

bool ContextImpl::Realize() {
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
    return true;
}

void ContextImpl::Unrealize() {
    mGl.reset();
}

bool ContextImpl::Destroy() {
    puglFreeView(mView);
    sNumInstances--;
    return true;
}

bool ContextImpl::IsCreated() const {
    return mImgui != nullptr;
}

void ContextImpl::SetClearColor(Magnum::Color4 color) {
    mClearColor = color;
}

bool ContextImpl::SetScale(double scale) {
    // Also, include puglGetScaleFactor?
    // TODO: scale font
    ImGui::GetStyle().ScaleAllSizes(static_cast<float>(scale));
    return true;
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
    return Destroy();
}

void ContextImpl::MakeCurrent() {
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    Magnum::Platform::GLContext::makeCurrent(mGl.get());
}

void ContextImpl::RunLoop() {
    while (sNumInstances > 0) {
        puglUpdate(sWorld, -1.0);
    }
}

void ContextImpl::RunSingleFrame() {
    puglUpdate(sWorld, 0.0);
}

void ContextImpl::OnResizeOrMove() {
    // nothing to do, yet
    // imgui viewport size is updated every frame
}

PuglStatus ContextImpl::OnPuglEvent(const PuglEvent* event) {
    if (ImGui_ImplPugl_ProcessEvent(event)) {
        return PUGL_SUCCESS;
    }
    switch (event->type) {
        case PUGL_CLOSE: {
            Close();
            break;
        }
        case PUGL_UPDATE: {
            // update
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplPugl_NewFrame();
            ImGui::NewFrame();
            ImGuiHelpers::beginFullscreen([&]() { mContext.OnUpdate(); });
            puglObscureView(mView);  // request draw
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
    return PUGL_SUCCESS;
}

bool ContextImpl::IsApiSupported(kitgui::WindowApi api, bool isFloating) {
    return api == kitgui::WindowApi::Any || api == kitgui::WindowApi::X11;
}
bool ContextImpl::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    apiOut = kitgui::WindowApi::X11;
    isFloatingOut = false;
    return true;
}

}  // namespace kitgui::pugl