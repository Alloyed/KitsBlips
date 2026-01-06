#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include "kitgui/app.h"
#include "kitgui/kitgui.h"
#include "kitgui/types.h"

#if KITGUI_USE_SDL
namespace kitgui::sdl {
class ContextImpl;
}
#elif KITGUI_USE_WIN32
namespace kitgui::win32 {
class ContextImpl;
}
#elif KITGUI_USE_COCOA
namespace kitgui::cocoa {
class ContextImpl;
}
#endif

namespace kitgui {
class BaseApp;
class FileContext;

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
    static void init(kitgui::WindowApi api, bool isFloating);
    static void deinit();

    // host events: (matches clap API)
    bool Create(kitgui::WindowApi api,
                bool isFloating);  // TODO: remove parameters and access them if necessary from the static init() call
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
    /*
     * Some platforms (eg. windows) will attach themselves automagically to the existing gui loop. others, like, SDL,
     * need manual taping up. you can do this by calling RunSingleFrame() in the parent loop.
     */
    static bool NeedsUpdateLoopIntegration();

    bool IsCreated() const;
    void SetClearColor(Color4 color);
    FileContext* GetFileContext() const;

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
#elif KITGUI_USE_COCOA
    using Impl = cocoa::ContextImpl;
    friend class cocoa::ContextImpl;
#endif

    AppFactory mCreateAppFn;
    SizeConfig mSizeConfig{};
    std::unique_ptr<FileContext> mFileContext;
    std::unique_ptr<Impl> mImpl;
    std::unique_ptr<BaseApp> mApp;

    friend class FileContext;
};

}  // namespace kitgui