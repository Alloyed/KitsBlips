#pragma once

#include <Magnum/Magnum.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include "kitgui/app.h"
#include "kitgui/kitgui.h"

struct ImGuiContext;

#if KITGUI_USE_SDL
namespace kitgui::sdl {
class ContextImpl;
}
#elif KITGUI_USE_WIN32
namespace kitgui::win32 {
class ContextImpl;
}
#endif

namespace kitgui {
class BaseApp;

struct SizeConfig {
    uint32_t startingWidth{400};
    uint32_t startingHeight{400};
    bool resizable{false};
    bool preserveAspectRatio{true};
};

class Context {
   public:
    using AppFactory = std::function<std::unique_ptr<BaseApp>(Context& ctx)>;
    explicit Context(AppFactory createAppFn);
    ~Context();
    static void init();
    static void deinit();

    // host events: (matches clap API)
    bool Create(kitgui::WindowApi api, bool isFloating);
    bool Destroy();
    bool SetScale(double scale);
    const SizeConfig& GetSizeConfig() const;
    void SetSizeConfig(const SizeConfig& cfg);
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

    // app events. forwards otherwise hidden events to impl
   protected:
    void OnActivate();
    void OnDeactivate();
    void OnUpdate();
    void OnDraw();

   private:
#if KITGUI_USE_SDL
    using Impl = sdl::ContextImpl;
    friend class sdl::ContextImpl;
#elif KITGUI_USE_WIN32
    using Impl = win32::ContextImpl;
    friend class win32::ContextImpl;
#endif

    AppFactory mCreateAppFn;
    SizeConfig mSizeConfig{};
    std::unique_ptr<BaseApp> mApp;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace kitgui