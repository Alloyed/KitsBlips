#pragma once

#ifdef KITSBLIPS_ENABLE_GUI

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <sstream>
#include "clapeze/ext/gui.h"

#define CLAPEZE_LOG_SDL_ERROR(instance)                                \
    do {                                                               \
        std::stringstream buf;                                         \
        buf << __FILE__ << ": " << __LINE__ << ": " << SDL_GetError(); \
        instance->mHost.Log(clapeze::LogSeverity::Error, buf.str());   \
    } while (0)

namespace clapeze {

// Forward declares
class PluginHost;

class SdlOpenGlExt : public GuiExt {
   public:
    SdlOpenGlExt(PluginHost& host) : mHost(host) {}
    ~SdlOpenGlExt() = default;
    // api
    [[nodiscard]] virtual bool MakeCurrent();
    virtual void OnEvent(const SDL_Event& event) {}
    virtual void Update() {}
    virtual void Draw() {}

    // impl
   public:
    bool IsApiSupported(ClapWindowApi api, bool isFloating) override;
    bool GetPreferredApi(ClapWindowApi& apiOut, bool& isFloatingOut) override;
    bool Create(ClapWindowApi api, bool isFloating) override;
    void Destroy() override;
    bool SetScale(double scale) override;
    bool GetSize(uint32_t& widthOut, uint32_t& heightOut) override;
    bool CanResize() override;
    bool GetResizeHints(clap_gui_resize_hints_t& hintsOut) override;
    bool AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) override;
    bool SetSize(uint32_t width, uint32_t height) override;
    bool SetParent(WindowHandle handle) override;
    bool SetTransient(WindowHandle handle) override;
    void SuggestTitle(std::string_view title) override;
    bool Show() override;
    bool Hide() override;

   protected:
    PluginHost& mHost;
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mCtx;

   private:
    ClapWindowApi mApi;
    static void InitOnce();
    static void AddActiveInstance(SdlOpenGlExt* instance);
    static void RemoveActiveInstance(SdlOpenGlExt* instance);
    static SdlOpenGlExt* FindInstanceForWindow(SDL_WindowID window);
    static void UpdateInstances();
    static std::vector<SdlOpenGlExt*> sActiveInstances;
    static PluginHost::TimerId sUpdateTimerId;
};
}  // namespace clapeze

#endif