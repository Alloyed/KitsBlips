#include "impl/sdl/contextImpl.h"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <algorithm>
#include <string_view>
#include "Magnum/GL/Renderer.h"
#include "immediateMode/misc.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"
#include "log.h"

#define LOG_SDL_ERROR() kitgui::log::error(SDL_GetError())

// linux-specific platform details
#ifdef __linux__
#include <X11/Xlib.h>
namespace {
using X11Window = Window;
using X11Display = Display;
void getX11Handles(SDL_Window* sdlWindow, X11Window& xWindow, X11Display*& xDisplay) {
    SDL_PropertiesID windowProps = SDL_GetWindowProperties(sdlWindow);
    if (windowProps == 0) {
        LOG_SDL_ERROR();
        return;
    }
    xWindow = SDL_GetNumberProperty(windowProps, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    xDisplay =
        static_cast<X11Display*>(SDL_GetPointerProperty(windowProps, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));

    assert(xDisplay != nullptr);
    assert(xWindow != 0);
}
X11Window getX11Window(const kitgui::WindowRef& ref) {
    if (ref.api == kitgui::WindowApi::X11) {
        return reinterpret_cast<unsigned long>(ref.ptr);
    }
    return 0;
}
void onCreateWindow_(kitgui::WindowApi api, SDL_Window* sdlWindow, bool isFloating) {
    if (api == kitgui::WindowApi::Wayland) {
        return;
    } else if (api == kitgui::WindowApi::X11 && !isFloating) {
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
bool setParent_(kitgui::WindowApi api, SDL_Window* sdlWindow, const kitgui::WindowRef& parentRef) {
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

bool setTransient_(kitgui::WindowApi api, SDL_Window* sdlWindow, const kitgui::WindowRef& parentRef) {
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

void checkSupportedApis_(bool& outHasX11, bool& outHasWayland) {
    static bool hasX11{};
    static bool hasWayland{};
    static bool hasChecked{};
    if (!hasChecked) {
        int numVideoDrivers = SDL_GetNumVideoDrivers();
        for (int i = 0; i < numVideoDrivers; i++) {
            std::string_view driverName = SDL_GetVideoDriver(i);
            if (driverName == "x11") {
                hasX11 = true;
            } else if (driverName == "wayland") {
                hasWayland = true;
            }
        }
        kitgui::log::info(fmt::format("Checking supported video drivers. x11: {}, wayland: {}", hasX11, hasWayland));
        hasChecked = true;
    }
    outHasX11 = hasX11;
    outHasWayland = hasWayland;
}

bool isApiSupported_(kitgui::WindowApi api, bool isFloating) {
    bool hasX11{};
    bool hasWayland{};
    checkSupportedApis_(hasX11, hasWayland);

    if (hasX11 && (api == kitgui::WindowApi::Any || api == kitgui::WindowApi::X11)) {
        return true;
    } else if (hasWayland && (api == kitgui::WindowApi::Any || api == kitgui::WindowApi::Wayland)) {
        // wayland only supports floating windows
        return isFloating;
    } else {
        return false;
    }
}

bool getPreferredApi_(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    bool hasX11{};
    bool hasWayland{};
    checkSupportedApis_(hasX11, hasWayland);

    if (hasWayland) {
        apiOut = kitgui::WindowApi::Wayland;
        isFloatingOut = true;
        return true;
    } else if (hasX11) {
        apiOut = kitgui::WindowApi::X11;
        isFloatingOut = false;
        return true;
    } else {
        // no video support at all
        return false;
    }
}
}  // namespace
#endif

using namespace Magnum;

namespace kitgui::sdl {
void ContextImpl::init(kitgui::WindowApi api, std::string_view appName) {
    (void)appName;
    sApi = api;
    switch (api) {
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
    if (sSdlGl) {
        sGl.reset();
        if (!SDL_GL_DestroyContext(sSdlGl)) {
            LOG_SDL_ERROR();
        }
        sSdlGl = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

ContextImpl::ContextImpl(kitgui::Context& ctx) : mContext(ctx) {}

bool ContextImpl::Create(bool isFloating) {
    mWindowProps = SDL_CreateProperties();
    if (mWindowProps == 0) {
        LOG_SDL_ERROR();
        return false;
    }

    SizeConfig cfg = mContext.GetSizeConfig();
    SDL_SetBooleanProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetBooleanProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    SDL_SetBooleanProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, cfg.resizable);
    SDL_SetBooleanProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, cfg.resizable);

    // https://wiki.libsdl.org/SDL3/README-highdpi
    SDL_SetBooleanProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetNumberProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<int64_t>(cfg.startingWidth));
    SDL_SetNumberProperty(mWindowProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<int64_t>(cfg.startingHeight));

    mContext.mSizeConfigChanged = false;
    mWindow = SDL_CreateWindowWithProperties(mWindowProps);

    if (mWindow == nullptr) {
        LOG_SDL_ERROR();
        return false;
    }

    if (sApi == kitgui::WindowApi::Any) {
        // figure out which api we actually are using
        std::string_view driver = SDL_GetCurrentVideoDriver();
        if (driver == "x11") {
            sApi = kitgui::WindowApi::X11;
        } else if (driver == "wayland") {
            sApi = kitgui::WindowApi::Wayland;
        } else {
            assert(false);
        }
    }

    // apply highdpi scale
    float scale = SDL_GetWindowDisplayScale(mWindow);
    if (scale == 0.0f) {
        LOG_SDL_ERROR();
        return false;
    } else if (scale > 1.0f) {
        SetUIScale(scale);
        SetSizeDirectly(cfg.startingWidth, cfg.startingHeight, cfg.resizable);
    }

    onCreateWindow_(sApi, mWindow, isFloating);

    if (sSdlGl == nullptr) {
        // create context on first use
        SDL_GLContext gl_context = SDL_GL_CreateContext(mWindow);
        if (gl_context == nullptr) {
            LOG_SDL_ERROR();
            return false;
        }
        sSdlGl = gl_context;
        if (!SDL_GL_MakeCurrent(mWindow, sSdlGl)) {
            LOG_SDL_ERROR();
            return false;
        }
        Magnum::Platform::GLContext::makeCurrent(nullptr);
        sGl = std::make_unique<Magnum::Platform::GLContext>();
    }
    Magnum::Platform::GLContext::makeCurrent(sGl.get());

    // setup imgui
    IMGUI_CHECKVERSION();
    mImgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(mImgui);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    //  disable file writing
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL3_InitForOpenGL(mWindow, sSdlGl)) {
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init()) {
        return false;
    }

    return true;
}

bool ContextImpl::Destroy() {
    if (IsCreated()) {
        MakeCurrent();
        RemoveActiveInstance(this);
        ImGui_ImplSDL3_Shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext(mImgui);
        SDL_DestroyProperties(mWindowProps);
        SDL_DestroyWindow(mWindow);
        mImgui = nullptr;
        mWindowProps = {};
        mWindow = nullptr;

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

bool ContextImpl::SetUIScale(double scale) {
    mScale = scale;
    // applied elsewhere
    return true;
}
double ContextImpl::GetUIScale() const {
    return mScale;
}
bool ContextImpl::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
    int32_t w{}, h{};
    bool success = SDL_GetWindowSize(mWindow, &w, &h);
    widthOut = static_cast<uint32_t>(w);
    heightOut = static_cast<uint32_t>(h);
    return success;
}
bool ContextImpl::GetSizeInPixels(uint32_t& widthOut, uint32_t& heightOut) const {
    int32_t w{}, h{};
    bool success = SDL_GetWindowSizeInPixels(mWindow, &w, &h);
    widthOut = static_cast<uint32_t>(w);
    heightOut = static_cast<uint32_t>(h);
    return success;
}
bool ContextImpl::SetSizeDirectly(uint32_t width, uint32_t height, bool resizable) {
    kitgui::log::TimeRegion r("ContextImpl::SetSizeDirectly()");
    if (!SDL_SetWindowResizable(mWindow, resizable)) {
        LOG_SDL_ERROR();
        return false;
    }
    double scale = GetUIScale();
    bool usesLogicalPixels = sApi == WindowApi::Wayland || sApi == WindowApi::Cocoa;
    int32_t targetWidth = static_cast<int32_t>(usesLogicalPixels ? width : width * scale);
    int32_t targetHeight = static_cast<int32_t>(usesLogicalPixels ? height : height * scale);
    if (!SDL_SetWindowSize(mWindow, targetWidth, targetHeight)) {
        LOG_SDL_ERROR();
        return false;
    }
    return true;
}
bool ContextImpl::SetParent(const kitgui::WindowRef& parentWindowRef) {
    return setParent_(sApi, mWindow, parentWindowRef);
}
bool ContextImpl::SetTransient(const kitgui::WindowRef& transientWindowRef) {
    return setTransient_(sApi, mWindow, transientWindowRef);
}
void ContextImpl::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    if (!SDL_SetWindowTitle(mWindow, titleTemp.c_str())) {
        LOG_SDL_ERROR();
    }
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
    if (sSdlGl) {
        if (!SDL_GL_MakeCurrent(mWindow, sSdlGl)) {
            LOG_SDL_ERROR();
        }
    }
    if (mImgui) {
        ImGui::SetCurrentContext(mImgui);
    }
    Magnum::Platform::GLContext::makeCurrent(sGl.get());
}

std::vector<ContextImpl*> ContextImpl::sActiveInstances = {};
kitgui::WindowApi ContextImpl::sApi = kitgui::WindowApi::Any;
SDL_GLContext ContextImpl::sSdlGl{};
std::unique_ptr<Magnum::Platform::GLContext> ContextImpl::sGl{};

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
            instance->mContext.Destroy();
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
            default: {
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
        instance->mContext.OnGuiUpdate();

        // draw
        int32_t width = 0, height = 0;
        SDL_GetWindowSizeInPixels(instance->mWindow, &width, &height);
        GL::defaultFramebuffer.setViewport({{}, {width, height}});
        GL::Renderer::setClearColor(instance->mClearColor);
        GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

        instance->mContext.OnDraw();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!SDL_GL_SwapWindow(instance->mWindow)) {
            LOG_SDL_ERROR();
        }
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }
}

bool ContextImpl::IsApiSupported(kitgui::WindowApi api, bool isFloating) {
    return isApiSupported_(api, isFloating);
}
bool ContextImpl::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    return getPreferredApi_(apiOut, isFloatingOut);
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
