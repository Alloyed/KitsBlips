#pragma once

// has to be first

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include "kitgui/app.h"
#include "kitgui/kitgui.h"

struct ImGuiContext;

namespace kitgui {
class BaseApp;
class Context {
   public:
    Context() = default;
    ~Context() = default;

    bool Create(platform::Api api, bool isFloating);
    bool Destroy();
    bool SetScale(double scale);
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) const;
    bool CanResize();
    // bool GetResizeHints(clap_gui_resize_hints_t& hintsOut);
    bool AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) const;
    bool SetSize(uint32_t width, uint32_t height);
    bool SetParent(SDL_Window* handle);
    bool SetTransient(SDL_Window* handle);
    void SuggestTitle(std::string_view title);
    bool Show();
    bool Hide();
    bool Close();

    void MakeCurrent();

    static bool GetPreferredApi(platform::Api& apiOut, bool& isFloatingOut);
    static Context* FindContextForWindow(SDL_WindowID window);

    // use if we can monopolize the main thread
    static void RunLoop();
    // use if we can't. attach to an update loop (60hz or so)
    static void RunSingleFrame();

    bool IsCreated() const;
    void SetClearColor(Magnum::Color4 color) { mClearColor = color; }
    void SetApp(std::shared_ptr<BaseApp> app) { mApp = app; }

   private:
    std::shared_ptr<BaseApp> mApp;
    platform::Api mApi;
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mSdlGl;
    std::unique_ptr<Magnum::Platform::GLContext> mGl = nullptr;
    ImGuiContext* mImgui = nullptr;
    bool mActive = false;
    bool mDestroy = false;
    Magnum::Color4 mClearColor = {0.5f, 0.5f, 0.5f, 1.0f};

    static void AddActiveInstance(Context* instance);
    static void RemoveActiveInstance(Context* instance);
    static std::vector<Context*> sActiveInstances;
};

}  // namespace kitgui