#pragma once

// has to be first
#include <glad/gl.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <string_view>
#include <vector>
#include "imgui_internal.h"
#include "kitgui/kitgui.h"

namespace kitgui {
class Context {
   public:
    Context() = default;
    ~Context() = default;

    bool Create(platform::Api api, bool isFloating);
    bool Destroy();

    bool IsCreated() const;

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

    const GladGLContext& MakeCurrent() const;

    static bool GetPreferredApi(platform::Api& apiOut, bool& isFloatingOut);
    static Context* FindContextForWindow(SDL_WindowID window);

   private:
    platform::Api mApi;
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mSdlGl;
    GladGLContext mGl;
    ImGuiContext* mImgui = nullptr;

    static void AddActiveInstance(Context* instance);
    static void RemoveActiveInstance(Context* instance);
    static void RunLoop();
    static void RunSingleFrame();
    static std::vector<Context*> sActiveInstances;
};

}  // namespace kitgui