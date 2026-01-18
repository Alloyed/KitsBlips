#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "kitgui/app.h"
#include "kitgui/kitgui.h"
#include "kitgui/types.h"

#if KITGUI_USE_PUGL
namespace kitgui::pugl {
class ContextImpl;
}
#elif KITGUI_USE_SDL
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
    uint32_t startingWidth{600};
    uint32_t startingHeight{600};
    bool resizable{false};
    bool preserveAspectRatio{true};
};

/**
 * a kitgui::Context holds most of kitgui's internal state.
 */
class Context {
   public:
    using AppFactory = std::function<std::unique_ptr<BaseApp>(Context& ctx)>;
    using FileLoader = std::function<std::optional<std::string>(std::string_view path)>;
    using Logger = std::function<void(std::string_view message)>;

    /**
     * Creates a new application context. you can customize the application by subclassing and then constructing an
     * instance of BaseApp. createAppFn is called only on the first call to `context::Create()`, so just constructing a
     * context won't have any side effects.
     */
    explicit Context(AppFactory createAppFn);
    ~Context();
    /** Runs any initialization the UI backend needs. run at most once, at the start of your application */
    static void init(kitgui::WindowApi api, std::string_view appName);
    /** cleans up the init() call. ensure all contexts are destroyed before doing this! */
    static void deinit();

    /**
     * Creates a window for this context. if isFloating is true, the window will be instatiated independently, if false,
     * we'll prepare for this window to be embedded into another one using Context::SetParent().
     */
    bool Create(bool isFloating);
    /**
     * Destroys the window for this context. it can be re-created again after the fact.
     */
    bool Destroy();
    /**
     * Set the desired scale of the UI as a multiplier: (eg. 2.0 means 200% scaling)
     */
    bool SetScale(double scale);
    const SizeConfig& GetSizeConfig() const;
    void SetSizeConfig(const SizeConfig& cfg);
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) const;
    bool SetSizeDirectly(uint32_t width, uint32_t height, bool resizable);
    /**
     * Embeds this window into another OS-level window.
     * This context must have been created with isFloating == false to work.
     */
    bool SetParent(const kitgui::WindowRef& handle);
    /**
     * Tells the OS that this window should be treated as part of the group that contains this OS-level window, ala a
     * popup. This context must have been created with isFloating == true to work.
     */
    bool SetTransient(const kitgui::WindowRef& handle);
    /**
     * Tells the OS that this window should have the provided title.
     * This context must have been created with isFloating == true to work.
     */
    void SuggestTitle(std::string_view title);
    /**
     * Shows this window. Create() does not do this for you; do it after all your setup is done.
     */
    bool Show();
    /**
     * Hides this window.
     */
    bool Hide();
    /**
     * Closes this window.
     */
    bool Close();

    /**
     * For APIs with global state (for example, OpenGL), this method marks this context as the current one that should
     * be rendered to and so on.
     */
    void MakeCurrent();

    /**
     * Returns true if the provided api/isFloating combo is supported.
     */
    static bool IsApiSupported(kitgui::WindowApi api, bool isFloating);
    /**
     * Looks at the current OS environment and suggests an api and isFloating combo to use.
     * Returns false if no combination will work.
     */
    static bool GetPreferredApi(kitgui::WindowApi& apiOut, bool& isFloatingOut);

    /**
     * Use to run an event loop for the current window. This is convenient if kitgui is the only UI in the current
     * application.
     */
    static void RunLoop();
    /**
     * Use to update kitgui within another update loop.
     * This may be necessary for your platform (see `NeedsUpdateLoopIntegration`)
     */
    static void RunSingleFrame();
    /**
     * Some platforms (eg. windows) will attach themselves automagically to the existing gui loop. others, like, SDL,
     * need manual taping up. you can do this by calling RunSingleFrame() in the parent loop.
     */
    static bool NeedsUpdateLoopIntegration();

    /**
     * Returns true if context.Create() has been called, and context.Destroy() has not.
     */
    bool IsCreated() const;
    /**
     * Sets the color the window's background is set to when cleared.
     */
    void SetClearColor(Color4 color);
    /**
     * The file context can be used to load assets associated with the gui.
     */
    FileContext* GetFileContext() const;
    /**
     * Sets the mechanism that `GetFileContext()` uses to load asset files. the default mechanism uses stdio.
     */
    void SetFileLoader(FileLoader fn);
    /**
     * Sets the mechanism that kitgui uses to log info. the default mechanism uses stdout.
     */
    static void SetLogger(Logger fn);

   protected:
    // app events. forwards otherwise hidden events to impl
    void OnActivate();
    void OnDeactivate();
    void OnUpdate();
    void OnDraw();

   private:
#if KITGUI_USE_PUGL
    using Impl = pugl::ContextImpl;
    friend class pugl::ContextImpl;
#elif KITGUI_USE_SDL
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
    bool mSizeConfigChanged{false};
    std::unique_ptr<FileContext> mFileContext;
    std::unique_ptr<Impl> mImpl;
    std::unique_ptr<BaseApp> mApp;

    friend class FileContext;
};

}  // namespace kitgui