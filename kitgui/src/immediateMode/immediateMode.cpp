#include <kitgui/immediateMode.h>

#include <imgui.h>
#include "controls/knob.h"
#include "kitgui/gfx/scene.h"

namespace {
void Imgui_DrawSceneCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    kitgui::Scene* scene = static_cast<kitgui::Scene*>(cmd->UserCallbackData);
    scene->Draw();
}
}  // namespace

namespace kitgui {

bool ImGuiKnob(const char* label, float* value, float min, float max, float step) {
    double rawValue = static_cast<double>(*value);
    kitgui::ImGuiHelpers::KnobConfig cfg{};
    bool changed = kitgui::ImGuiHelpers::knob(label, cfg, rawValue);
    *value = static_cast<float>(rawValue);
    return changed;
}

bool ImGuiScene(const char* label, Scene& scene) {
    scene.Update();
    ImGui::GetWindowDrawList()->AddCallback(Imgui_DrawSceneCallback, &scene);
    return false;
}

}  // namespace kitgui
