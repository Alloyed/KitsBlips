#pragma once

#include <SDL3/SDL_video.h>
#include <cstdint>
#include <memory>
#include "clapApi/ext/gui.h"

// Forward declares
class PluginHost;

struct SdlImguiConfig {
    std::function<void()> onGui;
};

class SdlImguiExt : public GuiExt {
   public:
    SdlImguiExt(PluginHost& host, const SdlImguiConfig config) : mHost(host), mConfig(config) {}
    ~SdlImguiExt() = default;
    bool IsApiSupported(WindowingApi api, bool isFloating) override;
    bool GetPreferredApi(WindowingApi& apiOut, bool& isFloatingOut) override;
    bool Create(WindowingApi api, bool isFloating) override;
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

    static bool OnAppInit();
    static bool OnAppQuit();
    static int32_t sInstances;

   private:
    void CreatePluginWindow();
    void CreateParentWindow(WindowHandle clapHandle);
    void Update(float dt);

    PluginHost& mHost;
    SdlImguiConfig mConfig;
    SDL_Window* mWindowHandle = nullptr;
    SDL_Window* mParentHandle = nullptr;
    SDL_GLContext mCtx;
};
