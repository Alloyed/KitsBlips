#pragma once

#if KITSBLIPS_ENABLE_GUI

#include <kitgui/context.h>
#include <kitgui/kitgui.h>
#include <cassert>
#include <string_view>
#include "clapeze/ext/gui.h"

namespace clapeze {

class PluginHost;

class KitguiFeature : public clapeze::GuiFeature {
   public:
    explicit KitguiFeature(clapeze::PluginHost& host,
                           kitgui::Context::AppFactory createAppFn,
                           const kitgui::SizeConfig& cfg = {});

    void Configure(BasePlugin& self) override;
    bool Validate(const BasePlugin& self) const override;

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

    kitgui::Context& GetContext() { return mCtx; };

   private:
    clapeze::PluginHost& mHost;
    kitgui::Context mCtx;
    static int32_t sInitCount;
    static ClapWindowApi sInitApi;  // only one API can be active per-process
    static clapeze::PluginHost::TimerId sTimerId;
};
}  // namespace clapeze
#endif