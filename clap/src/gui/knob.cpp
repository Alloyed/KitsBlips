#include "gui/knob.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include "clap/ext/params.h"
#include "clapeze/ext/parameterConfigs.h"
#include "kitdsp/math/util.h"

namespace kitgui {
bool knob(const char* id, const clapeze::NumericParam& param, const KnobConfig& knobConfig, double& rawValueInOut) {
    // roughly adapted from https://github.com/altschuler/imgui-knobs/tree/main
    static char valueText[100];
    etl::span<char> valueSpan{valueText, sizeof(valueText)};
    clap_param_info_t info;
    param.FillInformation(0, &info);
    param.ToText(rawValueInOut, valueSpan);

    auto itemWidth = knobConfig.diameter < 0 ? ImGui::GetTextLineHeight() * 4.0f
                                             : knobConfig.diameter * ImGui::GetIO().FontGlobalScale;
    auto radius = itemWidth * 0.5f;
    auto screen_pos = ImGui::GetCursorScreenPos();

    ImGui::PushID(id);
    ImGui::PushItemWidth(itemWidth);

    ImGui::BeginGroup();

    // Handle dragging
    ImGui::InvisibleButton(id, {radius * 2.0f, radius * 2.0f});

    auto gid = ImGui::GetID(id);
    ImGuiSliderFlags drag_behaviour_flags =
        static_cast<ImGuiSliderFlags>(ImGuiSliderFlags_Vertical) | ImGuiSliderFlags_AlwaysClamp;
    double min = 0.0;
    double max = 1.0;
    auto value_changed =
        ImGui::DragBehavior(gid, ImGuiDataType_Double, &rawValueInOut, 0, &min, &max, "%.2f", drag_behaviour_flags);

    // extras
    ImGui::Text("%s", valueText);
    ImGui::Text("%s", info.name);

    if (ImGui::IsItemActive() && ImGui::IsMouseDoubleClicked(0)) {
        rawValueInOut = info.default_value;
    }

    {
        // Drawing
        auto angle_min = knobConfig.minAngleRadians < 0 ? kitdsp::kPi * 0.75f : knobConfig.minAngleRadians;
        auto angle_max = knobConfig.maxAngleRadians < 0 ? kitdsp::kPi * 2.25f : knobConfig.maxAngleRadians;

        ImVec2 center = {screen_pos[0] + radius, screen_pos[1] + radius};
        auto is_active = ImGui::IsItemActive();
        auto is_hovered = ImGui::IsItemHovered();
        auto angle = angle_min + (angle_max - angle_min) * rawValueInOut;
        auto angle_cos = cosf(angle);
        auto angle_sin = sinf(angle);
        auto* colors = ImGui::GetStyle().Colors;
        // circle
        auto size = 0.85f;
        auto circle_radius = size * radius;

        ImColor base = colors[ImGuiCol_ButtonActive];
        ImColor hovered = colors[ImGuiCol_ButtonHovered];
        ImColor active = colors[ImGuiCol_ButtonHovered];

        ImGui::GetWindowDrawList()->AddCircleFilled(center, circle_radius,
                                                    is_active ? active : (is_hovered ? hovered : base));

        // tick
        ImColor base2 = ImVec4(colors[ImGuiCol_ButtonActive].x * 0.5f, colors[ImGuiCol_ButtonActive].y * 0.5f,
                               colors[ImGuiCol_ButtonActive].z * 0.5f, colors[ImGuiCol_ButtonActive].w);
        ;
        ImColor hovered2 = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f, colors[ImGuiCol_ButtonHovered].y * 0.5f,
                                  colors[ImGuiCol_ButtonHovered].z * 0.5f, colors[ImGuiCol_ButtonHovered].w);
        ;
        ImColor active2 = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f, colors[ImGuiCol_ButtonHovered].y * 0.5f,
                                 colors[ImGuiCol_ButtonHovered].z * 0.5f, colors[ImGuiCol_ButtonHovered].w);
        ;
        auto start = 0.5f;
        auto end = 0.85f;
        auto width = 0.08f;
        auto tick_start = start * radius;
        auto tick_end = end * radius;
        ImGui::GetWindowDrawList()->AddLine({center[0] + angle_cos * tick_end, center[1] + angle_sin * tick_end},
                                            {center[0] + angle_cos * tick_start, center[1] + angle_sin * tick_start},
                                            is_active ? active2 : (is_hovered ? hovered2 : base2), width * radius);
    }

    ImGui::EndGroup();
    ImGui::PopItemWidth();
    ImGui::PopID();

    return value_changed;
}

}  // namespace kitgui