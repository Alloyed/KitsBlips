#include "kitgui/controls/knob.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <string>
#include <algorithm>

namespace kitgui {
bool Knob::Update(double& rawValueInOut) {
    // roughly adapted from https://github.com/altschuler/imgui-knobs/tree/main

    auto itemWidth = mWidth ? (*mWidth) * ImGui::GetIO().FontGlobalScale : ImGui::GetTextLineHeight() * 2.0f;
    ImVec2 screen_pos = mPos ? ImVec2{mPos->x(), mPos->y()} : ImGui::GetCursorScreenPos();

    ImGui::PushID(static_cast<void*>(this));
    ImGui::PushItemWidth(itemWidth);
    ImGui::BeginGroup();
    ImGui::SetCursorScreenPos(screen_pos);

    // Handle dragging
    ImGui::InvisibleButton("button", {itemWidth, itemWidth});
    auto gid = ImGui::GetID("button");

    ImGuiSliderFlags drag_behaviour_flags =
        static_cast<ImGuiSliderFlags>(ImGuiSliderFlags_Vertical) | ImGuiSliderFlags_AlwaysClamp;
    auto value_changed =
        ImGui::DragBehavior(gid, ImGuiDataType_Double, &rawValueInOut, 0, &mMin, &mMax, "%.2f", drag_behaviour_flags);

    auto is_active = ImGui::IsItemActive();
    auto is_hovered = ImGui::IsItemHovered();

    // responses
    if (is_active) {
        // tooltip counts as a window! so this works
        ImGui::SetNextWindowPos({screen_pos.x + itemWidth, screen_pos.y + (itemWidth * 0.5f)});
        ImGui::SetTooltip("%s", FormatValueText(rawValueInOut).c_str());
    } else if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        // descriptive tooltip
        ImGui::SetTooltip("%s", GetName().c_str());
    }

    if (is_active && ImGui::IsMouseDoubleClicked(0)) {
        rawValueInOut = GetDefault();
        value_changed = true;
    }

    if (mShowDebug) {
        // Drawing using imgui primitives
        auto angle_min = mMinAngleRadians;
        auto angle_max = mMaxAngleRadians;
        auto normalizedValue = std::clamp((rawValueInOut - mMin) / (mMax - mMin), 0.0, 1.0);

        auto radius = itemWidth * 0.5f;
        ImVec2 center = {screen_pos[0] + radius, screen_pos[1] + radius};
        auto angle = angle_min + (angle_max - angle_min) * normalizedValue;
        auto angle_cos = std::cos(static_cast<float>(angle));
        auto angle_sin = std::sin(static_cast<float>(angle));
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
        ImColor hovered2 = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f, colors[ImGuiCol_ButtonHovered].y * 0.5f,
                                  colors[ImGuiCol_ButtonHovered].z * 0.5f, colors[ImGuiCol_ButtonHovered].w);
        ImColor active2 = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f, colors[ImGuiCol_ButtonHovered].y * 0.5f,
                                 colors[ImGuiCol_ButtonHovered].z * 0.5f, colors[ImGuiCol_ButtonHovered].w);
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