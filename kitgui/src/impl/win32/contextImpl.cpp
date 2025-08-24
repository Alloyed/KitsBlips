#include "impl/win32/contextImpl.h"

/*
 * here are the the primary references for this implementation:
 * https://github.com/ocornut/imgui/blob/master/examples/example_win32_opengl3/main.cpp
 * https://github.com/mosra/magnum/blob/master/src/Magnum/Platform/WindowlessWglApplication.cpp
 * https://nakst.gitlab.io/tutorial/clap-part-3.html
 */

#include <GL/gl.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Platform/GLContext.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <algorithm>
#include "imguiHelpers/misc.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"
#include "log.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite
// your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
// clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and hide them
// from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)  // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            // TODO: remove instance
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

std::string GetLastWinError() {
    // Retrieve the system error message for the last-error code

    char* lpMsgBuf;
    DWORD dw = GetLastError();
    size_t messageSize =
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

    std::string out(lpMsgBuf, messageSize);
    out.insert(0, std::format("Windows error({}): ", dw));

    LocalFree(lpMsgBuf);
    return out;
}
}  // namespace

using namespace Magnum;

namespace kitgui::win32 {
static constexpr char kClassName [] = "BLEHHH";  // TODO: runtime selectable

void ContextImpl::init() {
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASS windowClass = {};
    windowClass.lpfnWndProc = WndProc;
    windowClass.cbWndExtra = sizeof(ContextImpl*);
    windowClass.lpszClassName = kClassName;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.style = CS_DBLCLKS;
    RegisterClass(&windowClass);
}

void ContextImpl::deinit() {
    UnregisterClass(kClassName, NULL);
}

ContextImpl::ContextImpl(kitgui::Context& ctx) : mContext(ctx) {}

bool ContextImpl::Create(kitgui::WindowApi api, bool isFloating) {
    if (api != kitgui::WindowApi::Any && api != kitgui::WindowApi::Win32) {
        kitgui::log::error("API not supported");
        return false;
    }

    SizeConfig cfg = mContext.GetSizeConfig();
    DWORD windowStyle = 0;
    if (isFloating) {
        windowStyle |= WS_TILEDWINDOW;
    } else {
        windowStyle |= WS_CHILDWINDOW | WS_CLIPSIBLINGS;
    }
    if (cfg.resizable) {
        // support resizing
        windowStyle |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    mWindow = ::CreateWindow(
        // class name
        kClassName,
        // window name (fill in later)
        "",
        // window style
        windowStyle,
        // window position
        CW_USEDEFAULT, 0,
        // window size
        cfg.startingWidth, cfg.startingHeight,
        // parent
        GetDesktopWindow(),
        // menu
        NULL,
        // instance
        NULL,
        // param
        NULL);
    if (mWindow == nullptr) {
        kitgui::log::error(GetLastWinError());
        return false;
    }
    ::SetWindowLongPtr(mWindow, 0, reinterpret_cast<LONG_PTR>(this));

    if(!CreateWglContext()) {
        return false;
    }

    wglMakeCurrent(mDeviceContext, mWglContext);
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
    ImGui_ImplWin32_InitForOpenGL(mWindow);
    ImGui_ImplOpenGL3_Init();

    return true;
}

bool ContextImpl::Destroy() {
    if (IsCreated()) {
        MakeCurrent();
        RemoveActiveInstance(this);
        mGl.reset();
        DestroyWglContext();
        ::DestroyWindow(mWindow);
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

bool ContextImpl::SetScale(double scale) {
    // TODO: scale font
    ImGui::GetStyle().ScaleAllSizes(static_cast<float>(scale));
    return true;
}
bool ContextImpl::GetSize(uint32_t& widthOut, uint32_t& heightOut) const {
    RECT rect; 
    ::GetWindowRect(mWindow, &rect);
    widthOut = rect.right - rect.left;
    heightOut = rect.bottom - rect.top;
    return true;
}
bool ContextImpl::SetSizeDirectly(uint32_t width, uint32_t height) {
    return ::SetWindowPos(mWindow, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}
bool ContextImpl::SetParent(const kitgui::WindowRef& parentWindowRef) {
    return ::SetParent(mWindow, static_cast<HWND>(parentWindowRef.ptr));
}
bool ContextImpl::SetTransient([[maybe_unused]] const kitgui::WindowRef& transientWindowRef) {
    // TODO
    //return setTransient(mApi, mWindow, transientWindowRef);
    kitgui::log::error("SetTransient NYI");
    return false;
}
void ContextImpl::SuggestTitle(std::string_view title) {
    std::string titleTemp(title);
    ::SetWindowText(mWindow, titleTemp.c_str());
}

bool ContextImpl::Show() {
    ::ShowWindow(mWindow, SW_SHOW);
    AddActiveInstance(this);
    return true;
}
bool ContextImpl::Hide() {
    ::ShowWindow(mWindow, SW_HIDE);
    mActive = false;  // will be removed
    return true;
}
bool ContextImpl::Close() {
    mActive = false;  // will be removed
    mDestroy = true;  // will be destroyed
    return true;
}

void ContextImpl::MakeCurrent() {
    wglMakeCurrent(mDeviceContext, mWglContext);
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
    bool quitLoop = false;
    while (!quitLoop && !sActiveInstances.empty()) {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                quitLoop = true;
            }
        }
        ContextImpl::RunSingleFrame();
        ::Sleep(16);
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

    //SDL_Event event;
    //while (SDL_PollEvent(&event)) {
    //    SDL_Window* window = SDL_GetWindowFromEvent(&event);
    //    ContextImpl* instance = window ? FindContextImplForWindow(window) : nullptr;
    //    switch (event.type) {
    //        case SDL_EVENT_WINDOW_DESTROYED: {
    //            goto skip_event;
    //        }
    //        case SDL_EVENT_QUIT: {
    //            for (auto instanceIter : sActiveInstances) {
    //                instanceIter->Close();
    //            }
    //            break;
    //        }
    //        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
    //            if (instance) {
    //                instance->Close();
    //            }
    //            break;
    //        }
    //    }

    //    if (instance) {
    //        instance->MakeCurrent();
    //        ImGui_ImplSDL3_ProcessEvent(&event);
    //    }

    //skip_event:
    //    continue;
    //}

    for (auto instance : sActiveInstances) {
        instance->MakeCurrent();

        // update
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
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

        ::SwapBuffers(instance->mDeviceContext);
    }
}

bool ContextImpl::GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut) {
    apiOut = kitgui::WindowApi::Win32;
    isFloatingOut = false;
    return true;
}

//ContextImpl* ContextImpl::FindContextImplForWindow(SDL_Window* win) {
//    for (ContextImpl* instance : sActiveInstances) {
//        if (instance->mWindow == win) {
//            return instance;
//        }
//    }
//    return nullptr;
//}

bool ContextImpl::CreateWglContext() {
    mDeviceContext = ::GetDC(mWindow);
    if(mDeviceContext == nullptr)
    {
        kitgui::log::error(GetLastWinError());
        return false;
    }
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(mDeviceContext, &pfd);
    if (pf == 0 || ::SetPixelFormat(mDeviceContext, pf, &pfd) == false)
    {
        kitgui::log::error(GetLastWinError());
        return false;
    }

    mWglContext = wglCreateContext(mDeviceContext);
    return true;
}

void ContextImpl::DestroyWglContext() {
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(mWindow, mDeviceContext);
    mWglContext = nullptr;
    mDeviceContext = nullptr;
}
}  // namespace kitgui::sdl