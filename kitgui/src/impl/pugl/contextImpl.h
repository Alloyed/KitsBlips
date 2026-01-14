#pragma once

#ifndef KITGUI_USE_PUGL
#error "PUGL is not enabled, including this file should be guarded by KITGUI_USE_PUGL"
#endif

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <pugl/pugl.h>
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

namespace kitgui::pugl {
class ContextImpl {
   public:
    explicit ContextImpl(kitgui::Context& ctx);
    ~ContextImpl() = default;
    static void init(kitgui::WindowApi api, std::string_view appName);
    static void deinit();

    // host events: (matches clap API)
    bool Create(bool isFloating);
    bool Destroy();
    bool SetScale(double scale);
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) const;
    bool SetSizeDirectly(uint32_t width, uint32_t height, bool resizable);
    bool SetParent(const kitgui::WindowRef& handle);
    bool SetTransient(const kitgui::WindowRef& handle);
    void SuggestTitle(std::string_view title);
    bool Show();
    bool Hide();
    bool Close();

    void MakeCurrent();
    static void ClearCurrent();

    static bool IsApiSupported(kitgui::WindowApi api, bool isFloating);
    static bool GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut);

    // use if we can monopolize the main thread
    static void RunLoop();
    // use if we can't. attach to an update loop (60hz or so)
    static void RunSingleFrame();
    static bool NeedsUpdateLoopIntegration() { return true; }
    PuglStatus OnPuglEvent(const PuglEvent* event);
    bool Realize();
    void Unrealize();
    void OnResizeOrMove();

    bool IsCreated() const;
    void SetClearColor(Magnum::Color4 color);

   private:
    kitgui::Context& mContext;
    kitgui::WindowApi mApi;
    PuglView* mView;
    std::unique_ptr<Magnum::Platform::GLContext> mGl = nullptr;
    ImGuiContext* mImgui = nullptr;
    Magnum::Color4 mClearColor = {0.8f, 0.8f, 0.0f, 1.0f};

    static PuglWorld* sWorld;
    static kitgui::WindowApi sApi;
    static int32_t sNumInstances;
    static std::vector<kitgui::Context*> sToClose;
};

}  // namespace kitgui::pugl