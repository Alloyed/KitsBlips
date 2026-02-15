#if KITSBLIPS_ENABLE_GUI
#include "gui/kitguiFeature.h"

#include <clap/ext/timer-support.h>
#include <clapeze/basePlugin.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/features/baseGuiFeature.h>
#include <clapeze/impl/streamUtils.h>
#include <clapeze/pluginHost.h>
#include "kitgui/context.h"
#include "kitgui/kitgui.h"

namespace {

clapeze::ClapWindowApi toClapeze(kitgui::WindowApi api) {
    switch (api) {
        case kitgui::WindowApi::Any: {
            kitgui::WindowApi guiApi{};
            bool unused{};
            kitgui::Context::GetPreferredApi(guiApi, unused);
            return toClapeze(guiApi);
        }
        case kitgui::WindowApi::Win32: {
            return clapeze::ClapWindowApi::Win32;
        }
        case kitgui::WindowApi::Cocoa: {
            return clapeze::ClapWindowApi::Cocoa;
        }
        case kitgui::WindowApi::X11: {
            return clapeze::ClapWindowApi::X11;
        }
        case kitgui::WindowApi::Wayland: {
            return clapeze::ClapWindowApi::Wayland;
        }
    }
    assert(false);
    return clapeze::ClapWindowApi::None;
}

kitgui::WindowApi toKitGui(clapeze::ClapWindowApi api) {
    switch (api) {
        case clapeze::ClapWindowApi::None: {
            return kitgui::WindowApi::Any;
        }
        case clapeze::ClapWindowApi::Win32: {
            return kitgui::WindowApi::Win32;
        }
        case clapeze::ClapWindowApi::Cocoa: {
            return kitgui::WindowApi::Cocoa;
        }
        case clapeze::ClapWindowApi::X11: {
            return kitgui::WindowApi::X11;
        }
        case clapeze::ClapWindowApi::Wayland: {
            return kitgui::WindowApi::Wayland;
        }
    }
    assert(false);
    return kitgui::WindowApi::Any;
}
}  // namespace

namespace clapeze {

int32_t KitguiFeature::sInitCount = 0;
ClapWindowApi KitguiFeature::sInitApi = ClapWindowApi::None;
PluginHost::TimerId KitguiFeature::sTimerId = 0;

KitguiFeature::KitguiFeature(PluginHost& mPluginHost,
                             kitgui::Context::AppFactory createAppFn,
                             const kitgui::SizeConfig& cfg)
    : mHost(mPluginHost), mCtx(std::move(createAppFn)) {
    mCtx.SetSizeConfig(cfg);
    mCtx.SetLogger([this](std::string_view message) {
        std::string tmp{message};
        mHost.Log(LogSeverity::Info, tmp);
    });
}

void KitguiFeature::Configure(BasePlugin& self) {
    GuiFeature::Configure(self);
    mCtx.SetFileLoader([&self](std::string_view path) -> std::optional<std::string> {
        auto& assets = clapeze::AssetsFeature::GetFromPlugin<clapeze::AssetsFeature>(self);
        std::string pathstr{path};
        miniz_istream stream = assets.OpenFromPlugin(pathstr.c_str());
        std::string out = clapeze::impl::istream_tostring(stream);
        return out;
    });
}

bool KitguiFeature::Validate(const BasePlugin& plugin) const {
    if (plugin.TryGetFeature(AssetsFeature::NAME) == nullptr) {
        plugin.GetHost().Log(LogSeverity::Fatal, "Gui needs AssetsFeature. Please add it");
        return false;
    }
#ifdef __linux__
    if (!plugin.GetHost().HostSupportsExtension(CLAP_EXT_TIMER_SUPPORT)) {
        plugin.GetHost().Log(LogSeverity::Fatal, "Can't run gui on hosts without timer support");
        return false;
    }
#endif

    return true;
}

bool KitguiFeature::IsApiSupported(ClapWindowApi api, bool isFloating) {
    return kitgui::Context::IsApiSupported(toKitGui(api), isFloating);
}

bool KitguiFeature::GetPreferredApi(ClapWindowApi& apiOut, bool& isFloatingOut) {
    kitgui::WindowApi guiApi{};
    kitgui::Context::GetPreferredApi(guiApi, isFloatingOut);
    apiOut = toClapeze(guiApi);
    return true;
}

bool KitguiFeature::Create(ClapWindowApi api, bool isFloating) {
    sInitCount++;
    if (sInitCount == 1) {
        sInitApi = api;
        kitgui::Context::init(toKitGui(api), PRODUCT_NAME);
        if (kitgui::Context::NeedsUpdateLoopIntegration()) {
            sTimerId = mHost.AddTimer(32, []() { kitgui::Context::RunSingleFrame(); });
        }
    } else {
        assert(sInitApi == api);  // only one api supported at a time
    }

    return mCtx.Create(isFloating);
}

void KitguiFeature::Destroy() {
    mCtx.Destroy();
    sInitCount--;
    if (sInitCount <= 0) {
        // todo: deinit only once
        kitgui::Context::deinit();
        if (sTimerId) {
            mHost.CancelTimer(sTimerId);
            sTimerId = {};
        }
    }
}

bool KitguiFeature::SetScale(double scale) {
    return mCtx.SetUIScale(scale);
}

bool KitguiFeature::GetSize(uint32_t& widthOut, uint32_t& heightOut) {
    uint32_t width{};
    uint32_t height{};
    if (!mCtx.GetSize(width, height)) {
        return false;
    }
    // logical pixels on mac/wayland, physical pixels on win/X11
    widthOut = width;
    heightOut = height;

    if (sInitApi == ClapWindowApi::X11) {
        // hack for x11 highdpi
        std::string_view name = mHost.GetName();
        if (name == "Clap Test Host"             // bad news for standardization...
            || name == "renoise (CLAP-as-VST3)"  //
            || name == "REAPER"                  // but not the VST wrapper?
        ) {
            // note that the output of mCtx is already in physical pixels, so I'm not sure why these platforms need the
            // scale factor. in effect they must be dividing by scale internally to get the actual size....
            double scale = mCtx.GetUIScale();
            widthOut = static_cast<uint32_t>(static_cast<double>(width) * scale);
            heightOut = static_cast<uint32_t>(static_cast<double>(height) * scale);
        }
    }
    return true;
}

bool KitguiFeature::CanResize() {
    return mCtx.GetSizeConfig().resizable;
}

bool KitguiFeature::GetResizeHints(clap_gui_resize_hints_t& hintsOut) {
    const auto& cfg = mCtx.GetSizeConfig();
    hintsOut.preserve_aspect_ratio = cfg.preserveAspectRatio;
    hintsOut.aspect_ratio_width = cfg.startingWidth;
    hintsOut.aspect_ratio_height = cfg.startingHeight;
    hintsOut.can_resize_horizontally = cfg.resizable;
    hintsOut.can_resize_vertically = cfg.resizable;
    return true;
}

bool KitguiFeature::AdjustSize(uint32_t& widthInOut, uint32_t& heightInOut) {
    const auto& cfg = mCtx.GetSizeConfig();
    if (cfg.resizable && cfg.preserveAspectRatio) {
        heightInOut = widthInOut * cfg.startingWidth / cfg.startingHeight;
    }
    return true;
}

bool KitguiFeature::SetSize(uint32_t width, uint32_t height) {
    const auto& cfg = mCtx.GetSizeConfig();
    return mCtx.SetSizeDirectly(width, height, cfg.resizable);
}

bool KitguiFeature::SetParent(WindowHandle handle) {
    kitgui::WindowRef desiredParent = kitgui::wrapWindow(toKitGui(handle.api), handle.ptr);
    return mCtx.SetParent(desiredParent);
}

bool KitguiFeature::SetTransient(WindowHandle handle) {
    kitgui::WindowRef desiredTransient = kitgui::wrapWindow(toKitGui(handle.api), handle.ptr);
    return mCtx.SetTransient(desiredTransient);
}

void KitguiFeature::SuggestTitle(std::string_view title) {
    mCtx.SuggestTitle(title);
}

bool KitguiFeature::Show() {
    return mCtx.Show();
}

bool KitguiFeature::Hide() {
    return mCtx.Hide();
}

}  // namespace clapeze

#endif