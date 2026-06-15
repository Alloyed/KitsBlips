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
#include <filesystem>

namespace kitgui {
class PresetBrowser {
   public:
    static inline const char* kPopupName = "Presets";
    explicit PresetBrowser(clapeze::BasePlugin& plugin) : mPlugin(plugin) {}
    void PopulatePresets() {
        clapeze::AssetsFeature& assets = clapeze::BaseFeature::GetFromPlugin<clapeze::AssetsFeature>(mPlugin);
        mBuiltinPresets = assets.GetAllPathsFromPlugin("assets/kitsblips.kitskeys/");
        for(int32_t idx = narrow_cast<int32_t>(mBuiltinPresets.size())-1; idx >= 0; idx--) {
            auto ext = std::filesystem::path(mBuiltinPresets[idx]).extension();
            if(ext != ".preset") {
                mBuiltinPresets.erase(mBuiltinPresets.begin()+idx);
            }
        }
    }
    void Update() {
        if (mOpen) {
            //mOpen = false;
            // TODO: BeginPopupModal doesn't seem to be working
            if (ImGui::Begin("Presets", &mOpen)) {
                clapeze::PresetFeature& presets = clapeze::BaseFeature::GetFromPlugin<clapeze::PresetFeature>(mPlugin);
                if(ImGui::BeginTable("##columns", 2, ImGuiTableFlags_SizingStretchSame)) {
                    int32_t idx = 0;

                    // preset list
                    ImGui::TableNextColumn();
                    mFilter.Draw();
                    if (ImGui::BeginListBox("##PresetList", ImVec2(-FLT_MIN, -FLT_MIN))) {
                        const auto& lastLocation = presets.GetLastPresetLocation();
                        for (const auto& path : mBuiltinPresets) {
                            ImGui::PushID(++idx);
                            std::string name = std::filesystem::path(path).stem().generic_string();
                            bool selected = lastLocation.has_value() && lastLocation->kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN ? path == lastLocation->location : false;
                            if ((selected || mFilter.PassFilter(name.c_str())) && ImGui::Selectable(name.c_str(), selected)) {
                                // load
                                presets.LoadPreset(CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, path.c_str());
                            }
                            ImGui::PopID();
                        }
                        for (const auto& path : mFilePresets) {
                            ImGui::PushID(++idx);
                            std::string name = std::filesystem::path(path).stem().generic_string();
                            bool selected = lastLocation.has_value() && lastLocation->kind == CLAP_PRESET_DISCOVERY_LOCATION_FILE ? path == lastLocation->location : false;
                            if ((selected || mFilter.PassFilter(name.c_str())) && ImGui::Selectable(name.c_str(), selected)) {
                                // load
                                presets.LoadPreset(CLAP_PRESET_DISCOVERY_LOCATION_FILE, path.c_str(), nullptr);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndListBox();
                    }

                    // preset info
                    ImGui::TableNextColumn();

                    clapeze::PresetInfo& info = presets.GetPresetInfo();
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
                        //presets.SavePreset(mCurrentPresetPath.c_str());
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }
    }

    void PresetMenu() {
        if (ImGui::MenuItem("Browser")) {
            PopulatePresets();
            mOpen = true;
        }
        if(ImGui::BeginMenu("Built-in")) {
            for(const auto& file : mBuiltinPresets) {
                ImGui::MenuItem(file.c_str());
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("User")) {
            for(const auto& file : mFilePresets) {
                ImGui::MenuItem(file.c_str());
            }
            ImGui::EndMenu();
        }
    }

    bool mOpen = false;
   private:
    clapeze::BasePlugin& mPlugin;
    ImGuiTextFilter mFilter;
    std::vector<std::string> mBuiltinPresets;
    std::vector<std::string> mFilePresets;
};
}  // namespace kitgui
#endif