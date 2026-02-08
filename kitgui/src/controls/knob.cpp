#include "kitgui/controls/knob.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace kitgui {
bool Knob::Update(double& rawValueInOut) {
    // roughly adapted from https://github.com/altschuler/imgui-knobs/tree/main
    float scale = ImGui::GetIO().FontGlobalScale;
    auto itemWidth = mWidth ? (*mWidth) * scale : ImGui::GetTextLineHeight() * 2.0f;
    ImVec2 screen_pos = mPos ? ImVec2{mPos->x(), mPos->y()} : ImGui::GetCursorScreenPos();

    ImGui::PushID(static_cast<void*>(this));
    ImGui::PushItemWidth(itemWidth);
    ImGui::BeginGroup();
    ImGui::SetCursorScreenPos(screen_pos);

    // Handle dragging
    ImGui::InvisibleButton("button", {itemWidth, itemWidth});
    auto gid = ImGui::GetID("button");

    ImGuiSliderFlags drag_behaviour_flags = static_cast<ImGuiSliderFlags>(ImGuiSliderFlags_Vertical) |
                                            ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoSpeedTweaks;

    // manually inlining speed logic so we can make shift the "go slow" option
    ImGuiContext& g = *ImGui::GetCurrentContext();
    float v_speed = (float)((mMax - mMin) * g.DragSpeedDefaultRatio) * 0.25f;
    if (g.IO.KeyShift) {
        v_speed *= 0.1f;
    }

    auto value_changed = ImGui::DragBehavior(gid, ImGuiDataType_Double, &rawValueInOut, v_speed, &mMin, &mMax, "%.2f",
                                             drag_behaviour_flags);

    auto is_active = ImGui::IsItemActive();
    auto is_hovered = ImGui::IsItemHovered();

    // responses
    if (is_active || ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        // tooltip counts as a window! so this works
        float view_width = ImGui::GetMainViewport()->Size.x;
        bool pivot_left = screen_pos.x > view_width * 0.70f;
        if (pivot_left) {
            ImGui::SetNextWindowPos({screen_pos.x, screen_pos.y + (itemWidth * 0.5f)}, ImGuiCond_Always, {1.0, 0.0});
        } else {
            ImGui::SetNextWindowPos({screen_pos.x + itemWidth, screen_pos.y + (itemWidth * 0.5f)}, ImGuiCond_Always,
                                    {0.0, 0.0});
        }
        ImGui::SetNextWindowSize({0.0f, 0.0f}, ImGuiCond_Always);
        ImGui::SetTooltip("%s: %s", GetName().c_str(), ToValueText(rawValueInOut).c_str());
    }

    if (is_active && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        rawValueInOut = GetDefault();
        value_changed = true;
    }

    static std::string sValueText{};
    static bool sPopupOpened{};
    if (is_hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("editknob");
        sValueText = ToValueText(rawValueInOut);
        sPopupOpened = true;
    }

    ImGui::SetNextWindowSize({160.0f * scale, 0.0f});
    if (ImGui::BeginPopup("editknob")) {
        if (ImGui::InputText("##", &sValueText, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (FromValueText(sValueText, rawValueInOut)) {
                // TODO validation
                value_changed = true;
            }
            ImGui::CloseCurrentPopup();
        }
        if (sPopupOpened) {
            // doesn't seem to work :/
            // ImGui::SetKeyboardFocusHere(-1);
            sPopupOpened = false;
        }
        ImGui::EndPopup();
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
