#pragma once

#if KITSBLIPS_ENABLE_GUI

#include <kitgui/context.h>
#include <kitgui/kitgui.h>
#include <cassert>
#include <string_view>
#include "clapeze/ext/gui.h"

namespace clapeze {

class KitguiExt : public clapeze::GuiExt {
   public:
    KitguiExt(kitgui::Context::AppFactory createAppFn);
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
    kitgui::Context mCtx;
    static int32_t sInitCount;

    static kitgui::platform::Api ToPlatformApi(ClapWindowApi api);
};
}  // namespace clapeze
#endif