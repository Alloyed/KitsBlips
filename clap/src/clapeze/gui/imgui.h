#pragma once

#include <SDL3/SDL_video.h>
#include "clapeze/gui/sdlOpenGl.h"

// Forward declares
class PluginHost;
struct ImGuiContext;

struct ImGuiConfig {
    std::function<void()> onGui;
};

class ImGuiExt : public SdlOpenGlExt {
   public:
    ImGuiExt(PluginHost& host, ImGuiConfig config) : SdlOpenGlExt(host), mConfig(config) {}
    ~ImGuiExt() = default;

    bool Create(ClapWindowApi api, bool isFloating) override;
    void Destroy() override;
    bool MakeCurrent() override;
    void OnEvent(const SDL_Event& event) override;
    void Update() override;
    void Draw() override;

   private:
    ImGuiConfig mConfig;
    ImGuiContext* mImgui = nullptr;
};
