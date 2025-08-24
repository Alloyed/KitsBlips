#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/GLContext.h>
#include <SDL3/SDL_video.h>
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

namespace kitgui::sdl {
class ContextImpl {
   public:
    explicit ContextImpl(kitgui::Context& ctx);
    ~ContextImpl() = default;
    static void init();
    static void deinit();

    // host events: (matches clap API)
    bool Create(platform::Api api, bool isFloating);
    bool Destroy();
    bool SetScale(double scale);
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) const;
    bool SetSizeDirectly(uint32_t width, uint32_t height);
    bool SetParent(const platform::WindowRef& handle);
    bool SetTransient(const platform::WindowRef& handle);
    void SuggestTitle(std::string_view title);
    bool Show();
    bool Hide();
    bool Close();

    void MakeCurrent();

    static bool GetPreferredApi(platform::Api& apiOut, bool& isFloatingOut);

    // use if we can monopolize the main thread
    static void RunLoop();
    // use if we can't. attach to an update loop (60hz or so)
    static void RunSingleFrame();

    bool IsCreated() const;
    void SetClearColor(Magnum::Color4 color) { mClearColor = color; }

   private:
    kitgui::Context& mContext;
    platform::Api mApi;
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mSdlGl;
    std::unique_ptr<Magnum::Platform::GLContext> mGl = nullptr;
    ImGuiContext* mImgui = nullptr;
    bool mActive = false;
    bool mDestroy = false;
    Magnum::Color4 mClearColor = {0.5f, 0.5f, 0.5f, 1.0f};

    static void AddActiveInstance(ContextImpl* instance);
    static void RemoveActiveInstance(ContextImpl* instance);
    static ContextImpl* FindContextImplForWindow(SDL_Window* win);
    static std::vector<ContextImpl*> sActiveInstances;
};

}  // namespace kitgui::sdl