#pragma once

#ifndef KITGUI_USE_WIN32
#error "WIN32 is not enabled, including this file should be guarded by KITGUI_USE_WIN32"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include "kitgui/app.h"
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

struct ImGuiContext;

namespace kitgui {
class BaseApp;
struct SizeConfig;
}  // namespace kitgui

namespace kitgui::win32 {
class ContextImpl {
   public:
    explicit ContextImpl(kitgui::Context& ctx);
    ~ContextImpl() = default;
    static void init();
    static void deinit();

    // host events: (matches clap API)
    bool Create(kitgui::WindowApi api, bool isFloating);
    bool Destroy();
    bool SetScale(double scale);
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) const;
    bool SetSizeDirectly(uint32_t width, uint32_t height);
    bool SetParent(const kitgui::WindowRef& handle);
    bool SetTransient(const kitgui::WindowRef& handle);
    void SuggestTitle(std::string_view title);
    bool Show();
    bool Hide();
    bool Close();

    void MakeCurrent();

    static bool IsApiSupported(kitgui::WindowApi api, bool isFloating);
    static bool GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut);

    // use if we can monopolize the main thread
    static void RunLoop();
    // use if we can't. attach to an update loop (60hz or so)
    static void RunSingleFrame();

    bool IsCreated() const;
    void SetClearColor(Magnum::Color4 color);

    static LRESULT OnWindowsEvent(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

   private:
    bool CreateWglContext();
    void DestroyWglContext();

    kitgui::Context& mContext;
    kitgui::WindowApi mApi;
    ::HWND mWindow;
    ::HDC mDeviceContext;
    ::HGLRC mWglContext;
    std::unique_ptr<Magnum::Platform::GLContext> mGl = nullptr;
    ImGuiContext* mImgui = nullptr;
    bool mActive = false;
    bool mDestroy = false;
    Magnum::Color4 mClearColor = {0.5f, 0.5f, 0.5f, 1.0f};

    static void AddActiveInstance(ContextImpl* instance);
    static void RemoveActiveInstance(ContextImpl* instance);
    static ContextImpl* FindContextImplForWindow(HWND wnd);
    static std::vector<ContextImpl*> sActiveInstances;
};

}  // namespace kitgui::win32