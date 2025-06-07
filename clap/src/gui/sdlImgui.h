#pragma once

#include <SDL3/SDL_video.h>
#include <cstdint>
#include "clapApi/ext/gui.h"
#include "gui/platform/platform.h"

// Forward declares
class PluginHost;

struct SdlImguiConfig {
    std::function<void()> onGui;
};

class SdlImguiExt : public GuiExt {
   public:
    SdlImguiExt(PluginHost& host, const SdlImguiConfig config) : mHost(host), mConfig(config) {}
    ~SdlImguiExt() = default;
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

   private:
    void Update(float dt);

    PluginHost& mHost;
    SdlImguiConfig mConfig;
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mCtx;
    PluginHost::TimerId mTimerId = 0;
};
