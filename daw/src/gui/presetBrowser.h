#pragma once
#if KITSBLIPS_ENABLE_GUI
#include <clap/ext/params.h>
#include <clap/factory/preset-discovery.h>
#include <clapeze/basePlugin.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/features/baseFeature.h>
#include <clapeze/features/params/baseParameter.h>
#include <clapeze/features/params/parameterTypes.h>
#include <clapeze/features/presetFeature.h>
#include <imgui.h>
#include <kitgui/controls/knob.h>
#include <kitgui/controls/toggleSwitch.h>
#include <misc/cpp/imgui_stdlib.h>

namespace kitgui {
class PresetBrowser {
   public:
    const char* kPopupName = "Presets";
    explicit PresetBrowser(clapeze::BasePlugin& plugin) : mPlugin(plugin) {}
    void PopulatePresets() {
        clapeze::AssetsFeature& assets = clapeze::BaseFeature::GetFromPlugin<clapeze::AssetsFeature>(mPlugin);
        mStateTodo = assets.GetAllPathsFromPlugin();
    }
    void Update() {
        clapeze::PresetFeature& presets = clapeze::BaseFeature::GetFromPlugin<clapeze::PresetFeature>(mPlugin);
        if (ImGui::BeginPopupModal(kPopupName)) {
            // preset list
            ImGui::BeginGroup();
            if (ImGui::BeginListBox("wawa")) {
                for (const auto& path : mStateTodo) {
                    if (ImGui::Selectable(path.c_str())) {
                        // load
                        presets.LoadPreset(CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, path.c_str());
                    }
                }
            }
            ImGui::EndListBox();
            ImGui::EndGroup();

            ImGui::SameLine();

            // preset info
            ImGui::BeginGroup();
            clapeze::PresetInfo info;
            ImGui::InputText("name", &info.name);
            ImGui::InputText("creator", &info.creator);
            ImGui::InputTextMultiline("description", &info.description);
            // ImGui::InputText("tags", &info);
            if (ImGui::Button("Revert")) {
                // revert
                presets.LoadLastPreset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                // save
                presets.SavePreset("wawa");
            }
            ImGui::EndGroup();

            ImGui::EndPopup();
        }
    }

   private:
    clapeze::BasePlugin& mPlugin;
    std::vector<std::string> mStateTodo;
};
}  // namespace kitgui
#endif